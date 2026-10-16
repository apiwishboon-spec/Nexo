import Cocoa
import FlutterMacOS
import Speech

@main
class AppDelegate: FlutterAppDelegate {
  private let updaterChannelName = "com.nexo.nexo_app/updater"
  private let speechChannelName = "com.nexo.nexo_app/speech"

  override func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
    return true
  }

  override func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool {
    return true
  }

  override func applicationDidFinishLaunching(_ notification: Notification) {
    super.applicationDidFinishLaunching(notification)

    guard
      let window = NSApplication.shared.windows.first,
      let controller = window.contentViewController as? FlutterViewController
    else {
      return
    }

    let channel = FlutterMethodChannel(
      name: updaterChannelName,
      binaryMessenger: controller.engine.binaryMessenger
    )
    channel.setMethodCallHandler { [weak self] call, result in
      switch call.method {
      case "applyUpdate":
        guard let args = call.arguments as? [String: Any],
          let path = args["path"] as? String
        else {
          result(FlutterError(code: "bad_args", message: "Missing path", details: nil))
          return
        }
        self?.applyUpdate(zipPath: path, result: result)
      case "needsRelocation":
        result(self?.needsRelocation() ?? false)
      case "relocate":
        self?.relocate(result: result)
      case "reveal":
        guard let args = call.arguments as? [String: Any],
          let path = args["path"] as? String
        else {
          result(FlutterError(code: "bad_args", message: "Missing path", details: nil))
          return
        }
        NSWorkspace.shared.activateFileViewerSelecting([URL(fileURLWithPath: path)])
        result(nil)
      default:
        result(FlutterMethodNotImplemented)
      }
    }

    let speech = FlutterMethodChannel(
      name: speechChannelName,
      binaryMessenger: controller.engine.binaryMessenger
    )
    speech.setMethodCallHandler { [weak self] call, result in
      switch call.method {
      case "transcribeFile":
        guard let args = call.arguments as? [String: Any],
          let path = args["path"] as? String
        else {
          result(FlutterError(code: "bad_args", message: "Missing path", details: nil))
          return
        }
        self?.transcribe(path: path, result: result)
      default:
        result(FlutterMethodNotImplemented)
      }
    }
  }

  // MARK: - Speech (voice-memo transcription)

  /// Transcribes a 16 kHz mono WAV voice memo using Apple's on-device / server
  /// speech recognition. SFSpeechURLRecognitionRequest only handles ~1 minute
  /// per call, so the WAV is split into short chunks that are recognised in
  /// order and stitched back together.
  private func transcribe(path: String, result: @escaping FlutterResult) {
    guard let data = FileManager.default.contents(atPath: path) else {
      result(FlutterError(code: "not_found", message: "No audio file", details: nil))
      return
    }
    let headerSize = 44
    guard data.count > headerSize else {
      result("")
      return
    }
    let header = data.subdata(in: 0..<headerSize)
    let body = data.subdata(in: headerSize..<data.count)
    let sampleRate = Int(readU32(header, 24))
    let channels = max(1, Int(readU16(header, 22)))
    let bits = max(8, Int(readU16(header, 34)))
    let frameBytes = max(1, channels * (bits / 8))
    let rate = sampleRate > 0 ? sampleRate : 16000
    let chunkBytes = max(frameBytes, rate * frameBytes * 45)  // ~45 s of audio

    SFSpeechRecognizer.requestAuthorization { status in
      guard status == .authorized else {
        DispatchQueue.main.async {
          result(FlutterError(
            code: "denied",
            message: "Speech recognition is not authorized (System Settings ▸ Privacy).",
            details: nil))
        }
        return
      }
      guard let recognizer = SFSpeechRecognizer(), recognizer.isAvailable else {
        DispatchQueue.main.async {
          result(FlutterError(
            code: "unavailable",
            message: "Speech recognition is unavailable right now.",
            details: nil))
        }
        return
      }

      let tmp = FileManager.default.temporaryDirectory
        .appendingPathComponent("nexo-speech-\(UUID().uuidString)")
      try? FileManager.default.createDirectory(at: tmp, withIntermediateDirectories: true)
      var urls: [URL] = []
      var offset = 0
      var n = 0
      while offset < body.count {
        let end = min(offset + chunkBytes, body.count)
        let chunk = body.subdata(in: offset..<end)
        let url = tmp.appendingPathComponent("chunk-\(n).wav")
        if self.writeWavChunk(header: header, body: chunk, to: url) {
          urls.append(url)
        }
        offset = end
        n += 1
      }
      guard !urls.isEmpty else {
        DispatchQueue.main.async { result("") }
        return
      }
      self.recognize(
        urls: urls, index: 0, acc: "", recognizer: recognizer, tmp: tmp, result: result)
    }
  }

  private func recognize(
    urls: [URL], index: Int, acc: String, recognizer: SFSpeechRecognizer,
    tmp: URL, result: @escaping FlutterResult
  ) {
    if index >= urls.count {
      try? FileManager.default.removeItem(at: tmp)
      let text = acc.trimmingCharacters(in: .whitespacesAndNewlines)
      DispatchQueue.main.async { result(text) }
      return
    }
    let request = SFSpeechURLRecognitionRequest(url: urls[index])
    request.shouldReportPartialResults = false
    recognizer.recognitionTask(with: request) { res, error in
      if let res = res, res.isFinal {
        let part = res.bestTranscription.formattedString
        let merged = acc.isEmpty ? part : acc + " " + part
        self.recognize(
          urls: urls, index: index + 1, acc: merged, recognizer: recognizer, tmp: tmp, result: result)
      } else if let error = error {
        try? FileManager.default.removeItem(at: tmp)
        if acc.isEmpty {
          DispatchQueue.main.async {
            result(FlutterError(
              code: "failed", message: error.localizedDescription, details: nil))
          }
        } else {
          DispatchQueue.main.async { result(acc) }
        }
      }
    }
  }

  private func readU32(_ d: Data, _ o: Int) -> UInt32 {
    return UInt32(d[o]) | (UInt32(d[o + 1]) << 8) | (UInt32(d[o + 2]) << 16)
      | (UInt32(d[o + 3]) << 24)
  }

  private func readU16(_ d: Data, _ o: Int) -> UInt16 {
    return UInt16(d[o]) | (UInt16(d[o + 1]) << 8)
  }

  private func writeWavChunk(header: Data, body: Data, to url: URL) -> Bool {
    var h = header
    let dataLen = UInt32(body.count)
    let riffLen = UInt32(36) + dataLen
    h.replaceSubrange(4..<8, with: withUnsafeBytes(of: riffLen.littleEndian) { Data($0) })
    h.replaceSubrange(40..<44, with: withUnsafeBytes(of: dataLen.littleEndian) { Data($0) })
    var out = h
    out.append(body)
    return (try? out.write(to: url)) != nil
  }

  // MARK: - Self-update

  private var runningAppPath: String { Bundle.main.bundlePath }

  /// True when the running bundle cannot be replaced in place — i.e. macOS
  /// Gatekeeper translocation (launched straight from a quarantined folder/DMG)
  /// or a genuinely read-only location.
  private func needsRelocation() -> Bool {
    runningAppPath.contains("/AppTranslocation/")
      || !FileManager.default.isWritableFile(atPath: runningAppPath)
  }

  /// Where a downloaded build should be installed. Update in place when we can,
  /// otherwise fall back to /Applications, then ~/Applications.
  private func installTarget() -> String {
    let fm = FileManager.default
    if !needsRelocation() {
      return runningAppPath
    }
    if fm.isWritableFile(atPath: "/Applications") {
      return "/Applications/Nexo.app"
    }
    let home = NSHomeDirectory()
    try? fm.createDirectory(
      atPath: "\(home)/Applications", withIntermediateDirectories: true)
    return "\(home)/Applications/Nexo.app"
  }

  private func logURL() -> URL {
    let fm = FileManager.default
    let logs = fm.homeDirectoryForCurrentUser.appendingPathComponent("Library/Logs/Nexo")
    try? fm.createDirectory(at: logs, withIntermediateDirectories: true)
    return logs.appendingPathComponent("updater.log")
  }

  /// Downloads are installed on top of the existing bundle; when the running
  /// copy is translocated/read-only the new build is placed in /Applications so
  /// the next launch is a normal, updatable install.
  private func applyUpdate(zipPath: String, result: @escaping FlutterResult) {
    launchHelper(script: updateScript, args: [zipPath, installTarget()], result: result)
  }

  /// Moves the currently running bundle to /Applications and relaunches it.
  /// Used when there is no update to install but the app is running from a
  /// translocated path and would otherwise never be able to self-update.
  private func relocate(result: @escaping FlutterResult) {
    let target = installTarget()
    if target == runningAppPath {
      result(false)
      return
    }
    launchHelper(script: relocateScript, args: [runningAppPath, target], result: result)
  }

  private func launchHelper(
    script: String, args: [String], result: @escaping FlutterResult
  ) {
    let process = Process()
    process.executableURL = URL(fileURLWithPath: "/bin/bash")
    process.arguments =
      ["-c", script, "nexo-updater"] + args
      + [String(ProcessInfo.processInfo.processIdentifier), logURL().path]
    process.standardOutput = FileHandle.nullDevice
    process.standardError = FileHandle.nullDevice

    do {
      try process.run()
    } catch {
      result(false)
      return
    }

    result(true)
    DispatchQueue.main.asyncAfter(deadline: .now() + 0.6) {
      NSApp.terminate(nil)
    }
  }

  private let updateScript = """
  #!/bin/bash
  SRC_ZIP="$1"; APP="$2"; PID="$3"; LOG="$4"
  exec >>"$LOG" 2>&1
  echo "== [updater] $(date) install zip=$SRC_ZIP -> $APP"
  while kill -0 "$PID" 2>/dev/null; do sleep 0.4; done
  TMP="$(/usr/bin/mktemp -d)"
  if ! /usr/bin/ditto -x -k "$SRC_ZIP" "$TMP"; then
    echo "[updater] extract failed"; /bin/rm -rf "$TMP"; exit 1
  fi
  NEW="$(/usr/bin/find "$TMP" -maxdepth 2 -name '*.app' -print -quit)"
  if [ -z "$NEW" ]; then
    echo "[updater] no .app in archive"; /bin/rm -rf "$TMP"; exit 1
  fi
  if [ -e "$APP" ]; then
    /bin/rm -rf "$APP.old"
    /bin/mv "$APP" "$APP.old" 2>/dev/null || /bin/rm -rf "$APP"
  fi
  if ! /usr/bin/ditto "$NEW" "$APP"; then
    echo "[updater] copy failed"; /bin/rm -rf "$TMP"; exit 1
  fi
  /bin/rm -rf "$APP.old" "$TMP" "$SRC_ZIP"
  /usr/bin/xattr -dr com.apple.quarantine "$APP" 2>/dev/null
  echo "[updater] launching $APP"
  /usr/bin/open "$APP"
  echo "[updater] done"
  """

  private let relocateScript = """
  #!/bin/bash
  SRC_APP="$1"; APP="$2"; PID="$3"; LOG="$4"
  exec >>"$LOG" 2>&1
  echo "== [updater] $(date) relocate $SRC_APP -> $APP"
  while kill -0 "$PID" 2>/dev/null; do sleep 0.4; done
  if [ -e "$APP" ]; then
    /bin/rm -rf "$APP.old"
    /bin/mv "$APP" "$APP.old" 2>/dev/null || /bin/rm -rf "$APP"
  fi
  if ! /usr/bin/ditto "$SRC_APP" "$APP"; then
    echo "[updater] copy failed"; exit 1
  fi
  /bin/rm -rf "$APP.old"
  /usr/bin/xattr -dr com.apple.quarantine "$APP" 2>/dev/null
  echo "[updater] launching $APP"
  /usr/bin/open "$APP"
  echo "[updater] done"
  """
}
