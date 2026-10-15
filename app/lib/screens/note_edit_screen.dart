// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../models/models.dart';
import '../state/app_state.dart';
import '../widgets/pixel_pet.dart';

class NoteEditScreen extends StatefulWidget {
  const NoteEditScreen({super.key, this.note});

  final Note? note;

  @override
  State<NoteEditScreen> createState() => _NoteEditScreenState();
}

class _NoteEditScreenState extends State<NoteEditScreen> {
  late final TextEditingController _title;
  late final TextEditingController _body;
  bool _saving = false;

  bool get isNew => widget.note == null;

  @override
  void initState() {
    super.initState();
    _title = TextEditingController(text: widget.note?.title ?? '');
    _body = TextEditingController(text: widget.note?.body ?? '');
  }

  @override
  void dispose() {
    _title.dispose();
    _body.dispose();
    super.dispose();
  }

  Future<void> _save() async {
    final app = context.read<AppState>();
    setState(() => _saving = true);
    try {
      if (isNew) {
        await app.createNote(title: _title.text.trim(), body: _body.text);
      } else {
        await context.read<AppState>().updateNote(
              widget.note!,
              title: _title.text,
              body: _body.text,
            );
      }
      if (mounted) Navigator.of(context).pop();
    } finally {
      if (mounted) setState(() => _saving = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final maxBody = 4000;
    return Scaffold(
      backgroundColor: Colors.transparent,
      appBar: AppBar(
        leading: const Padding(
          padding: EdgeInsets.only(left: 12),
          child: PixelPet(size: 32),
        ),
        leadingWidth: 52,
        title: Text(isNew ? 'New note' : 'Edit note'),
        actions: [
          if (!isNew)
            IconButton(
              tooltip: 'Delete',
              icon: const Icon(Icons.delete_outline),
              onPressed: _saving ? null : _confirmDelete,
            ),
          TextButton(
            onPressed: _saving ? null : _save,
            child: const Text('Save'),
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            TextField(
              controller: _title,
              textCapitalization: TextCapitalization.sentences,
              maxLength: 64,
              decoration: const InputDecoration(
                labelText: 'Title',
                counterText: '',
              ),
            ),
            const SizedBox(height: 12),
            Expanded(
              child: TextField(
                controller: _body,
                expands: true,
                maxLines: null,
                minLines: null,
                maxLength: maxBody,
                textAlignVertical: TextAlignVertical.top,
                textCapitalization: TextCapitalization.sentences,
                decoration: const InputDecoration(
                  labelText: 'Body',
                  alignLabelWithHint: true,
                  counterText: '',
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Future<void> _confirmDelete() async {
    final app = context.read<AppState>();
    final n = widget.note!;
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Delete note?'),
        content: const Text('This note will be removed from the device.'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Delete'),
          ),
        ],
      ),
    );
    if (ok == true) {
      await app.deleteNote(n.id);
      if (mounted) Navigator.pop(context);
    }
  }
}
