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
- **macOS** (`macos/Runner/*.entitlements`): the app-sandbox Bluetooth entitlement
  `com.apple.security.device.bluetooth`. `Info.plist` includes
  `NSBluetoothAlwaysUsageDescription`.

### Dependency notes

- `flutter_blue_plus: ^1.35.0` (resolved to 1.36.x) — native BLE through
  CoreBluetooth on Apple platforms and the Android BLE stack.
- `shared_preferences` — local offline cache.
- `provider` — state management.
