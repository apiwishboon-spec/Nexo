// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../protocol/constants.dart';
import '../state/app_state.dart';
import '../widgets/pixel_pet.dart';
import '../widgets/pixel_ui.dart';
import '../widgets/update_panel.dart';

class AboutScreen extends StatelessWidget {
  const AboutScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();
    final scheme = Theme.of(context).colorScheme;
    return Scaffold(
      backgroundColor: Colors.transparent,
      appBar: AppBar(
        leading: const Padding(
          padding: EdgeInsets.only(left: 12),
          child: PixelPet(size: 32),
        ),
        leadingWidth: 52,
        title: const Text('About'),
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          PixelPanel(
            child: Row(
              children: [
                const PixelPet(size: 72, mood: PetMood.happy),
                const SizedBox(width: 16),
                Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('Nexo',
                        style: Theme.of(context).textTheme.headlineSmall),
                    const SizedBox(height: 8),
                    const PixelLabel('Cardputer companion'),
                  ],
                ),
              ],
            ),
          ),
          const SizedBox(height: 20),
          PixelPanel(
            padding: EdgeInsets.zero,
            child: Column(
              children: [
                const ListTile(
                  leading: Icon(Icons.memory),
                  title: Text('Firmware'),
                  subtitle: Text('M5Stack Cardputer / Stamp-S3A'),
                ),
                const Divider(height: 1),
                ListTile(
                  leading: const Icon(Icons.settings_ethernet),
                  title: const Text('Protocol version'),
                  subtitle: Text('${NexoProtocol.version}'),
                ),
                if (app.details != null && app.details!.firmwareVersion.isNotEmpty) ...[
                  const Divider(height: 1),
                  ListTile(
                    leading: const Icon(Icons.memory),
                    title: const Text('Device firmware'),
                    subtitle: Text(app.details!.firmwareVersion),
                  ),
                ],
                const Divider(height: 1),
                const ListTile(
                  leading: Icon(Icons.wifi),
                  title: Text('Wireless'),
                  subtitle: Text('Bluetooth Low Energy + optional Wi-Fi AP'),
                ),
              ],
            ),
          ),
          const SizedBox(height: 16),
          if (app.connected)
            PixelPanel(
              padding: EdgeInsets.zero,
              child: Column(
                children: [
                  ListTile(
                    leading: const Icon(Icons.link),
                    title: const Text('Connected device'),
                    subtitle: Text(app.connectedId ?? ''),
                  ),
                  const Divider(height: 1),
                  ListTile(
                    leading: const Icon(Icons.description_outlined),
                    title: const Text('Notes on device'),
                    subtitle: Text('${app.visibleNotes.length}'),
                  ),
                  const Divider(height: 1),
                  ListTile(
                    leading: const Icon(Icons.school_outlined),
                    title: const Text('Homework entries'),
                    subtitle: Text('${app.visibleHomework.length}'),
                  ),
                ],
              ),
            ),
          const SizedBox(height: 16),
          const UpdatePanel(),
          const SizedBox(height: 16),
          Text(
            'Nexo is offline-first: your notes and homework live on the '
            'Cardputer. This app mirrors them and syncs changes over Bluetooth.',
            style: TextStyle(color: scheme.onSurfaceVariant, fontSize: 18),
          ),
        ],
      ),
    );
  }
}
