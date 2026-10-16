import Cocoa
import FlutterMacOS

@main
class AppDelegate: FlutterAppDelegate {
  private let updaterChannelName = "com.nexo.nexo_app/updater"

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
