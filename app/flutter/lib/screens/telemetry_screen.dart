import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../ble/bmw_ble_provider.dart';

/// Shift light threshold (RPM) — flashes M-Red when exceeded.
const int kShiftLightRpmThreshold = 5500;

/// Telemetry: RPM, Coolant Temp, Voltage, Shift Light; lock, ignition, odometer, PDC.
class TelemetryScreen extends ConsumerStatefulWidget {
  const TelemetryScreen({super.key});

  @override
  ConsumerState<TelemetryScreen> createState() => _TelemetryScreenState();
}

class _TelemetryScreenState extends ConsumerState<TelemetryScreen>
    with SingleTickerProviderStateMixin {
  late AnimationController _shiftLightController;

  @override
  void initState() {
    super.initState();
    _shiftLightController = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 400),
    )..repeat(reverse: true);
  }

  @override
  void dispose() {
    _shiftLightController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final ble = ref.watch(bmwBleStateProvider);
    final s = ble.status;
    final showShiftLight = s.rpm >= kShiftLightRpmThreshold;
    const mRed = Color(0xFFE21A23);

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Text(
          'Telemetry',
          style: Theme.of(context).textTheme.headlineSmall,
        ),
        const SizedBox(height: 8),
        Text(
          ble.isConnected
              ? 'I-Bus / ESP32 real-time'
              : 'Connect for data',
          style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                color: Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.7),
              ),
        ),
        const SizedBox(height: 20),
        _Tile(title: 'I-Bus', value: s.ibusSynced ? 'Synced' : '—'),
        _Tile(title: 'RPM', value: s.rpm >= 0 ? '${s.rpm}' : '—'),
        _Tile(title: 'Coolant', value: s.coolantC > -128 ? '${s.coolantC} °C' : '—'),
        _Tile(title: 'Voltage', value: '—'),
        if (showShiftLight) ...[
          const SizedBox(height: 16),
          AnimatedBuilder(
            animation: _shiftLightController,
            builder: (context, child) {
              return Container(
                padding: const EdgeInsets.symmetric(vertical: 12, horizontal: 16),
                decoration: BoxDecoration(
                  color: mRed.withValues(
                    alpha: 0.15 + 0.35 * _shiftLightController.value,
                  ),
                  border: Border.all(color: mRed, width: 2),
                  borderRadius: BorderRadius.circular(8),
                ),
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(Icons.warning_amber, color: mRed, size: 28),
                    const SizedBox(width: 12),
                    Text(
                      'SHIFT',
                      style: Theme.of(context).textTheme.titleLarge?.copyWith(
                            color: mRed,
                            fontWeight: FontWeight.bold,
                          ),
                    ),
                  ],
                ),
              );
            },
          ),
        ],
        const SizedBox(height: 16),
        _Tile(title: 'Oil', value: s.oilC > -128 ? '${s.oilC} °C' : '—'),
        _Tile(
          title: 'Lock',
          value: s.lockState == 0
              ? 'Unlocked'
              : s.lockState == 1
                  ? 'Locked'
                  : s.lockState == 2
                      ? 'Double'
                      : '—',
        ),
        _Tile(
          title: 'Ignition',
          value: s.ignition == 0
              ? 'Off'
              : s.ignition == 1
                  ? 'Pos 1'
                  : s.ignition == 2
                      ? 'Pos 2'
                      : '—',
        ),
        _Tile(title: 'Odometer', value: s.odometerKm >= 0 ? '${s.odometerKm} km' : '—'),
        if (s.pdcValid) ...[
          const SizedBox(height: 16),
          Text('PDC (cm)', style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 8),
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceEvenly,
            children: [
              _PdcTile('FL', s.pdcDists[0]),
              _PdcTile('FR', s.pdcDists[1]),
              _PdcTile('RL', s.pdcDists[2]),
              _PdcTile('RR', s.pdcDists[3]),
            ],
          ),
        ],
        const SizedBox(height: 24),
      ],
    );
  }
}

class _Tile extends StatelessWidget {
  final String title;
  final String value;

  const _Tile({required this.title, required this.value});

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: ListTile(
        title: Text(title),
        trailing: Text(value, style: Theme.of(context).textTheme.titleMedium),
      ),
    );
  }
}

class _PdcTile extends StatelessWidget {
  final String label;
  final int value;

  const _PdcTile(this.label, this.value);

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Text(label, style: Theme.of(context).textTheme.labelSmall),
        const SizedBox(height: 4),
        Text(value >= 0 ? '$value' : '—', style: Theme.of(context).textTheme.titleSmall),
        Icon(Icons.sensors, size: 20, color: Theme.of(context).colorScheme.primary),
      ],
    );
  }
}
