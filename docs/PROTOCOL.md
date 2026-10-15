# Nexo BLE Protocol v2

This document is the single source of truth for the wire protocol shared by the
Cardputer firmware (`firmware/`) and the companion app (`app/`).

The protocol is a small, versioned, request/response RPC carried over a single
BLE GATT service. Binary frames are chunked to respect the negotiated ATT MTU.
All integers are **little-endian**.

> **v2 is a clean break.** The chunk `version` byte is `0x02`; a device receiving
> any other version answers once with `ERR_VERSION`. v2 keeps the v1 opcode
> numbers (`0x01`..`0x42`) so the opcode table is easy to reason about, and adds
> device-management (`0x03`..`0x07`) and media (`0x50`..`0x53`) opcodes. Bulk
> media transfer and firmware updates do **not** use BLE — they use the device's
> Wi-Fi HTTP server (§9).

---

## 1. GATT profile

The Cardputer runs a BLE peripheral (GATT server). The phone/mac runs a BLE
central (GATT client). Advertising is connectable and includes the service UUID
and a local name `Nexo-XXXX` (XXXX = last two bytes of the device MAC).

| Role            | UUID                                     | Properties                |
| --------------- | ---------------------------------------- | ------------------------- |
| Nexo Service    | `6e65786f-0001-4e58-8000-4e65786f0000`   | primary                   |
| RX (central→dev)| `6e65786f-0002-4e58-8000-4e65786f0000`   | Write, Write Without Resp |
| TX (dev→central)| `6e65786f-0003-4e58-8000-4e65786f0000`   | Notify, Indicate          |
| Status          | `6e65786f-0004-4e58-8000-4e65786f0000`   | Read, Notify              |

TX is **indicated** (acknowledged) for reliable delivery. If the central does not
confirm, the device retransmits up to `NEXO_INDICATE_RETRIES` times.

Multiple centrals may connect at once (up to the NimBLE build limit). Responses
are broadcast on the TX characteristic and correlated by the message id.

### Status characteristic (20 bytes, read + notify)

| Offset | Size | Field                                              |
| ------ | ---- | -------------------------------------------------- |
| 0      | 1    | protocol version (`0x02`)                          |
| 1      | 1    | authenticated (0 = locked, 1 = unlocked)           |
| 2      | 1    | connected                                          |
| 3      | 1    | reserved                                           |
| 4      | 4    | note count                                         |
| 8      | 4    | homework count                                     |
| 12     | 4    | uptime seconds                                     |
| 16     | 4    | data revision (monotonic; bumped on any data change) |

The **data revision** is incremented by the device whenever notes, homework or
settings change — whether the edit originated on the device UI or from a client
over BLE. Clients watch it to trigger an automatic sync when another writer has
changed the data set.

---

## 2. Transport framing / chunking

A **message** may be larger than the ATT MTU. Each BLE write/notify carries a
**chunk**. A chunk starts with an 8-byte header, followed by up to
`mtuPayload - 8` bytes of message data.

Chunk header:

| Offset | Size | Field                          |
| ------ | ---- | ------------------------------ |
| 0      | 1    | protocol version (`0x02`)      |
| 1      | 1    | message type (opcode)          |
| 2      | 2    | message id (correlation id)    |
| 4      | 2    | offset of this chunk in message|
| 6      | 2    | total message length           |

Rules:
- `mtuPayload` is `negotiated_mtu - 3`, capped at `NEXO_MTU_CAP` (default 244).
- The receiver appends each chunk at `offset`. When it has assembled
  `total length` bytes for a given `(type, message id)` it emits the message.
- Out-of-order and duplicate chunks are tolerated (offset-addressed).
- Messages larger than `NEXO_MAX_MESSAGE` (default 4096 bytes) are rejected with
  `ERR_TOO_LARGE`.

The **payload** of a message is opcode-specific.

---

## 3. Opcodes

A request opcode and its response opcode differ by the high bit
(`RESPONSE = REQUEST | 0x80`).

| Opcode | Name             | Request payload                    | Response payload |
| ------ | ---------------- | ---------------------------------- | ---------------- |
| `0x01` | `PING`           | *empty*                            | `u32 device_time` |
| `0x02` | `GET_INFO`       | *empty*                            | Info TLV |
| `0x03` | `GET_DEVICE_INFO`| *empty*                            | DeviceInfo (see §5) |
| `0x04` | `SET_TIME`       | `u32 epoch, i16 tzOffsetMin`       | *empty* |
| `0x05` | `REBOOT`         | *empty*                            | *empty* |
| `0x06` | `FACTORY_RESET`  | *empty*                            | *empty* |
| `0x07` | `FIND_DEVICE`    | *empty*                            | *empty* |
| `0x0F` | `AUTH`           | `u8 pinLen, pin bytes`             | *empty* |
| `0x10` | `LIST_NOTES`     | `u16 offset, u16 limit`            | `NoteBatch` |
| `0x11` | `GET_NOTE`       | `16-byte id`                       | `Note` |
| `0x12` | `PUT_NOTE`       | `Note` (with `baseRev`)            | `Note` (stored) |
| `0x13` | `DELETE_NOTE`    | `16-byte id, u32 baseRev`          | `16-byte id` |
| `0x20` | `LIST_HOMEWORK`  | `u16 offset, u16 limit`            | `HomeworkBatch` |
| `0x21` | `GET_HOMEWORK`   | `16-byte id`                       | `Homework` |
| `0x22` | `PUT_HOMEWORK`   | `Homework` (with `baseRev`)        | `Homework` (stored) |
| `0x23` | `DELETE_HOMEWORK`| `16-byte id, u32 baseRev`          | `16-byte id` |
| `0x30` | `GET_SETTINGS`   | *empty*                            | `Settings` |
| `0x31` | `PUT_SETTINGS`   | `Settings`                         | `Settings` (stored) |
| `0x40` | `SYNC_PULL`      | `u32 sinceEpoch, u8 cursor`        | `SyncDelta` |
| `0x41` | `SYNC_PUSH`      | `SyncPush`                         | `SyncResult` |
| `0x42` | `SYNC_SUMMARY`   | *empty*                            | `u32 noteCount, u32 hwCount, u32 maxUpdated` |
| `0x50` | `LIST_MUSIC`     | *empty*                            | `MusicBatch` (§8) |
| `0x51` | `MUSIC_CONTROL`  | `u8 action, i16 index, u16 arg`    | `MusicStatus` |
| `0x52` | `NOW_PLAYING`    | *empty*                            | `MusicStatus` |
| `0x53` | `DELETE_MUSIC`   | `u16 name (str16)`                 | *empty* |

Every response begins with a **status byte** (§7). On non-`OK` status the rest of
the payload is an optional human-readable UTF-8 error string.

**Auth allow-list:** only `PING`, `GET_INFO` and `AUTH` work while locked.
Everything else (including `GET_DEVICE_INFO`, `SET_TIME`, device actions and
media ops) requires a successful `AUTH`. `GET_DEVICE_INFO` includes the AP
password, so it is intentionally authenticated.

### Music actions (`MUSIC_CONTROL.action`)

| Value | Name              | `index`         | `arg`           |
| ----- | ----------------- | --------------- | --------------- |
| 1     | `TOGGLE`          | ignored         | ignored         |
| 2     | `PLAY_INDEX`      | track index     | ignored         |
| 3     | `NEXT`            | ignored         | ignored         |
| 4     | `PREV`            | ignored         | ignored         |
| 5     | `STOP`            | ignored         | ignored         |
| 6     | `SEEK`            | ignored         | seconds         |
| 7     | `SET_VOLUME`      | ignored         | 0..100          |

---

## 4. Record encodings

Records use a compact tag scheme. Each field is `tag(1 byte)` + value. Tags
`0x00`..`0x0F` are fixed width, `0x10`..`0x1F` are length-prefixed (`u16` length +
bytes), `0x20`..`0x2F` are length-prefixed (`u8` length + bytes), `0x80`
terminates a record.

### ID (16 bytes)
Stable identifier: 16 lowercase hex characters (8 random bytes). Generated on
whichever side creates the record. Never changes.

### Note
| Tag  | Name       | Type / width        |
| ---- | ---------- | ------------------- |
| 0x01 | baseRev    | u32                 |
| 0x02 | rev        | u32                 |
| 0x03 | created    | u32 (epoch seconds) |
| 0x04 | updated    | u32 (epoch seconds) |
| 0x05 | flags      | u8 (bit0 = deleted) |
| 0x10 | id         | bytes (16)          |
| 0x11 | title      | u16 + UTF-8         |
| 0x12 | body       | u16 + UTF-8         |

### Homework
| Tag  | Name        | Type / width        |
| ---- | ----------- | ------------------- |
| 0x01 | baseRev     | u32                 |
| 0x02 | rev         | u32                 |
| 0x03 | created     | u32                 |
| 0x04 | updated     | u32                 |
| 0x05 | flags       | u8 (bit0 = deleted) |
| 0x06 | status      | u8 (0..2)           |
| 0x07 | dueDate     | u32 (epoch day)     |
| 0x10 | id          | bytes (16)          |
| 0x11 | subject     | u16 + UTF-8         |
| 0x12 | description | u16 + UTF-8         |

`dueDate` is a Unix epoch **day** number (days since 1970-01-01, UTC), `0` = none.
The companion app converts to/from a local `YYYY-MM-DD` date.

### Settings
| Tag  | Name            | Type / width        |
| ---- | --------------- | ------------------- |
| 0x10 | deviceName      | u16 + UTF-8         |
| 0x11 | pin             | u16 + UTF-8 (write-only; empty on read) |
| 0x12 | apSsid          | u16 + UTF-8         |
| 0x13 | apPassword      | u16 + UTF-8 (write-only; accepted, unused) |
| 0x20 | apEnabled       | u8                  |
| 0x21 | volume          | u8 (0..100)         |
| 0x22 | soundEnabled    | u8                  |
| 0x23 | brightness      | u8 (0..100)         |
| 0x24 | screenTimeoutSec| u16 (0 = never)     |
| 0x25 | autoAdvanceMusic| u8                  |
| 0x26 | tzOffsetMin     | i16 (minutes east of UTC) |

`PUT_SETTINGS` applies live changes immediately (volume, sound, brightness) and
persists them. `pin` is only changed when a non-empty value is sent.

---

## 5. DeviceInfo (GET_DEVICE_INFO response)

Tags are local to the DeviceInfo record.

| Tag  | Name            | Type / width        |
| ---- | --------------- | ------------------- |
| 0x01 | protocolVersion | u8                  |
| 0x02 | batteryPercent  | u8 (0xFF = unknown) |
| 0x03 | charging        | u8                  |
| 0x04 | uptimeSec       | u32                 |
| 0x05 | freeHeapKiB     | u32                 |
| 0x06 | sdPresent       | u8                  |
| 0x07 | sdFreeMb        | u32                 |
| 0x08 | sdTotalMb       | u32                 |
| 0x09 | musicCount      | u16                 |
| 0x0A | volume          | u8                  |
| 0x0B | soundEnabled    | u8                  |
| 0x0C | headphoneIn     | u8                  |
| 0x0D | apEnabled       | u8                  |
| 0x0E | rssi            | i8                  |
| 0x0F | screenTimeoutSec| u16                 |
| 0x10 | deviceName      | u16 + UTF-8         |
| 0x11 | firmwareVersion | u16 + UTF-8         |
| 0x12 | mac             | u16 + UTF-8         |
| 0x13 | apSsid          | u16 + UTF-8         |
| 0x14 | apPassword      | u16 + UTF-8         |
| 0x15 | ip              | u16 + UTF-8         |
| 0x80 | end             | terminator          |

---

## 6. Batches and sync

- `NoteBatch` = `u16 count` + `Note`* + `u8 more` (1 = another page exists).
- `HomeworkBatch` = `u16 count` + `Homework`* + `u8 more`.
- `SyncDelta` = `u16 count` + (`u8 kind, record`)* + `u32 serverEpoch`.
  `kind` is `1` = note, `2` = homework. Tombstones (`deleted`) are included.
- `SyncPush` = `u16 count` + (`u8 kind, record`)* (records carry `baseRev`).
- `SyncResult` = `u16 applied, u16 conflicts` + `u32 serverEpoch` + for each
  conflict `u8 kind` + record (current server version).

---

## 7. Status codes

| Code | Name              | Meaning |
| ---- | ----------------- | ------- |
| 0x00 | `OK`              | success |
| 0x01 | `ERR_UNKNOWN`     | unhandled failure |
| 0x02 | `ERR_BAD_REQUEST` | malformed frame/payload |
| 0x03 | `ERR_NOT_FOUND`   | unknown id |
| 0x04 | `ERR_UNAUTHORIZED`| PIN required or wrong |
| 0x05 | `ERR_CONFLICT`    | `baseRev` != server `rev` |
| 0x06 | `ERR_TOO_LARGE`   | message exceeds limit |
| 0x07 | `ERR_STORAGE`     | persistence failure |
| 0x08 | `ERR_BUSY`        | retry later |
| 0x09 | `ERR_VERSION`     | unsupported protocol version |
| 0x0A | `ERR_LOCKED`      | auth throttle (too many attempts) |

---

## 8. Music

- `MusicBatch` = `u16 count` + (`MusicTrack`)*.
  - `MusicTrack` = `str16 name` (`0x10` tag) + `u32 size`.
- `MusicStatus` = `i16 index` (`-1` = none) + `u8 playing` + `u16 positionSec` +
  `u16 durationSec` + `u8 volume`.

BLE music ops are **metadata + control only**. Adding/removing files is done over
Wi-Fi HTTP (§9) because BLE throughput is far too low for multi-MB audio.

---

## 9. Wi-Fi HTTP (media transfer + OTA)

When the AP is enabled the device serves HTTP on `http://192.168.4.1/`. Auth uses
the same PIN; the server issues a session token cookie, or accepts
`X-Nexo-Token: <token>`.

| Method | Path                     | Purpose |
| ------ | ------------------------ | ------- |
| GET    | `/`                      | browser UI (notes/homework) |
| POST   | `/api/login`             | `{"pin":"..."}` → sets token cookie |
| GET    | `/api/info`              | counts + protocol version |
| GET    | `/api/music`             | list tracks `{items:[{name,size}]}` |
| POST   | `/api/music`             | multipart upload (field = file) → `/music/<name>` |
| GET    | `/api/music/file?name=..`| stream a track for download (audio/mpeg\|wav) |
| POST   | `/api/music/delete`      | `{name}` → delete a track |
| GET    | `/api/music/now`         | `{index,name,playing,position,duration,volume}` |
| POST   | `/api/music/control`     | `{action,index,arg}` where action ∈ toggle/play/next/prev/stop/seek/volume |
| POST   | `/api/ota`               | multipart upload of a firmware image → OTA, device reboots on success |

Auth (PIN) is required on every route except `/` and `/api/login`. Bulk audio
uses a streaming `multipart/form-data` upload so multi-MB files never buffer in
RAM; the body is written straight to the SD card and the library is rescanned on
completion. Unsafe filenames are reduced to a sanitised base name (`.mp3`/`.wav`
only).

The app turns the AP on automatically for transfers: it sets `apEnabled` via
`PUT_SETTINGS`, reads `apSsid` / `apPassword` / `ip` from `GET_DEVICE_INFO`, joins
the AP, performs the transfer, then restores the previous AP state.

`POST /api/ota` accepts a `multipart/form-data` upload whose file part is the raw
application image (the `firmware.bin` produced by PlatformIO). The stream is
written to the inactive OTA partition via `Update.h`; on success the device
replies `{"ok":true,"bytes":N}` and reboots. If the write fails the current
firmware keeps running and `{"error":"update failed"}` is returned.

---

## 10. Authentication

Auth is **application-level** (not BLE bonding) so it works identically on
Android, macOS and iOS without platform pairing prompts.

1. On connect the device reports `auth state = locked` via the Status
   characteristic.
2. Every request except `PING`, `GET_INFO` and `AUTH` returns `ERR_UNAUTHORIZED`
   while locked.
3. The central sends `AUTH` with the PIN (default `123456`, changeable in
   Settings). On success the device sets `auth state = unlocked` and notifies
   Status.
4. Five consecutive wrong PINs lock auth for 30 s (`ERR_LOCKED`).

Auth state is shared across all connected links and cleared when the last central
disconnects. The PIN is never returned over the read path.

---

## 11. Synchronization strategy

Stable IDs + per-record `rev` (monotonic) + `updated` (epoch seconds) provide
conflict detection.

- **Pull.** Client sends `SYNC_PULL(sinceEpoch)`. Device returns every record
  (including tombstones) with `updated >= sinceEpoch`, paged. Subsequent pages
  use the `cursor` byte. The client adopts any record newer than its local copy;
  if the local copy is dirty and the incoming `updated` is newer, the client
  flags a conflict for the user.
- **Push.** Client sends `SYNC_PUSH` with its dirty records, each carrying the
  `baseRev` it started from. For each record:
  - `baseRev == 0` → create. Device assigns an id if empty and sets `rev = 1`.
  - `baseRev == server.rev` → update. Device sets `rev = server.rev + 1`.
  - otherwise → **conflict**: the device does not overwrite and returns its
    current version for the client to resolve.
- **Duplicates.** `PUT_*` with the same id is idempotent for equal `baseRev`;
  the device tracks the last 64 request ids and replays the cached response for
  duplicate request ids (at-least-once transport protection).
- **Commit.** After a successful push the client adopts the returned server
  revisions and marks records clean. Sync is only reported as successful once
  the `SYNC_PUSH` response (`SyncResult`) has been received and applied.

The device has **no RTC**; the app pushes wall-clock time with `SET_TIME` on each
connect so `updated` timestamps and due dates stay meaningful.

Deletion uses tombstones so deletions propagate. Tombstones are retained for at
least 30 days and pruned opportunistically.

---

## 12. Versioning

- Frames carry `version` per chunk. A device receiving an unknown version
  responds once with `ERR_VERSION`.
- Protocol version is also exposed via `GET_INFO`, `GET_DEVICE_INFO` and the
  Status characteristic.
- v2 is a clean break from v1: bump the version byte and require matching
  firmware/app. New optional fields within a record use new tags so minor changes
  stay forward-compatible.
