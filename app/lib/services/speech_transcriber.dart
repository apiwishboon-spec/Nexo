// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:io';

import 'package:flutter/services.dart';

/// On-device speech-to-text for a downloaded voice memo.
///
/// Only macOS implements this (Apple's Speech framework can transcribe an
/// existing audio file). Android has no offline file transcription, so callers
/// fall back to the OS share sheet there.
class SpeechTranscriber {
  static const MethodChannel _channel = MethodChannel('com.nexo.nexo_app/speech');

  /// Whether in-app file transcription is available on this platform.
  static bool get supported => Platform.isMacOS;

  /// Transcribes the WAV at [path] and returns the recognised text.
  static Future<String> transcribeFile(String path) async {
    if (!Platform.isMacOS) {
      throw UnsupportedError('On-device transcription is only available on macOS.');
    }
    final text =
        await _channel.invokeMethod<String>('transcribeFile', {'path': path});
    return text ?? '';
  }
}
