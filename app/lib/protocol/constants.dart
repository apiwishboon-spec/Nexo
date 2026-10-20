// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
/// Nexo BLE protocol constants. Must stay in sync with docs/PROTOCOL.md and
/// firmware/lib/nexo_core/src/nexo_protocol.h.
library;

class NexoProtocol {
  static const int version = 2;

  // GATT UUIDs
  static const String serviceUuid = '6e65786f-0001-4e58-8000-4e65786f0000';
  static const String rxUuid = '6e65786f-0002-4e58-8000-4e65786f0000';
  static const String txUuid = '6e65786f-0003-4e58-8000-4e65786f0000';
  static const String statusUuid = '6e65786f-0004-4e58-8000-4e65786f0000';

  static const int chunkHeaderLen = 8;
  static const int maxMessage = 8192;

  // Opcodes
  static const int opPing = 0x01;
  static const int opGetInfo = 0x02;
  static const int opGetDeviceInfo = 0x03;
  static const int opSetTime = 0x04;
  static const int opReboot = 0x05;
  static const int opFactoryReset = 0x06;
  static const int opFindDevice = 0x07;
  static const int opAuth = 0x0F;
  static const int opListNotes = 0x10;
  static const int opGetNote = 0x11;
  static const int opPutNote = 0x12;
  static const int opDeleteNote = 0x13;
  static const int opListHomework = 0x20;
  static const int opGetHomework = 0x21;
  static const int opPutHomework = 0x22;
  static const int opDeleteHomework = 0x23;
  static const int opGetSettings = 0x30;
  static const int opPutSettings = 0x31;
  static const int opSyncPull = 0x40;
  static const int opSyncPush = 0x41;
  static const int opSyncSummary = 0x42;
  static const int opListMusic = 0x50;
  static const int opMusicControl = 0x51;
  static const int opNowPlaying = 0x52;
  static const int opDeleteMusic = 0x53;

  static const int responseBit = 0x80;

  // Music actions (MUSIC_CONTROL)
  static const int musicToggle = 1;
  static const int musicPlayIndex = 2;
  static const int musicNext = 3;
  static const int musicPrev = 4;
  static const int musicStop = 5;
  static const int musicSeek = 6;
  static const int musicSetVolume = 7;

  // Device settings byte limits
  static const int maxTrackNameLen = 96;

  // Status codes
  static const int stOk = 0x00;
  static const int stUnknown = 0x01;
  static const int stBadRequest = 0x02;
  static const int stNotFound = 0x03;
  static const int stUnauthorized = 0x04;
  static const int stConflict = 0x05;
  static const int stTooLarge = 0x06;
  static const int stStorage = 0x07;
  static const int stBusy = 0x08;
  static const int stVersion = 0x09;
  static const int stLocked = 0x0A;

  static String statusName(int s) {
    switch (s) {
      case stOk:
        return 'OK';
      case stUnknown:
        return 'Unknown error';
      case stBadRequest:
        return 'Bad request';
      case stNotFound:
        return 'Not found';
      case stUnauthorized:
        return 'Unauthorized (PIN)';
      case stConflict:
        return 'Conflict';
      case stTooLarge:
        return 'Too large';
      case stStorage:
        return 'Storage error';
      case stBusy:
        return 'Device busy';
      case stVersion:
        return 'Unsupported protocol version';
      case stLocked:
        return 'Auth locked, try later';
      default:
        return 'Error $s';
    }
  }
}
