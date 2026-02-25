import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../ble/bmw_ble_provider.dart';

/// Telemetry: RPM, coolant, oil, lock, ignition, odometer, PDC. Dashboard layout.
class TelemetryScreen extends ConsumerWidget {
  const TelemetryScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final ble = ref.watch(bmwBleStateProvider);
    final s = ble.status;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Text(
          'Телеметрия',
          style: Theme.of(context).textTheme.headlineSmall,
        ),
        const SizedBox(height: 8),
        Text(
          ble.isConnected ? 'Данные I-Bus / ESP32' : 'Подключитесь для получения данных',
          style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                color: Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.7),
              ),
        ),
        const SizedBox(height: 20),
        _Tile(title: 'I-Bus', value: s.ibusSynced ? 'Синхронизирован' : '—'),
        _Tile(title: 'RPM', value: s.rpm >= 0 ? '${s.rpm}' : '—'),
        _Tile(title: 'Охлаждающая', value: s.coolantC > -128 ? '${s.coolantC} °C' : '—'),
        _Tile(title: 'Масло', value: s.oilC > -128 ? '${s.oilC} °C' : '—'),
        _Tile(
          title: 'Замки',
          value: s.lockState == 0
              ? 'Открыто'
              : s.lockState == 1
                  ? 'Закрыто'
                  : s.lockState == 2
                      ? 'Двойное'
                      : '—',
        ),
        _Tile(
          title: 'Зажигание',
          value: s.ignition == 0
              ? 'Выкл'
              : s.ignition == 1
                  ? 'Pos 1'
                  : s.ignition == 2
                      ? 'Pos 2'
                      : '—',
        ),
        _Tile(title: 'Пробег', value: s.odometerKm >= 0 ? '${s.odometerKm} км' : '—'),
        if (s.pdcValid) ...[
          const SizedBox(height: 16),
          Text('PDC (см)', style: Theme.of(context).textTheme.titleMedium),
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
