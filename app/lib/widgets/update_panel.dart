// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:url_launcher/url_launcher.dart';

import '../state/update_state.dart';
import '../theme.dart';
import '../widgets/pixel_ui.dart';

/// "App updates" section: shows the running version, a manual check button,
/// an auto-check toggle and, when a newer release exists, its notes plus a
/// one-tap download & install action.
class UpdatePanel extends StatelessWidget {
  const UpdatePanel({super.key});

  @override
  Widget build(BuildContext context) {
    final u = context.watch<UpdateState>();
    final scheme = Theme.of(context).colorScheme;

    return PixelPanel(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              const PixelLabel('App updates'),
              Text(
                'v${u.currentVersion}'
                '${u.currentBuild.isNotEmpty ? ' (${u.currentBuild})' : ''}',
                style: const TextStyle(
                    fontFamily: kFontBody, fontSize: 18, color: NexoColors.primary),
              ),
            ],
          ),
          const SizedBox(height: 12),
          if (!u.supported)
            Text(
              'In-app updates are available on the Android and macOS builds.',
              style: TextStyle(
                  fontFamily: kFontBody,
                  fontSize: 18,
                  color: scheme.onSurfaceVariant),
            )
          else ...[
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              dense: true,
              value: u.autoCheck,
              title: const Text('Check automatically on launch'),
              onChanged: (v) => u.setAutoCheck(v),
            ),
            const SizedBox(height: 4),
            Row(
              children: [
                OutlinedButton.icon(
                  onPressed: u.checking || u.downloading
                      ? null
                      : () => u.check(),
                  icon: u.checking
                      ? const SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        )
                      : const Icon(Icons.refresh),
                  label: Text(u.checking ? 'Checking…' : 'Check for updates'),
                ),
                const SizedBox(width: 12),
                if (u.hasUpdate)
                  Expanded(
                    child: FilledButton.icon(
                      onPressed: u.downloading ? null : u.downloadAndInstall,
                      icon: const Icon(Icons.download),
                      label: Text('Update to ${u.available!.version}'),
                    ),
                  ),
              ],
            ),
            if (u.downloading) ...[
              const SizedBox(height: 12),
              LinearProgressIndicator(value: u.progress, minHeight: 8),
              const SizedBox(height: 6),
              Text('${(u.progress * 100).toStringAsFixed(0)}%',
                  style: const TextStyle(fontFamily: kFontBody, fontSize: 18)),
            ],
            if (u.status != null && !u.downloading) ...[
              const SizedBox(height: 8),
              Text(u.status!,
                  style: TextStyle(
                      fontFamily: kFontBody,
                      fontSize: 18,
                      color: scheme.onSurfaceVariant)),
            ],
            if (u.error != null) ...[
              const SizedBox(height: 8),
              Text(u.error!,
                  style: const TextStyle(
                      fontFamily: kFontBody,
                      fontSize: 18,
                      color: NexoColors.error)),
            ],
            if (u.hasUpdate && u.available!.notes.trim().isNotEmpty) ...[
              const SizedBox(height: 12),
              const PixelLabel('What\'s new'),
              const SizedBox(height: 6),
              Container(
                constraints: const BoxConstraints(maxHeight: 160),
                child: SingleChildScrollView(
                  child: Text(
                    u.available!.notes.trim(),
                    style: TextStyle(
                        fontFamily: kFontBody,
                        fontSize: 16,
                        height: 1.25,
                        color: scheme.onSurfaceVariant),
                  ),
                ),
              ),
            ],
            const SizedBox(height: 4),
            Align(
              alignment: Alignment.centerLeft,
              child: TextButton.icon(
                onPressed: () => launchUrl(
                  Uri.parse(UpdateState.releaseUrl(u.available)),
                  mode: LaunchMode.externalApplication,
                ),
                icon: const Icon(Icons.open_in_new, size: 18),
                label: const Text('Open release page'),
              ),
            ),
          ],
        ],
      ),
    );
  }
}
