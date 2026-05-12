import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../ble/bmw_ble_provider.dart';

/// Tab 5: Sonar — PDC radar (4 sensors), color-coded by distance (cm): green / yellow / red.
class SonarTab extends ConsumerWidget {
  const SonarTab({super.key});

  static const int _greenCm = 80;
  static const int _yellowCm = 40;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final ble = ref.watch(bmwBleStateProvider);
    final status = ble.status;
    final dists = status.pdcDists;
    const labels = ['FL', 'FR', 'RL', 'RR'];

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Text(
          'PDC Radar (cm)',
          style: Theme.of(context).textTheme.titleMedium?.copyWith(
                color: Theme.of(context).colorScheme.secondary,
                fontWeight: FontWeight.w600,
              ),
        ),
        const SizedBox(height: 8),
        if (!status.pdcValid)
          Card(
            child: Padding(
              padding: const EdgeInsets.all(24),
              child: Center(
                child: Text(
                  'No PDC data — connect to car',
                  style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                        color: Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.7),
                      ),
                ),
              ),
            ),
          )
        else
          Card(
            child: Padding(
              padding: const EdgeInsets.all(20),
              child: Column(
                children: [
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                    children: [
                      _PdcIndicator(label: labels[0], cm: dists[0]),
                      _PdcIndicator(label: labels[1], cm: dists[1]),
                    ],
                  ),
                  const SizedBox(height: 24),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                    children: [
                      _PdcIndicator(label: labels[2], cm: dists[2]),
                      _PdcIndicator(label: labels[3], cm: dists[3]),
                    ],
                  ),
                  const SizedBox(height: 16),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      _LegendDot(color: const Color(0xFF4CAF50), label: '>$_greenCm cm'),
                      const SizedBox(width: 16),
                      _LegendDot(color: const Color(0xFFFFC107), label: '$_yellowCm–$_greenCm'),
                      const SizedBox(width: 16),
                      _LegendDot(color: const Color(0xFFE21A23), label: '<$_yellowCm cm'),
                    ],
                  ),
                ],
              ),
            ),
          ),
        const SizedBox(height: 32),
      ],
    );
  }

  static Color _colorForCm(int cm) {
    if (cm < 0) return const Color(0xFF616161);
    if (cm < _yellowCm) return const Color(0xFFE21A23);
    if (cm < _greenCm) return const Color(0xFFFFC107);
    return const Color(0xFF4CAF50);
  }
}

class _PdcIndicator extends StatelessWidget {
  final String label;
  final int cm;

  const _PdcIndicator({required this.label, required this.cm});

  @override
  Widget build(BuildContext context) {
    final color = SonarTab._colorForCm(cm);
    return Column(
      children: [
        Container(
          width: 56,
          height: 56,
          decoration: BoxDecoration(
            color: color.withValues(alpha: 0.3),
            shape: BoxShape.circle,
            border: Border.all(color: color, width: 2),
          ),
          child: Center(
            child: Text(
              cm >= 0 ? '$cm' : '—',
              style: Theme.of(context).textTheme.titleMedium?.copyWith(
                    fontFamily: 'monospace',
                    color: color,
                    fontWeight: FontWeight.bold,
                  ),
            ),
          ),
        ),
        const SizedBox(height: 6),
        Text(label, style: Theme.of(context).textTheme.labelSmall),
      ],
    );
  }
}

class _LegendDot extends StatelessWidget {
  final Color color;
  final String label;

  const _LegendDot({required this.color, required this.label});

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Container(width: 10, height: 10, decoration: BoxDecoration(color: color, shape: BoxShape.circle)),
        const SizedBox(width: 6),
        Text(label, style: Theme.of(context).textTheme.labelSmall),
      ],
    );
  }
}
