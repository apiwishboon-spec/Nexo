// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../state/app_state.dart';
import '../state/update_state.dart';
import '../theme.dart';
import '../widgets/pixel_pet.dart';
import '../widgets/pixel_ui.dart';
import 'about_screen.dart';
import 'device_screen.dart';
import 'homework_screen.dart';
import 'music_screen.dart';
import 'notes_screen.dart';
import 'settings_screen.dart';

class HomeShell extends StatefulWidget {
  const HomeShell({super.key});

  @override
  State<HomeShell> createState() => _HomeShellState();
}

class _HomeShellState extends State<HomeShell> {
  int _index = 0;

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();

    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (app.error != null) {
        ScaffoldMessenger.maybeOf(context)?.showSnackBar(
          SnackBar(content: Text(app.error!)),
        );
        context.read<AppState>().clearError();
      } else if (context.read<AppState>().toast != null) {
        final t = context.read<AppState>().toast!;
        context.read<AppState>().clearToast();
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text(t)));
      }

      final upd = context.read<UpdateState>();
      if (upd.hasUpdate && !upd.prompted) {
        upd.prompted = true;
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Nexo ${upd.available!.version} is available'),
            action: SnackBarAction(
              label: 'Details',
              onPressed: () => setState(() => _index = 5), // About tab
            ),
          ),
        );
      }
    });

    const pages = [
      NotesScreen(),
      HomeworkScreen(),
      MusicScreen(),
      DeviceScreen(),
      SettingsScreen(),
      AboutScreen(),
    ];

    final body = Stack(
      children: [
        const Positioned.fill(child: PixelGrid()),
        IndexedStack(index: _index, children: pages),
      ],
    );

    if (MediaQuery.sizeOf(context).width >= 720) {
      return Scaffold(
        body: Row(
          children: [
            NavigationRail(
              selectedIndex: _index,
              onDestinationSelected: (i) => setState(() => _index = i),
              labelType: NavigationRailLabelType.all,
              leading: const Padding(
                padding: EdgeInsets.only(top: 16, bottom: 8),
                child: _Brand(),
              ),
              destinations: const [
                NavigationRailDestination(
                  icon: Icon(Icons.description_outlined),
                  selectedIcon: Icon(Icons.description),
                  label: Text('Notes'),
                ),
                NavigationRailDestination(
                  icon: Icon(Icons.school_outlined),
                  selectedIcon: Icon(Icons.school),
                  label: Text('Homework'),
                ),
                NavigationRailDestination(
                  icon: Icon(Icons.music_note_outlined),
                  selectedIcon: Icon(Icons.music_note),
                  label: Text('Music'),
                ),
                NavigationRailDestination(
                  icon: Icon(Icons.bluetooth_outlined),
                  selectedIcon: Icon(Icons.bluetooth),
                  label: Text('Device'),
                ),
                NavigationRailDestination(
                  icon: Icon(Icons.settings_outlined),
                  selectedIcon: Icon(Icons.settings),
                  label: Text('Settings'),
                ),
                NavigationRailDestination(
                  icon: Icon(Icons.info_outline),
                  selectedIcon: Icon(Icons.info),
                  label: Text('About'),
                ),
              ],
            ),
            const VerticalDivider(width: 2, thickness: 2),
            Expanded(child: body),
          ],
        ),
      );
    }

    return Scaffold(
      body: body,
      bottomNavigationBar: Container(
        decoration: const BoxDecoration(
          border: Border(top: BorderSide(color: NexoColors.outline, width: 2)),
        ),
        child: NavigationBar(
          selectedIndex: _index,
          onDestinationSelected: (i) => setState(() => _index = i),
          destinations: const [
            NavigationDestination(
              icon: Icon(Icons.description_outlined),
              selectedIcon: Icon(Icons.description),
              label: 'Notes',
            ),
            NavigationDestination(
              icon: Icon(Icons.school_outlined),
              selectedIcon: Icon(Icons.school),
              label: 'Homework',
            ),
            NavigationDestination(
              icon: Icon(Icons.music_note_outlined),
              selectedIcon: Icon(Icons.music_note),
              label: 'Music',
            ),
            NavigationDestination(
              icon: Icon(Icons.bluetooth_outlined),
              selectedIcon: Icon(Icons.bluetooth),
              label: 'Device',
            ),
            NavigationDestination(
              icon: Icon(Icons.settings_outlined),
              selectedIcon: Icon(Icons.settings),
              label: 'Settings',
            ),
            NavigationDestination(
              icon: Icon(Icons.info_outline),
              selectedIcon: Icon(Icons.info),
              label: 'About',
            ),
          ],
        ),
      ),
    );
  }
}

class _Brand extends StatelessWidget {
  const _Brand();

  @override
  Widget build(BuildContext context) {
    return const Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        PixelPet(size: 40),
        SizedBox(height: 6),
        Text(
          'NEXO',
          style: TextStyle(
            fontFamily: kFontDisplay,
            fontSize: 8,
            letterSpacing: 1.5,
            color: NexoColors.primary,
          ),
        ),
      ],
    );
  }
}
