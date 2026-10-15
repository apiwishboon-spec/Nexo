# Building Nexo

This guide covers building the Cardputer firmware and the Flutter companion app.

## Prerequisites

- **Firmware:** PlatformIO Core (`pio`) on Python ≥ 3.8, with the Espressif32
  platform. On this machine it lives at `~/Library/Python/3.13/bin/pio`.
- **App:** Flutter 3.47+ (Dart 3.13+), Xcode (macOS build), Android SDK + JDK 17/21.
- **macOS app:** CocoaPods (`brew install cocoapods`).
- **Android app:** an accepted SDK license set (`flutter doctor --android-licenses`).

## Firmware

The project targets an ESP32-S3 (Cardputer / Stamp-S3A, 8 MB flash). The
Launcher-compatible app image is produced at offset `0x10000`.

```bash
cd firmware
export PATH="$HOME/Library/Python/3.13/bin:$PATH"

# Build the application image.
pio run -e m5stack-cardputer
# Output: .pio/build/m5stack-cardputer/firmware.bin

# Run the portable core unit tests on the host.
pio test -e native
```

### Installing onto the Cardputer

Two options:

1. **Via Launcher:** copy `firmware.bin` to the SD card / Launcher `apps`
   directory. Launcher chain-loads it as a standalone app binary; it is built
   for an app base offset of `0x10000` (`board_upload.offset_address`).
2. **Raw flash (development only):** with a Cardputer on USB,
   `pio run -e m5stack-cardputer -t upload`. This overwrites the flash layout,
   so re-flash a Launcher build afterwards if you use Launcher.

Never flash a device you cannot recover without a backup of its current firmware.

### Key build settings (`platformio.ini`)

- `board = esp32-s3-devkitc-1` (ESP32-S3FN8, 8 MB flash).
- `board_upload.offset_address = 0x10000` and `partitions.csv` place `app0` at
  `0x10000` with a `spiffs` (LittleFS) data partition after it.
- `build_unflags = -std=gnu++11` + `-std=gnu++17` — the Arduino core sets
  `gnu++11`; the code uses `std::make_unique`, so C++17 is required.

## Companion app

```bash
cd app
flutter pub get
flutter test                 # protocol/model unit tests
flutter analyze

# macOS
flutter build macos --release
# Output: build/macos/Build/Products/Release/Nexo.app

# Android
flutter build apk --release
# Output: build/app/outputs/flutter-apk/app-release.apk
```

### Platform configuration

- **Android** (`android/app/src/main/AndroidManifest.xml`): declares
  `BLUETOOTH_SCAN`/`BLUETOOTH_CONNECT` (Android 12+) and legacy
  `BLUETOOTH`/`BLUETOOTH_ADMIN`/`ACCESS_FINE_LOCATION`, plus the BLE feature.
  `REQUEST_INSTALL_PACKAGES` and a `FileProvider` (`res/xml/file_paths.xml`) let
  the app hand a downloaded update to the system installer.
- **macOS** (`macos/Runner/*.entitlements`): the **App Sandbox is disabled** so
  the app can replace its own bundle in `/Applications` during a self-update.
  Bluetooth works via `NSBluetoothAlwaysUsageDescription` in `Info.plist`.

### In-app updates

Both apps check GitHub Releases (`apiwishboon-spec/Nexo`) on launch and via
**About → App updates**. When a newer release exists:

- **Android** downloads `Nexo-android-release.apk` and launches the package
  installer (the first time, allow "Install unknown apps" for Nexo).
- **macOS** downloads `Nexo-macos-release.zip`, then a detached helper waits for
  the app to quit, swaps `/Applications/Nexo.app` and relaunches it. If the app
  is running from a read-only/translocated path the swap is skipped and the
  download is revealed in Finder instead.

To ship a release the updater will see: bump `version:` in `app/pubspec.yaml`
and `kNexoVersion` in the firmware, then push a `vX.Y.Z` tag. The
[`release` workflow](../.github/workflows/release.yml) builds the firmware, the
Android APK and the macOS `.app`, generates `SHA256SUMS`, and attaches them to a
GitHub release (using `docs/releases/X.Y.Z.md`, without the leading `v`, as the
body when present).

## UI preview & web emulator

The firmware UI also runs on the host with no hardware. All of these compile the
real `src/ui/ui.cpp` against a fake M5Cardputer display:

```bash
# One-off PNG renders of every screen (needs clang++ + Pillow).
bash firmware/tools/preview/build_preview.sh

# Interactive Tk emulator (needs python3 with tkinter).
bash firmware/tools/preview/emu.sh

# WebAssembly build for the website (needs the Emscripten SDK on PATH).
bash firmware/tools/preview/build_wasm.sh    # -> website/assets/emu/
```

The website loads the WASM build on `emulator.html`; the `emulator` workflow
rebuilds and commits it whenever the shared UI changes.

## Continuous integration & releases

Cheap, slow work (PlatformIO builds, Flutter analyze/test, release builds) runs
on GitHub Actions so it does not tie up a local machine.

- **`ci.yml`** — on pushes to `main` and pull requests: firmware host unit tests
  + firmware build, and `flutter analyze` + `flutter test`.
- **`release.yml`** — on a `v*` tag (or manual dispatch): builds all three
  artifacts and publishes a GitHub Release. The `FLUTTER_VERSION` env in both
  workflows pins the SDK version.
- **`build.yml`** — on pushes to `main` that touch `app/**` or `firmware/**`:
  detects which component changed, rebuilds only that one, and publishes the
  fresh binaries to a rolling `continuous` prerelease with an auto-generated
  description (changed components + commit log). Stable builds still come from
  `release.yml`.
- **`emulator.yml`** — on UI changes: rebuilds the WASM emulator and commits it
  into `website/assets/emu/`.

### Android release signing (important)

By default the app signs releases with the **debug** keystore, which is unique per
machine. A CI-built APK signed with a different debug key **cannot update a
previously installed build in place** (Android rejects the signature mismatch).

For consistent, updatable releases, create a keystore once and store it as
repository secrets:

```bash
keytool -genkey -v -keystore release.keystore -alias nexo \
  -keyalg RSA -keysize 2048 -validity 10000

base64 -i release.keystore | gh secret set ANDROID_KEYSTORE_BASE64

cat > key.properties <<'EOF'
storePassword=<your-store-password>
keyPassword=<your-key-password>
keyAlias=nexo
storeFile=release.keystore
EOF
gh secret set ANDROID_KEY_PROPERTIES < key.properties

rm release.keystore key.properties   # keep a backup somewhere safe!
```

`android/app/build.gradle.kts` uses `android/key.properties` when present and
falls back to the debug keystore otherwise. **Back up the keystore**: losing it
means future builds can no longer update installed apps.

### Dependency notes

- `flutter_blue_plus: ^1.35.0` (resolved to 1.36.x) — native BLE through
  CoreBluetooth on Apple platforms and the Android BLE stack.
- `shared_preferences` — local offline cache.
- `provider` — state management.
- `package_info_plus` — reads the running app version.
- `path_provider` — temp directory for downloaded updates.
- `url_launcher` — opens the release page as a fallback.
- `pub_semver` — compares app/release versions.
- `file_selector` — picks audio files to upload to the Cardputer.
