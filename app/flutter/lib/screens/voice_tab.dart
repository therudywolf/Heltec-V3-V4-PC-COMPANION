import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../ble/bmw_ble_provider.dart';
import '../ble/bmw_ble_constants.dart';

/// Tab 3: Voice — Audio transport (Next/Prev), IKE injection (max 20 chars, Send to Cluster).
class VoiceTab extends ConsumerStatefulWidget {
  const VoiceTab({super.key});

  @override
  ConsumerState<VoiceTab> createState() => _VoiceTabState();
}

class _VoiceTabState extends ConsumerState<VoiceTab> {
  final _clusterController = TextEditingController();
  static const int _maxClusterChars = 20;

  @override
  void dispose() {
    _clusterController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final ble = ref.watch(bmwBleStateProvider);
    final notifier = ref.read(bmwBleStateProvider.notifier);
    final status = ble.status;
    final enabled = ble.isConnected && status.ibusSynced;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Text(
          'Audio',
          style: Theme.of(context).textTheme.titleMedium?.copyWith(
                color: Theme.of(context).colorScheme.secondary,
                fontWeight: FontWeight.w600,
              ),
        ),
        const SizedBox(height: 12),
        Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            FilledButton.icon(
              onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.mflPrev) : null,
              icon: const Icon(Icons.skip_previous),
              label: const Text('Prev'),
              style: FilledButton.styleFrom(
                backgroundColor: const Color(0xFF00A1E4),
                foregroundColor: Colors.white,
                padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
              ),
            ),
            const SizedBox(width: 16),
            FilledButton.icon(
              onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.mflNext) : null,
              icon: const Icon(Icons.skip_next),
              label: const Text('Next'),
              style: FilledButton.styleFrom(
                backgroundColor: const Color(0xFF00A1E4),
                foregroundColor: Colors.white,
                padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
              ),
            ),
          ],
        ),
        const SizedBox(height: 32),
        Text(
          'Cluster Injection',
          style: Theme.of(context).textTheme.titleMedium?.copyWith(
                color: Theme.of(context).colorScheme.secondary,
                fontWeight: FontWeight.w600,
              ),
        ),
        const SizedBox(height: 8),
        TextField(
          controller: _clusterController,
          maxLength: _maxClusterChars,
          decoration: InputDecoration(
            hintText: 'Max $_maxClusterChars characters',
            border: const OutlineInputBorder(),
            filled: true,
            fillColor: const Color(0xFF1E1E1E),
          ),
          style: const TextStyle(fontFamily: 'monospace'),
        ),
        const SizedBox(height: 12),
        SizedBox(
          width: double.infinity,
          child: FilledButton.icon(
            onPressed: enabled
                ? () {
                    final text = _clusterController.text.trim();
                    if (text.isNotEmpty) notifier.sendClusterText(text);
                  }
                : null,
            icon: const Icon(Icons.send),
            label: const Text('Send to Cluster'),
            style: FilledButton.styleFrom(
              backgroundColor: const Color(0xFFE21A23),
              foregroundColor: Colors.white,
              padding: const EdgeInsets.symmetric(vertical: 12),
            ),
          ),
        ),
        const SizedBox(height: 32),
      ],
    );
  }
}
