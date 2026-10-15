// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../models/models.dart';
import '../state/app_state.dart';
import '../widgets/pixel_pet.dart';
import '../widgets/pixel_ui.dart';

class HomeworkEditScreen extends StatefulWidget {
  const HomeworkEditScreen({super.key, this.homework});

  final Homework? homework;

  @override
  State<HomeworkEditScreen> createState() => _HomeworkEditScreenState();
}

class _HomeworkEditScreenState extends State<HomeworkEditScreen> {
  late final TextEditingController _subject;
  late final TextEditingController _description;
  late HomeworkStatus _status;
  int _dueEpochDay = 0;

  bool get isNew => widget.homework == null;

  @override
  void initState() {
    super.initState();
    final h = widget.homework;
    _subject = TextEditingController(text: h?.subject ?? '');
    _description = TextEditingController(text: h?.description ?? '');
    _status = h?.status ?? HomeworkStatus.notStarted;
    _dueEpochDay = h?.dueEpochDay ?? 0;
  }

  @override
  void dispose() {
    _subject.dispose();
    _description.dispose();
    super.dispose();
  }

  Future<void> _pickDueDate() async {
    final initial = dueDateFromEpochDay(_dueEpochDay) ??
        DateTime.now().toUtc().add(const Duration(days: 1));
    final picked = await showDatePicker(
      context: context,
      initialDate: initial,
      firstDate: DateTime.utc(2000),
      lastDate: DateTime.utc(2100),
    );
    if (picked != null) {
      setState(() => _dueEpochDay = epochDayFromDate(picked));
    }
  }

  Future<void> _save() async {
    final app = context.read<AppState>();
    final now = DateTime.now().millisecondsSinceEpoch ~/ 1000;
    final base = widget.homework;
    final record = Homework(
      id: base?.id ?? generateId(),
      subject: _subject.text.trim(),
      description: _description.text.trim(),
      dueEpochDay: _dueEpochDay,
      status: _status,
      created: base?.created ?? now,
      updated: now,
      rev: base?.rev ?? 0,
      dirty: true,
    );
    await app.saveHomeworkRecord(record);
    if (mounted) Navigator.pop(context);
  }

  Future<void> _delete() async {
    final app = context.read<AppState>();
    final h = widget.homework!;
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Delete homework?'),
        content: const Text('This entry will be removed from the device.'),
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
      await app.deleteHomework(h.id);
      if (mounted) Navigator.pop(context);
    }
  }

  @override
  Widget build(BuildContext context) {
    final due = dueDateFromEpochDay(_dueEpochDay);
    return Scaffold(
      backgroundColor: Colors.transparent,
      appBar: AppBar(
        leading: const Padding(
          padding: EdgeInsets.only(left: 12),
          child: PixelPet(size: 32),
        ),
        leadingWidth: 52,
        title: Text(isNew ? 'Add homework' : 'Edit homework'),
        actions: [
          if (!isNew)
            IconButton(
              tooltip: 'Delete',
              icon: const Icon(Icons.delete_outline),
              onPressed: _delete,
            ),
          TextButton(onPressed: _save, child: const Text('Save')),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          TextField(
            controller: _subject,
            textCapitalization: TextCapitalization.sentences,
            maxLength: 48,
            decoration: const InputDecoration(
              labelText: 'Subject',
              counterText: '',
            ),
          ),
          const SizedBox(height: 12),
          TextField(
            controller: _description,
            textCapitalization: TextCapitalization.sentences,
            maxLines: 5,
            maxLength: 500,
            decoration: const InputDecoration(
              labelText: 'Description',
              alignLabelWithHint: true,
            ),
          ),
          const SizedBox(height: 12),
          PixelPanel(
            padding: EdgeInsets.zero,
            child: ListTile(
              leading: const Icon(Icons.event),
              title: const Text('Due date'),
              subtitle: Text(
                due == null
                    ? 'Not set'
                    : dueIsoFromEpochDay(_dueEpochDay),
              ),
              trailing: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  if (_dueEpochDay != 0)
                    IconButton(
                      tooltip: 'Clear',
                      icon: const Icon(Icons.clear),
                      onPressed: () => setState(() => _dueEpochDay = 0),
                    ),
                  const Icon(Icons.chevron_right),
                ],
              ),
              onTap: _pickDueDate,
            ),
          ),
          const SizedBox(height: 12),
          const PixelLabel('Status'),
          const SizedBox(height: 8),
          SegmentedButton<HomeworkStatus>(
            segments: const [
              ButtonSegment(
                value: HomeworkStatus.notStarted,
                icon: Icon(Icons.radio_button_unchecked),
                label: Text('Todo'),
              ),
              ButtonSegment(
                value: HomeworkStatus.inProgress,
                icon: Icon(Icons.timelapse),
                label: Text('Doing'),
              ),
              ButtonSegment(
                value: HomeworkStatus.done,
                icon: Icon(Icons.check_circle),
                label: Text('Done'),
              ),
            ],
            selected: {_status},
            onSelectionChanged: (s) => setState(() => _status = s.first),
          ),
        ],
      ),
    );
  }
}
