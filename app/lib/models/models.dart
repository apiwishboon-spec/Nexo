// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
/// Nexo data models. These mirror the firmware's portable core types
/// (firmware/lib/nexo_core/src/nexo_types.h).
library;

import 'dart:convert';
import 'dart:math';

/// Record-kind identifiers used by the sync codec.
class RecordKind {
  static const int note = 1;
  static const int homework = 2;
}

enum HomeworkStatus { notStarted, inProgress, done }

extension HomeworkStatusX on HomeworkStatus {
  int get wire => index;
  String get label => switch (this) {
        HomeworkStatus.notStarted => 'Not started',
        HomeworkStatus.inProgress => 'In progress',
        HomeworkStatus.done => 'Done',
      };
  static HomeworkStatus fromWire(int v) =>
      v <= 0 ? HomeworkStatus.notStarted : (v == 1 ? HomeworkStatus.inProgress : HomeworkStatus.done);
}

class Note {
  String id;
  String title;
  String body;
  int created;
  int updated;
  int rev;
  bool deleted;

  /// True when this record has local edits not yet confirmed by the device.
  bool dirty;

  Note({
    required this.id,
    this.title = '',
    this.body = '',
    this.created = 0,
    this.updated = 0,
    this.rev = 0,
    this.deleted = false,
    this.dirty = false,
  });

  Map<String, dynamic> toJson() => {
        'id': id,
        'title': title,
        'body': body,
        'created': created,
        'updated': updated,
        'rev': rev,
        'deleted': deleted,
        'dirty': dirty,
      };

  factory Note.fromJson(Map<String, dynamic> j) => Note(
        id: j['id'] as String,
        title: (j['title'] ?? '') as String,
        body: (j['body'] ?? '') as String,
        created: (j['created'] ?? 0) as int,
        updated: (j['updated'] ?? 0) as int,
        rev: (j['rev'] ?? 0) as int,
        deleted: (j['deleted'] ?? false) as bool,
        dirty: (j['dirty'] ?? false) as bool,
      );

  Note copyWith({
    String? title,
    String? body,
    int? created,
    int? updated,
    int? rev,
    bool? deleted,
    bool? dirty,
  }) =>
      Note(
        id: id,
        title: title ?? this.title,
        body: body ?? this.body,
        created: created ?? this.created,
        updated: updated ?? this.updated,
        rev: rev ?? this.rev,
        deleted: deleted ?? this.deleted,
        dirty: dirty ?? this.dirty,
      );
}

class Homework {
  String id;
  String subject;
  String description;

  /// Days since 1970-01-01 UTC; 0 means unset.
  int dueEpochDay;
  HomeworkStatus status;
  int created;
  int updated;
  int rev;
  bool deleted;
  bool dirty;

  Homework({
    required this.id,
    this.subject = '',
    this.description = '',
    this.dueEpochDay = 0,
    this.status = HomeworkStatus.notStarted,
    this.created = 0,
    this.updated = 0,
    this.rev = 0,
    this.deleted = false,
    this.dirty = false,
  });

  Map<String, dynamic> toJson() => {
        'id': id,
        'subject': subject,
        'description': description,
        'dueEpochDay': dueEpochDay,
        'status': status.index,
        'created': created,
        'updated': updated,
        'rev': rev,
        'deleted': deleted,
        'dirty': dirty,
      };

  factory Homework.fromJson(Map<String, dynamic> j) => Homework(
        id: j['id'] as String,
        subject: (j['subject'] ?? '') as String,
        description: (j['description'] ?? '') as String,
        dueEpochDay: (j['dueEpochDay'] ?? 0) as int,
        status: HomeworkStatusX.fromWire((j['status'] ?? 0) as int),
        created: (j['created'] ?? 0) as int,
        updated: (j['updated'] ?? 0) as int,
        rev: (j['rev'] ?? 0) as int,
        deleted: (j['deleted'] ?? false) as bool,
        dirty: (j['dirty'] ?? false) as bool,
      );

  Homework copyWith({
    String? subject,
    String? description,
    int? dueEpochDay,
    HomeworkStatus? status,
    int? created,
    int? updated,
    int? rev,
    bool? deleted,
    bool? dirty,
  }) =>
      Homework(
        id: id,
        subject: subject ?? this.subject,
        description: description ?? this.description,
        dueEpochDay: dueEpochDay ?? this.dueEpochDay,
        status: status ?? this.status,
        created: created ?? this.created,
        updated: updated ?? this.updated,
        rev: rev ?? this.rev,
        deleted: deleted ?? this.deleted,
        dirty: dirty ?? this.dirty,
      );
}

class DeviceSettings {
  String deviceName;
  String apSsid;
  bool apEnabled;

  /// Only sent when the user wants to change the PIN.
  String? pin;

  // v2 extended settings.
  int volume;
  bool soundEnabled;
  int brightness;
  int screenTimeoutSec;

  /// Minutes east of UTC. The device has no RTC, so the app supplies this.
  int timezoneOffsetMin;
  bool autoAdvanceMusic;

  /// Only sent when the user wants to change the AP password.
  String? apPassword;

  DeviceSettings({
    this.deviceName = 'Nexo',
    this.apSsid = 'Nexo',
    this.apEnabled = false,
    this.pin,
    this.volume = 60,
    this.soundEnabled = true,
    this.brightness = 100,
    this.screenTimeoutSec = 30,
    this.timezoneOffsetMin = 0,
    this.autoAdvanceMusic = true,
    this.apPassword,
  });

  Map<String, dynamic> toJson() => {
        'deviceName': deviceName,
        'apSsid': apSsid,
        'apEnabled': apEnabled,
        'volume': volume,
        'soundEnabled': soundEnabled,
        'brightness': brightness,
        'screenTimeoutSec': screenTimeoutSec,
        'timezoneOffsetMin': timezoneOffsetMin,
        'autoAdvanceMusic': autoAdvanceMusic,
      };

  factory DeviceSettings.fromJson(Map<String, dynamic> j) => DeviceSettings(
        deviceName: (j['deviceName'] ?? 'Nexo') as String,
        apSsid: (j['apSsid'] ?? 'Nexo') as String,
        apEnabled: (j['apEnabled'] ?? false) as bool,
        volume: (j['volume'] ?? 60) as int,
        soundEnabled: (j['soundEnabled'] ?? true) as bool,
        brightness: (j['brightness'] ?? 100) as int,
        screenTimeoutSec: (j['screenTimeoutSec'] ?? 30) as int,
        timezoneOffsetMin: (j['timezoneOffsetMin'] ?? 0) as int,
        autoAdvanceMusic: (j['autoAdvanceMusic'] ?? true) as bool,
      );

  DeviceSettings copyWith({
    String? deviceName,
    String? apSsid,
    bool? apEnabled,
    String? pin,
    int? volume,
    bool? soundEnabled,
    int? brightness,
    int? screenTimeoutSec,
    int? timezoneOffsetMin,
    bool? autoAdvanceMusic,
    String? apPassword,
  }) =>
      DeviceSettings(
        deviceName: deviceName ?? this.deviceName,
        apSsid: apSsid ?? this.apSsid,
        apEnabled: apEnabled ?? this.apEnabled,
        pin: pin,
        volume: volume ?? this.volume,
        soundEnabled: soundEnabled ?? this.soundEnabled,
        brightness: brightness ?? this.brightness,
        screenTimeoutSec: screenTimeoutSec ?? this.screenTimeoutSec,
        timezoneOffsetMin: timezoneOffsetMin ?? this.timezoneOffsetMin,
        autoAdvanceMusic: autoAdvanceMusic ?? this.autoAdvanceMusic,
        apPassword: apPassword,
      );
}

/// Live device health/status snapshot (GET_DEVICE_INFO response).
class DeviceDetails {
  int protocolVersion;
  int batteryPercent;
  bool charging;
  int uptimeSec;
  int freeHeapKiB;
  bool sdPresent;
  int sdFreeMb;
  int sdTotalMb;
  int musicCount;
  int volume;
  bool soundEnabled;
  bool headphoneIn;
  bool apEnabled;
  int rssi;
  int screenTimeoutSec;
  String deviceName;
  String firmwareVersion;
  String mac;
  String apSsid;
  String apPassword;
  String ip;

  DeviceDetails({
    this.protocolVersion = 0,
    this.batteryPercent = 0xFF,
    this.charging = false,
    this.uptimeSec = 0,
    this.freeHeapKiB = 0,
    this.sdPresent = false,
    this.sdFreeMb = 0,
    this.sdTotalMb = 0,
    this.musicCount = 0,
    this.volume = 0,
    this.soundEnabled = true,
    this.headphoneIn = false,
    this.apEnabled = false,
    this.rssi = 0,
    this.screenTimeoutSec = 0,
    this.deviceName = '',
    this.firmwareVersion = '',
    this.mac = '',
    this.apSsid = '',
    this.apPassword = '',
    this.ip = '',
  });

  bool get batteryKnown => batteryPercent != 0xFF;
}

class MusicTrack {
  String name;
  int size;
  MusicTrack(this.name, this.size);
}

/// A voice memo stored on the device's SD card (`/memos`).
class MemoItem {
  String name;
  int size;
  MemoItem(this.name, this.size);
}

class MusicStatus {
  int index;
  bool playing;
  int positionSec;
  int durationSec;
  int volume;

  MusicStatus({
    this.index = -1,
    this.playing = false,
    this.positionSec = 0,
    this.durationSec = 0,
    this.volume = 0,
  });

  bool get hasTrack => index >= 0;
}


/// Generates a 16-hex-character identifier (8 random bytes).
String generateId([Random? rng]) {
  final r = rng ?? Random.secure();
  final sb = StringBuffer();
  for (var i = 0; i < 8; i++) {
    sb.write(r.nextInt(256).toRadixString(16).padLeft(2, '0'));
  }
  return sb.toString();
}

bool isValidId(String id) {
  if (id.length != 16) return false;
  return RegExp(r'^[0-9a-fA-F]{16}$').hasMatch(id);
}

// ---- Due-date helpers (mirror nexo_types.cpp) -------------------------------

DateTime? dueDateFromEpochDay(int epochDay) {
  if (epochDay <= 0) return null;
  return DateTime.utc(1970, 1, 1).add(Duration(days: epochDay - 1));
}

int epochDayFromDate(DateTime d) {
  final days = DateTime.utc(d.year, d.month, d.day)
      .difference(DateTime.utc(1970, 1, 1))
      .inDays;
  return days + 1;
}

String dueIsoFromEpochDay(int epochDay) {
  final d = dueDateFromEpochDay(epochDay);
  if (d == null) return '';
  return '${d.year.toString().padLeft(4, '0')}-'
      '${d.month.toString().padLeft(2, '0')}-'
      '${d.day.toString().padLeft(2, '0')}';
}

/// Encodes the settings map as a compact JSON string for the local cache.
String settingsToJson(DeviceSettings s) => jsonEncode(s.toJson());
