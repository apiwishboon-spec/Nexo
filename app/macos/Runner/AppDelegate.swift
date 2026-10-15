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

  /// Spawns a detached helper that waits for Nexo to quit, unzips the download,
  /// replaces the running bundle and relaunches it. Returns false (so the Dart
  /// side can fall back to revealing the download) when it cannot run — e.g. the
  /// app is running from a Gatekeeper translocation path.
  private func applyUpdate(zipPath: String, result: @escaping FlutterResult) {
    let appPath = Bundle.main.bundlePath
    if appPath.contains("/AppTranslocation/") {
      result(FlutterError(
        code: "translocated",
        message: "Nexo is running from a temporary macOS security path. "
          + "Move Nexo to /Applications, relaunch it, then update again.",
        details: nil
      ))
      return
    }
    // Also refuse when the bundle lives somewhere read-only (e.g. the user
    // launched it straight from the .dmg or a Downloads folder without moving
    // it to /Applications): swapping it would fail silently in the helper.
    if !FileManager.default.isWritableFile(atPath: appPath) {
      result(FlutterError(
        code: "read_only",
        message: "Nexo is running from a read-only location. "
          + "Move it to /Applications, relaunch, then update again.",
        details: nil
      ))
      return
    }

    let script = """
    #!/bin/bash
    ZIP="$1"; APP="$2"; PID="$3"
    while kill -0 "$PID" 2>/dev/null; do sleep 0.4; done
    TMP="$(/usr/bin/mktemp -d)"
    /usr/bin/ditto -x -k "$ZIP" "$TMP" >/dev/null 2>&1
    NEW="$(/usr/bin/find "$TMP" -maxdepth 2 -name '*.app' -print -quit)"
    if [ -z "$NEW" ]; then /bin/rm -rf "$TMP"; exit 1; fi
    if ! /bin/mv "$APP" "$APP.old" 2>/dev/null; then /bin/rm -rf "$APP"; fi
    /usr/bin/ditto "$NEW" "$APP"
    /bin/rm -rf "$APP.old" "$TMP" "$ZIP"
    /usr/bin/xattr -dr com.apple.quarantine "$APP" 2>/dev/null
    /usr/bin/open "$APP"
    """

    let process = Process()
    process.executableURL = URL(fileURLWithPath: "/bin/bash")
    process.arguments = [
      "-c", script, "nexo-updater", zipPath, appPath,
      String(ProcessInfo.processInfo.processIdentifier),
    ]
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
}
