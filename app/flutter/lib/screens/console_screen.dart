import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../ble/bmw_ble_provider.dart';
import '../ble/bmw_ble_constants.dart';

/// Console: MFL Prev/Next controls and 20-char text input to push to physical IKE screen (Radio mode).
class ConsoleScreen extends ConsumerStatefulWidget {
  const ConsoleScreen({super.key});

  @override
  ConsumerState<ConsoleScreen> createState() => _ConsoleScreenState();
}

class _ConsoleScreenState extends ConsumerState<ConsoleScreen> {
  final _ikeTextController = TextEditingController();
  static const int _ikeTextMaxLength = 20;

  @override
  void dispose() {
    _ikeTextController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final ble = ref.watch(bmwBleStateProvider);
    final notifier = ref.read(bmwBleStateProvider.notifier);
    final enabled = ble.isConnected && ble.status.ibusSynced;
    final primary = Theme.of(context).colorScheme.primary;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Text(
          'Console',
          style: Theme.of(context).textTheme.headlineSmall,
        ),
        const SizedBox(height: 8),
        Text(
          'MFL controls and IKE text (20 chars, padded on bus).',
          style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                color: Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.7),
              ),
        ),
        const SizedBox(height: 24),
        Text('MFL', style: Theme.of(context).textTheme.titleMedium?.copyWith(color: primary)),
        const SizedBox(height: 8),
        Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            FilledButton.tonalIcon(
              onPressed: enabled
                  ? () => notifier.sendControl(BmwControlCmd.mflPrev)
                  : null,
              icon: const Icon(Icons.skip_previous),
              label: const Text('Prev'),
            ),
            const SizedBox(width: 16),
            FilledButton.tonalIcon(
              onPressed: enabled
                  ? () => notifier.sendControl(BmwControlCmd.mflNext)
                  : null,
              icon: const Icon(Icons.skip_next),
              label: const Text('Next'),
            ),
          ],
        ),
        const SizedBox(height: 8),
        Text(
          'Prev/Next send MFL commands to radio (track skip).',
          style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.6),
              ),
        ),
        const SizedBox(height: 32),
        Text('IKE text (Radio mode)', style: Theme.of(context).textTheme.titleMedium?.copyWith(color: primary)),
        const SizedBox(height: 8),
        TextField(
          controller: _ikeTextController,
          maxLength: _ikeTextMaxLength,
          decoration: const InputDecoration(
            hintText: 'Up to 20 characters',
            border: OutlineInputBorder(),
            counterText: '',
          ),
          enabled: enabled,
          style: Theme.of(context).textTheme.titleMedium?.copyWith(fontFamily: 'monospace'),
        ),
        const SizedBox(height: 8),
        Text(
          'Padded with spaces to 20 chars; sent as C8 80 23 42 32 to IKE.',
          style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.6),
              ),
        ),
        const SizedBox(height: 12),
        FilledButton.icon(
          onPressed: enabled
              ? () {
                  final text = _ikeTextController.text;
                  if (text.isEmpty) return;
                  notifier.sendClusterText(text.length > _ikeTextMaxLength
                      ? text.substring(0, _ikeTextMaxLength)
                      : text);
                }
              : null,
          icon: const Icon(Icons.send),
          label: const Text('Send to IKE'),
        ),
        const SizedBox(height: 24),
      ],
    );
  }
}
