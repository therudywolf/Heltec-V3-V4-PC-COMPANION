import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../ble/bmw_ble_provider.dart';
import '../settings/e39_variant_provider.dart';

/// Dashboard: connection, E39 variant, lock, ignition, MFL, engine, PDC. No car pictogram.
class HeroScreen extends ConsumerWidget {
  const HeroScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final ble = ref.watch(bmwBleStateProvider);
    final e39 = ref.watch(e39VariantProvider);
    final e39Notifier = ref.read(e39VariantProvider.notifier);

    return CustomScrollView(
      slivers: [
        SliverAppBar(
          title: const Text('My BMW'),
          actions: [
            if (ble.isConnected)
              IconButton(
                icon: const Icon(Icons.bluetooth_connected),
                onPressed: () => ref.read(bmwBleStateProvider.notifier).disconnect(),
              ),
          ],
        ),
        SliverToBoxAdapter(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Text(
                  e39.label,
                  style: Theme.of(context).textTheme.titleMedium?.copyWith(
                        color: Theme.of(context).colorScheme.primary,
                      ),
                ),
                const SizedBox(height: 8),
                SegmentedButton<E39Variant>(
                  segments: const [
                    ButtonSegment(value: E39Variant.preFacelift, label: Text('E39 дорест')),
                    ButtonSegment(value: E39Variant.facelift, label: Text('E39 рест')),
                  ],
                  selected: {e39},
                  onSelectionChanged: (s) {
                    if (s.isNotEmpty) e39Notifier.setVariant(s.first);
                  },
                ),
                const SizedBox(height: 20),
                _StatusCard(
                  title: 'Подключение',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      _Row('BLE', ble.isConnected ? 'Подключено' : 'Нет'),
                      _Row('I-Bus', ble.status.ibusSynced ? 'Синхронизирован' : '—'),
                      if (ble.error != null)
                        Padding(
                          padding: const EdgeInsets.only(top: 8),
                          child: Text(
                            ble.error!,
                            style: TextStyle(color: Theme.of(context).colorScheme.error, fontSize: 12),
                          ),
                        ),
                    ],
                  ),
                ),
                if (!ble.isConnected)
                  Padding(
                    padding: const EdgeInsets.only(top: 8),
                    child: Text(
                      'Нажмите «Подключить» для поиска My BMW / BMW Key',
                      style: Theme.of(context).textTheme.bodySmall?.copyWith(
                            color: Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.7),
                          ),
                    ),
                  ),
                const SizedBox(height: 16),
                _StatusCard(
                  title: 'Замки',
                  child: Text(
                    _lockLabel(ble.status.lockState),
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                ),
                const SizedBox(height: 8),
                _StatusCard(
                  title: 'Зажигание',
                  child: Text(
                    _ignitionLabel(ble.status.ignition),
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                ),
                const SizedBox(height: 8),
                _StatusCard(
                  title: 'MFL',
                  child: Text(
                    _mflLabel(ble.status.lastMflAction),
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                ),
                const SizedBox(height: 16),
                Text('Двигатель', style: Theme.of(context).textTheme.titleSmall),
                const SizedBox(height: 4),
                Row(
                  children: [
                    Expanded(
                      child: _StatusCard(
                        title: 'RPM',
                        child: Text(
                          ble.status.rpm >= 0 ? '${ble.status.rpm}' : '—',
                          style: Theme.of(context).textTheme.titleMedium,
                        ),
                      ),
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: _StatusCard(
                        title: 'Охлажд.',
                        child: Text(
                          ble.status.coolantC > -128 ? '${ble.status.coolantC} °C' : '—',
                          style: Theme.of(context).textTheme.titleMedium,
                        ),
                      ),
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: _StatusCard(
                        title: 'Масло',
                        child: Text(
                          ble.status.oilC > -128 ? '${ble.status.oilC} °C' : '—',
                          style: Theme.of(context).textTheme.titleMedium,
                        ),
                      ),
                    ),
                  ],
                ),
                if (ble.status.pdcValid) ...[
                  const SizedBox(height: 16),
                  Text('PDC (см)', style: Theme.of(context).textTheme.titleSmall),
                  const SizedBox(height: 4),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceAround,
                    children: [
                      _PdcChip('FL', ble.status.pdcDists[0]),
                      _PdcChip('FR', ble.status.pdcDists[1]),
                      _PdcChip('RL', ble.status.pdcDists[2]),
                      _PdcChip('RR', ble.status.pdcDists[3]),
                    ],
                  ),
                ],
                const SizedBox(height: 20),
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(12),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          'О приложении',
                          style: Theme.of(context).textTheme.labelMedium?.copyWith(
                                color: Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.7),
                              ),
                        ),
                        const SizedBox(height: 4),
                        Text(
                          'My BMW — спутник для E39. Автор: RudyWolf.',
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                      ],
                    ),
                  ),
                ),
                const SizedBox(height: 24),
              ],
            ),
          ),
        ),
      ],
    );
  }

  static String _lockLabel(int s) {
    switch (s) {
      case 0: return 'Открыто';
      case 1: return 'Закрыто';
      case 2: return 'Двойное';
      default: return '—';
    }
  }

  static String _ignitionLabel(int s) {
    switch (s) {
      case 0: return 'Выкл';
      case 1: return 'Pos 1';
      case 2: return 'Pos 2';
      default: return '—';
    }
  }

  static String _mflLabel(int a) {
    switch (a) {
      case 1: return 'Next';
      case 2: return 'Prev';
      case 3: return 'Play/Pause';
      case 4: return 'Vol+';
      case 5: return 'Vol−';
      default: return '—';
    }
  }
}

class _StatusCard extends StatelessWidget {
  final String title;
  final Widget child;

  const _StatusCard({required this.title, required this.child});

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              title,
              style: Theme.of(context).textTheme.labelMedium?.copyWith(
                    color: Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.7),
                  ),
            ),
            const SizedBox(height: 4),
            child,
          ],
        ),
      ),
    );
  }
}

class _Row extends StatelessWidget {
  final String label;
  final String value;

  const _Row(this.label, this.value);

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 2),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [Text(label), Text(value)],
      ),
    );
  }
}

class _PdcChip extends StatelessWidget {
  final String label;
  final int value;

  const _PdcChip(this.label, this.value);

  @override
  Widget build(BuildContext context) {
    return Chip(
      label: Text('$label ${value >= 0 ? value : "—"}'),
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
    );
  }
}
