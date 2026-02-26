import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../ble/bmw_ble_provider.dart';
import '../ble/bmw_ble_constants.dart';
import '../widgets/shift_light.dart';

/// Tab 1: Dashboard — Hero image, vitals grid (metric), Shift Light, Lock/Unlock.
class DashboardTab extends ConsumerWidget {
  const DashboardTab({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final ble = ref.watch(bmwBleStateProvider);
    final notifier = ref.read(bmwBleStateProvider.notifier);
    final status = ble.status;
    final enabled = ble.isConnected && status.ibusSynced;
    final screenH = MediaQuery.of(context).size.height;
    final heroH = screenH * 0.35;

    return CustomScrollView(
      slivers: [
        SliverToBoxAdapter(
          child: Padding(
            padding: const EdgeInsets.fromLTRB(16, 16, 16, 0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                _StatusChip(connected: ble.isConnected, synced: status.ibusSynced),
                const SizedBox(height: 16),
                Center(
                  child: FractionallySizedBox(
                    widthFactor: 0.9,
                    child: SizedBox(
                      height: heroH,
                      child: ClipRRect(
                        borderRadius: BorderRadius.circular(16),
                        child: Image.asset(
                          'assets/images/car_hero.png',
                          fit: BoxFit.contain,
                          errorBuilder: (_, __, ___) => _Placeholder(height: heroH),
                        ),
                      ),
                    ),
                  ),
                ),
                const SizedBox(height: 20),
                _VitalsGrid(status: status),
                const SizedBox(height: 12),
                ShiftLightWidget(rpm: status.rpm >= 0 ? status.rpm : 0),
                const SizedBox(height: 20),
                Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    _LockButton(
                      label: 'Lock',
                      icon: Icons.lock,
                      onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.lock) : null,
                    ),
                    const SizedBox(width: 16),
                    _LockButton(
                      label: 'Unlock',
                      icon: Icons.lock_open,
                      onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.unlock) : null,
                    ),
                  ],
                ),
                const SizedBox(height: 32),
              ],
            ),
          ),
        ),
      ],
    );
  }
}

class _StatusChip extends StatelessWidget {
  final bool connected;
  final bool synced;

  const _StatusChip({required this.connected, required this.synced});

  @override
  Widget build(BuildContext context) {
    final label = !connected ? 'NOT CONNECTED' : !synced ? 'CONNECTING…' : 'READY';
    final color = !connected ? const Color(0xFF616161)
        : !synced ? const Color(0xFF00A1E4) : const Color(0xFF4CAF50);
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.2),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: color),
      ),
      child: Text(label, style: Theme.of(context).textTheme.labelLarge?.copyWith(color: color, fontWeight: FontWeight.w600)),
    );
  }
}

class _VitalsGrid extends StatelessWidget {
  final BmwStatus status;

  const _VitalsGrid({required this.status});

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Vitals', style: Theme.of(context).textTheme.titleSmall?.copyWith(color: Theme.of(context).colorScheme.primary)),
            const SizedBox(height: 12),
            Wrap(
              spacing: 16,
              runSpacing: 12,
              children: [
                _VitalChip(label: 'RPM', value: status.rpm >= 0 ? '${status.rpm}' : '--'),
                _VitalChip(label: 'Coolant', value: status.coolantC > -128 ? '${status.coolantC} °C' : '--'),
                _VitalChip(label: 'Speed', value: '-- km/h'),
                _VitalChip(label: 'Battery', value: '-- V'),
                _VitalChip(label: 'Fuel', value: '-- L'),
                _VitalChip(label: '0–100', value: '-- s'),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _VitalChip extends StatelessWidget {
  final String label;
  final String value;

  const _VitalChip({required this.label, required this.value});

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(label, style: Theme.of(context).textTheme.labelSmall?.copyWith(color: Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.7))),
        const SizedBox(height: 2),
        Text(value, style: Theme.of(context).textTheme.titleSmall?.copyWith(fontFamily: 'monospace')),
      ],
    );
  }
}

class _LockButton extends StatelessWidget {
  final String label;
  final IconData icon;
  final VoidCallback? onPressed;

  const _LockButton({required this.label, required this.icon, this.onPressed});

  @override
  Widget build(BuildContext context) {
    return FilledButton.icon(
      onPressed: onPressed,
      icon: Icon(icon, size: 20),
      label: Text(label),
      style: FilledButton.styleFrom(
        backgroundColor: const Color(0xFFE21A23),
        foregroundColor: Colors.white,
        padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
      ),
    );
  }
}

class _Placeholder extends StatelessWidget {
  final double height;

  const _Placeholder({required this.height});

  @override
  Widget build(BuildContext context) {
    return Container(
      height: height,
      decoration: BoxDecoration(
        color: const Color(0xFF1E1E1E),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: const Color(0xFFE21A23).withValues(alpha: 0.5)),
      ),
      child: const Center(child: Icon(Icons.directions_car, size: 48, color: Color(0xFF616161))),
    );
  }
}
