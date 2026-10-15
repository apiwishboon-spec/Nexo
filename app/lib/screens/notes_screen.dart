// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../ble/nexo_ble_client.dart';
import '../models/models.dart';
import '../state/app_state.dart';
import '../theme.dart';
import '../widgets/pixel_pet.dart';
import '../widgets/pixel_ui.dart';
import 'note_edit_screen.dart';

class NotesScreen extends StatefulWidget {
  const NotesScreen({super.key});

  @override
  State<NotesScreen> createState() => _NotesScreenState();
}

class _NotesScreenState extends State<NotesScreen> {
  String _query = '';

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();
    final scheme = Theme.of(context).colorScheme;
    final notes = app.visibleNotes.where((n) {
      if (_query.isEmpty) return true;
      final q = _query.toLowerCase();
      return n.title.toLowerCase().contains(q) ||
          n.body.toLowerCase().contains(q);
    }).toList();

    return Scaffold(
      backgroundColor: Colors.transparent,
      appBar: AppBar(
        leading: const Padding(
          padding: EdgeInsets.only(left: 12),
          child: PixelPet(size: 32),
        ),
        leadingWidth: 52,
        title: const Text('Notes'),
        actions: [
          IconButton(
            tooltip: 'Sync',
            onPressed: app.connState == NexoConnState.ready && !app.busy
                ? () => app.sync()
                : null,
            icon: Badge(
              isLabelVisible: app.pendingCount > 0,
              label: Text('${app.pendingCount}'),
              child: const Icon(Icons.sync),
            ),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: () => _openEditor(context),
        icon: const Icon(Icons.add),
        label: const Text('New note'),
      ),
      body: Column(
        children: [
          if (!app.connected)
            Material(
              color: scheme.surfaceContainerHighest,
              child: Padding(
                padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
                child: Row(
                  children: [
                    Icon(Icons.cloud_off, size: 18, color: scheme.onSurfaceVariant),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        'Offline — showing cached notes. Open the Device tab to connect.',
                        style: TextStyle(color: scheme.onSurfaceVariant),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          Padding(
            padding: const EdgeInsets.all(16),
            child: TextField(
              decoration: const InputDecoration(
                prefixIcon: Icon(Icons.search),
                hintText: 'Search notes',
              ),
              onChanged: (v) => setState(() => _query = v),
            ),
          ),
          Expanded(
            child: notes.isEmpty
                ? _empty(context, app)
                : ListView.builder(
                    padding: const EdgeInsets.fromLTRB(16, 0, 16, 96),
                    itemCount: notes.length,
                    itemBuilder: (ctx, i) => _noteTile(ctx, notes[i]),
                  ),
          ),
        ],
      ),
    );
  }

  Widget _empty(BuildContext context, AppState app) {
    return Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const PixelPet(size: 84, mood: PetMood.sleepy),
          const SizedBox(height: 16),
          PixelLabel(_query.isEmpty ? 'No notes yet' : 'No matches'),
        ],
      ),
    );
  }

  Widget _noteTile(BuildContext context, Note n) {
    final scheme = Theme.of(context).colorScheme;
    return Padding(
      padding: const EdgeInsets.only(bottom: 12),
      child: PixelPanel(
        padding: EdgeInsets.zero,
        child: ListTile(
          contentPadding:
              const EdgeInsets.symmetric(horizontal: 14, vertical: 6),
          title: Text(
            n.title.isEmpty ? '(untitled)' : n.title,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: const TextStyle(
              fontFamily: kFontDisplay,
              fontSize: 9,
              height: 1.6,
              color: NexoColors.onSurface,
            ),
          ),
          subtitle: Padding(
            padding: const EdgeInsets.only(top: 6),
            child: Text(
              n.body.isEmpty ? 'Empty note' : n.body,
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(
                fontFamily: kFontBody,
                fontSize: 18,
                height: 1.0,
                color: NexoColors.onSurfaceVariant,
              ),
            ),
          ),
          leading: Icon(
            n.dirty ? Icons.cloud_upload_outlined : Icons.description_outlined,
            color: n.dirty ? scheme.primary : scheme.outline,
          ),
          onTap: () => _openEditor(context, existing: n),
        ),
      ),
    );
  }

  Future<void> _openEditor(BuildContext context, {Note? existing}) async {
    final app = context.read<AppState>();
    await Navigator.of(context).push(
      MaterialPageRoute(
        builder: (_) => NoteEditScreen(note: existing),
      ),
    );
    // Persist handled inside the editor; nothing else required here.
    // ignore: unnecessary_statements
    app;
  }
}
