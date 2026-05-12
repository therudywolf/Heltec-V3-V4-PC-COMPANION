import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../ble/bmw_ble_provider.dart';
import '../ble/bmw_ble_constants.dart';

/// Tab 2: Instincts — Windows/doors, Lighting (Welcome, Follow Me Home, Strobe, Sensory Dark, Mirror Fold).
class InstinctsTab extends ConsumerStatefulWidget {
  const InstinctsTab({super.key});

  @override
  ConsumerState<InstinctsTab> createState() => _InstinctsTabState();
}

class _InstinctsTabState extends ConsumerState<InstinctsTab> {
  bool _welcomeLights = false;
  bool _sensoryDark = false;
  bool _mirrorFoldOnLock = false;

  @override
  Widget build(BuildContext context) {
    final ble = ref.watch(bmwBleStateProvider);
    final notifier = ref.read(bmwBleStateProvider.notifier);
    final status = ble.status;
    final enabled = ble.isConnected && status.ibusSynced;
    const mRed = Color(0xFFE21A23);

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        _SectionTitle(title: 'Windows / Doors'),
        const SizedBox(height: 8),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            FilledButton.icon(
              onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.interiorOn3s) : null,
              icon: const Icon(Icons.air, size: 18),
              label: const Text('Ventilation (Drop 3 cm)'),
              style: FilledButton.styleFrom(backgroundColor: const Color(0xFF00A1E4), foregroundColor: Colors.white),
            ),
            OutlinedButton.icon(
              onPressed: enabled ? () {
                notifier.sendControl(BmwControlCmd.windowFrontDriverClose);
                notifier.sendControl(BmwControlCmd.windowFrontPassengerClose);
                notifier.sendControl(BmwControlCmd.windowRearDriverClose);
                notifier.sendControl(BmwControlCmd.windowRearPassengerClose);
              } : null,
              icon: const Icon(Icons.close_fullscreen, size: 18),
              label: const Text('Close All'),
            ),
          ],
        ),
        const SizedBox(height: 24),
        _SectionTitle(title: 'Lighting'),
        const SizedBox(height: 8),
        SwitchListTile(
          title: const Text('Welcome Lights Show'),
          value: _welcomeLights,
          onChanged: enabled
              ? (v) {
                  setState(() => _welcomeLights = v);
                  if (v) notifier.sendControl(BmwControlCmd.startLightShow);
                  else notifier.sendControl(BmwControlCmd.stopLightShow);
                }
              : null,
          activeColor: const Color(0xFF00A1E4),
        ),
        SwitchListTile(
          title: const Text('Follow Me Home'),
          value: false,
          onChanged: enabled ? (v) => notifier.sendControl(BmwControlCmd.followMeHome) : null,
          activeColor: const Color(0xFF00A1E4),
        ),
        const SizedBox(height: 12),
        SizedBox(
          width: double.infinity,
          child: FilledButton.icon(
            onPressed: enabled ? () => notifier.sendStrobeCommand() : null,
            icon: const Icon(Icons.flash_on, size: 24),
            label: const Text('Strobe Mode'),
            style: FilledButton.styleFrom(
              backgroundColor: mRed,
              foregroundColor: Colors.white,
              padding: const EdgeInsets.symmetric(vertical: 16),
            ),
          ),
        ),
        SwitchListTile(
          title: const Text('Sensory Dark Mode'),
          subtitle: const Text('Dim interior to 0%'),
          value: _sensoryDark,
          onChanged: enabled
              ? (v) {
                  setState(() => _sensoryDark = v);
                  notifier.sendSensoryDarkMode(v);
                }
              : null,
          activeColor: const Color(0xFF00A1E4),
        ),
        SwitchListTile(
          title: const Text('Mirror Fold on Lock'),
          value: _mirrorFoldOnLock,
          onChanged: enabled
              ? (v) {
                  setState(() => _mirrorFoldOnLock = v);
                  notifier.sendMirrorFoldPreference(v);
                }
              : null,
          activeColor: const Color(0xFF00A1E4),
        ),
        const SizedBox(height: 32),
      ],
    );
  }
}

class _SectionTitle extends StatelessWidget {
  final String title;

  const _SectionTitle({required this.title});

  @override
  Widget build(BuildContext context) {
    return Text(
      title,
      style: Theme.of(context).textTheme.titleMedium?.copyWith(
            color: Theme.of(context).colorScheme.secondary,
            fontWeight: FontWeight.w600,
          ),
    );
  }
}
