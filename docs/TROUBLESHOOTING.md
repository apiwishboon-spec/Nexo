# Troubleshooting

## Firmware

### `std::make_unique is not a member of 'std'`
The Arduino framework injects `-std=gnu++11`. `platformio.ini` must both remove
it and set C++17:

```ini
build_unflags = -std=gnu++11
build_flags = -std=gnu++17
```

### `pio test -e native` fails to link (`undefined reference to main`)
PlatformIO does **not** generate `main()` for the native environment. The test
translation unit must define its own `int main()` that runs the tests.

### NimBLE-Arduino 1.4.x API

The bundled NimBLE-Arduino 1.4.3 has **no** `NimBLEServer::getConnId()` or
`getPeerAddress()`. Use the two-argument callbacks instead:

```cpp
void onConnect(NimBLEServer* server, ble_gap_conn_desc* desc);
void onDisconnect(NimBLEServer* server, ble_gap_conn_desc* desc);
// conn id: desc->conn_handle ; peer: NimBLEAddress(desc->peer_id_addr).toString()
```

Also `esp_read_mac()` needs `#include <esp_mac.h>`.

### The firmware is an app image, not a full flash

`firmware.bin` is built to load at `0x10000` for Launcher. Do not flash it as if
it were a bootloader/partition-table factory image unless you know your
partition layout matches `firmware/partitions.csv`.

### Random reboots / dropped connections during sync

NimBLE invokes the GATT callbacks (`onWrite`/`onConnect`/`onDisconnect`) on the
**Bluetooth host task**. Doing repository access, LittleFS writes or
`char->notify()` from there can panic the flash cache or corrupt shared state and
reset the device. The firmware therefore only copies incoming writes into a small
lock-protected queue inside `onWrite`, and performs reassembly, service handling,
persistence and notifications from `BleService::loop()` on the Arduino task.

When extending the firmware: never touch `repo()`, `persist*()` or a
characteristic's `notify()` from a NimBLE callback — hand the work to `loop()`.

### Screen flashes during animations (Home character, memo recording)

**Symptom:** the whole display blanks/repaints about twice a second while the
Home character animates, or about three times a second while recording a memo.

**Cause:** older firmware called `Ui::draw()`, which begins with
`fillScreen()`, on every animation frame. Throttling the frame rate (v2.2.4)
made it less frequent but never removed the full clear.

**Fixed in v2.3.3:** animation frames now use incremental repaints —
`drawHomeCharacterFrame()` clears only the one-pixel strip exposed by the bob,
and `drawMemoLive()` repaints only the REC dot / elapsed time or the playing
row. The host test `firmware/tools/preview/build_render_test.sh` asserts that
these paths perform zero full-screen clears; keep it green when changing the UI.

If you still see flashing on v2.3.3+, check that you flashed the matching
firmware (`Settings → About` shows the version) and not an older app image.

### Battery level

The header battery indicator reads the Cardputer's GPIO10 ADC through
`M5Cardputer.Power.getBatteryLevel()` / `isCharging()` (refreshed every ~5 s).

## Companion app

### `pod not found` / CocoaPods errors (macOS)

```bash
brew install cocoapods
```

Then rebuild. The macOS target needs the Bluetooth entitlement in
`DebugProfile.entitlements` and `Release.entitlements`.

### Android: “Could not determine java version” / Gradle failures

The system JDK may be too new. Point Flutter at a supported JDK (17 or 21):

```bash
flutter config --jdk-dir="/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home"
```

### Android: “Some Android licenses not accepted”

```bash
flutter doctor --android-licenses
```

### Device not found when scanning

- The Cardputer only advertises the Nexo service while the firmware is running.
  Launch the Nexo app on the device first.
- The app filters scans by the Nexo service UUID. If the device is not
  advertising that service it will not appear.
- On macOS, grant Bluetooth permission when prompted in
  *System Settings → Privacy & Security → Bluetooth*.
- On Android 12+, grant the *Nearby devices* permission when prompted.

### Connect succeeds but requests time out

- The device accepts a single BLE client at a time; it stops advertising while
  connected and rejects extra links. Close Nexo on other phones/computers (and
  any stale instance on this machine) before connecting.
- PIN mismatch returns `UNAUTHORIZED (0x04)`; repeated failures trigger the
  device's auth lock (`LOCKED 0x0A`) for a short cool-down.
- The firmware's default PIN is **123456** and the app pre-fills it for a new
  device. If you changed the PIN in the app's Settings tab, the app remembers
  the new value; if authentication still fails, disconnect, scan again, and
  enter the current PIN. Factory resetting the device restores `123456`.
- If you see `Unsupported protocol version`, the app and firmware protocol
  versions differ. Update both (see `docs/PROTOCOL.md`).

### Sync conflicts

The device owns the authoritative revision. When the app pushes a record whose
`baseRev` is stale, the device replies with the current record and the app adopts
the device version. Unsent local edits are preserved until a successful push.

## General

- **No hardware was tested during development.** All firmware results are from
  compilation and host unit tests; verify on a real Cardputer before relying on it.
- BLE throughput is modest. Large note bodies are chunked (8-byte header per
  chunk) and the device negotiates up to a 244-byte payload; very large syncs may
  take several seconds.
