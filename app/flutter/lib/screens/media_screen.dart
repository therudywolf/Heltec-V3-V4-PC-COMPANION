import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../ble/bmw_ble_provider.dart';

/// Media: send track/artist to IKE; cluster text. Shows BLE errors.
class MediaScreen extends ConsumerStatefulWidget {
  const MediaScreen({super.key});

  @override
  ConsumerState<MediaScreen> createState() => _MediaScreenState();
}

class _MediaScreenState extends ConsumerState<MediaScreen> {
  final _trackController = TextEditingController();
  final _artistController = TextEditingController();
  final _clusterController = TextEditingController();

  @override
  void dispose() {
    _trackController.dispose();
    _artistController.dispose();
    _clusterController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final ble = ref.watch(bmwBleStateProvider);
    final notifier = ref.read(bmwBleStateProvider.notifier);
    final enabled = ble.isConnected;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Text(
          'Медиа → IKE',
          style: Theme.of(context).textTheme.headlineSmall,
        ),
        const SizedBox(height: 8),
        Text(
          'Трек и исполнитель на кластер. Текст на кластер — до 20 символов.',
          style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                color: Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.7),
              ),
        ),
        if (ble.error != null) ...[
          const SizedBox(height: 12),
          Card(
            color: Theme.of(context).colorScheme.errorContainer,
            child: Padding(
              padding: const EdgeInsets.all(12),
              child: Text(
                ble.error!,
                style: TextStyle(color: Theme.of(context).colorScheme.onErrorContainer, fontSize: 12),
              ),
            ),
          ),
        ],
        const SizedBox(height: 24),
        TextField(
          controller: _trackController,
          decoration: const InputDecoration(
            labelText: 'Трек',
            border: OutlineInputBorder(),
          ),
          enabled: enabled,
        ),
        const SizedBox(height: 12),
        TextField(
          controller: _artistController,
          decoration: const InputDecoration(
            labelText: 'Исполнитель',
            border: OutlineInputBorder(),
          ),
          enabled: enabled,
        ),
        const SizedBox(height: 16),
        FilledButton.icon(
          onPressed: enabled
              ? () => notifier.sendNowPlaying(
                    _trackController.text.trim(),
                    _artistController.text.trim(),
                  )
              : null,
          icon: const Icon(Icons.send),
          label: const Text('Отправить на кластер'),
        ),
        const SizedBox(height: 32),
        Text('Текст на кластер', style: Theme.of(context).textTheme.titleMedium),
        const SizedBox(height: 8),
        TextField(
          controller: _clusterController,
          decoration: const InputDecoration(
            hintText: 'Макс. 20 символов',
            border: OutlineInputBorder(),
          ),
          maxLength: 20,
          enabled: enabled,
        ),
        FilledButton.icon(
          onPressed: enabled
              ? () => notifier.sendClusterText(_clusterController.text.trim())
              : null,
          icon: const Icon(Icons.text_fields),
          label: const Text('Отправить на кластер'),
        ),
        const SizedBox(height: 24),
      ],
    );
  }
}
