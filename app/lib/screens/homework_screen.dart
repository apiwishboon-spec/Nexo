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
import 'homework_edit_screen.dart';

class HomeworkScreen extends StatelessWidget {
  const HomeworkScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();
    final items = app.visibleHomework;

    return Scaffold(
      backgroundColor: Colors.transparent,
      appBar: AppBar(
        leading: const Padding(
          padding: EdgeInsets.only(left: 12),
          child: PixelPet(size: 32),
        ),
        leadingWidth: 52,
        title: const Text('Homework'),
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
        onPressed: () => _open(context),
        icon: const Icon(Icons.add),
        label: const Text('Add homework'),
      ),
      body: items.isEmpty
          ? Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  const PixelPet(size: 84, mood: PetMood.sleepy),
                  const SizedBox(height: 16),
                  const PixelLabel('No homework yet'),
                ],
              ),
            )
          : ListView.builder(
              padding: const EdgeInsets.fromLTRB(16, 16, 16, 96),
              itemCount: items.length,
              itemBuilder: (ctx, i) => _tile(ctx, app, items[i]),
            ),
    );
  }

  Widget _tile(BuildContext context, AppState app, Homework h) {
    final scheme = Theme.of(context).colorScheme;
    final due = dueDateFromEpochDay(h.dueEpochDay);
    final dueText = due == null
        ? 'No due date'
        : 'Due ${due.year}-${due.month.toString().padLeft(2, '0')}-'
            '${due.day.toString().padLeft(2, '0')}';
    final overdue = due != null &&
        h.status != HomeworkStatus.done &&
        due.isBefore(DateTime.now().toUtc());

    return Padding(
      padding: const EdgeInsets.only(bottom: 12),
      child: PixelPanel(
        padding: EdgeInsets.zero,
        child: Column(
          children: [
            ListTile(
              leading: CircleAvatar(
                backgroundColor: _statusColor(scheme, h.status),
                child:
                    Icon(_statusIcon(h.status), color: NexoColors.ink, size: 20),
              ),
              title: Text(
                h.subject.isEmpty ? '(no subject)' : h.subject,
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
                  dueText + (h.description.isEmpty ? '' : '  ·  ${h.description}'),
                  maxLines: 2,
                  overflow: TextOverflow.ellipsis,
                  style: TextStyle(
                    fontFamily: kFontBody,
                    fontSize: 18,
                    height: 1.0,
                    color: overdue ? scheme.error : scheme.onSurfaceVariant,
                  ),
                ),
              ),
              trailing: h.dirty
                  ? Icon(Icons.cloud_upload_outlined,
                      color: scheme.primary, size: 18)
                  : null,
              onTap: () => _open(context, h),
            ),
            Padding(
              padding: const EdgeInsets.fromLTRB(12, 0, 12, 10),
              child: SegmentedButton<HomeworkStatus>(
                segments: const [
                  ButtonSegment(
                    value: HomeworkStatus.notStarted,
                    label: Text('Todo'),
                  ),
                  ButtonSegment(
                    value: HomeworkStatus.inProgress,
                    label: Text('Doing'),
                  ),
                  ButtonSegment(
                    value: HomeworkStatus.done,
                    label: Text('Done'),
                  ),
                ],
                selected: {h.status},
                showSelectedIcon: false,
                onSelectionChanged: (s) => app.setHomeworkStatus(h, s.first),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Color _statusColor(ColorScheme scheme, HomeworkStatus s) => switch (s) {
        HomeworkStatus.notStarted => scheme.outline,
        HomeworkStatus.inProgress => scheme.tertiary,
        HomeworkStatus.done => scheme.primary,
      };

  IconData _statusIcon(HomeworkStatus s) => switch (s) {
        HomeworkStatus.notStarted => Icons.radio_button_unchecked,
        HomeworkStatus.inProgress => Icons.timelapse,
        HomeworkStatus.done => Icons.check,
      };

  void _open(BuildContext context, [Homework? h]) {
    Navigator.push(
      context,
      MaterialPageRoute(builder: (_) => HomeworkEditScreen(homework: h)),
    );
  }
}
