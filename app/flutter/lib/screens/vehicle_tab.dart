import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../ble/bmw_ble_provider.dart';
import '../ble/bmw_ble_constants.dart';

/// Vehicle tab: header, hero image, info row, action grid, telemetry cards.
class VehicleTab extends ConsumerWidget {
  const VehicleTab({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final ble = ref.watch(bmwBleStateProvider);
    final notifier = ref.read(bmwBleStateProvider.notifier);
    final status = ble.status;
    final enabled = ble.isConnected && status.ibusSynced;
    final primary = Theme.of(context).colorScheme.primary;
    final surface = Theme.of(context).colorScheme.surface;

    final statusLabel = _statusLabel(status.lockState, status.ibusSynced, ble.isConnected);
    final statusColor = _statusColor(status.lockState, status.ibusSynced);

    return CustomScrollView(
      slivers: [
        SliverToBoxAdapter(
          child: Padding(
            padding: const EdgeInsets.fromLTRB(20, 24, 20, 0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  'BMW 523i (E39)',
                  style: Theme.of(context).textTheme.headlineMedium?.copyWith(
                        fontWeight: FontWeight.bold,
                        color: Theme.of(context).colorScheme.onSurface,
                      ),
                ),
                const SizedBox(height: 10),
                _StatusBanner(label: statusLabel, color: statusColor),
                const SizedBox(height: 24),
                Center(
                  child: ClipRRect(
                    borderRadius: BorderRadius.circular(16),
                    child: Image.asset(
                      'assets/images/car_hero.png',
                      height: 160,
                      width: 200,
                      fit: BoxFit.contain,
                      errorBuilder: (_, __, ___) => _E39WireframePlaceholder(),
                    ),
                  ),
                ),
                const SizedBox(height: 24),
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                  children: [
                    _InfoColumn(label: 'Range', value: '-- km'),
                    _InfoColumn(label: 'Odometer', value: '320,000 km'),
                  ],
                ),
                const SizedBox(height: 24),
                Text(
                  'Actions',
                  style: Theme.of(context).textTheme.titleMedium?.copyWith(
                        color: primary,
                      ),
                ),
                const SizedBox(height: 12),
                SizedBox(
                  height: 100,
                  child: ListView(
                    scrollDirection: Axis.horizontal,
                    children: [
                      _ActionChip(
                        icon: Icons.lock,
                        label: 'Lock',
                        onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.lock) : null,
                      ),
                      _ActionChip(
                        icon: Icons.lock_open,
                        label: 'Unlock',
                        onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.unlock) : null,
                      ),
                      _ActionChip(
                        icon: Icons.flash_on,
                        label: 'Flash Lights',
                        onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.hazardLights) : null,
                      ),
                      _ActionChip(
                        icon: Icons.air,
                        label: 'Ventilation',
                        onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.interiorOn3s) : null,
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 24),
                Text(
                  'Telemetry',
                  style: Theme.of(context).textTheme.titleMedium?.copyWith(
                        color: primary,
                      ),
                ),
                const SizedBox(height: 12),
              ],
            ),
          ),
        ),
        SliverList(
          delegate: SliverChildListDelegate([
            _TelemetryCard(
              title: 'Vehicle Status (Doors / Windows)',
              child: _VehicleStatusContent(status: status),
            ),
            _TelemetryCard(
              title: 'Engine Vitals (RPM, Coolant)',
              child: _EngineVitalsContent(status: status),
            ),
            _TelemetryCard(
              title: 'Check Control Messages',
              child: _CheckControlContent(status: status),
            ),
            const SizedBox(height: 32),
          ]),
        ),
      ],
    );
  }

  static String _statusLabel(int lockState, bool ibusSynced, bool connected) {
    if (!connected) return 'NOT CONNECTED';
    if (!ibusSynced) return 'CONNECTING…';
    switch (lockState) {
      case 0: return 'ALL GOOD';
      case 1: return 'LOCKED';
      case 2: return 'DOUBLE LOCKED';
      default: return '--';
    }
  }

  static Color _statusColor(int lockState, bool ibusSynced) {
    if (!ibusSynced) return const Color(0xFF00A1E4);
    if (lockState == 0) return const Color(0xFF4CAF50);
    return const Color(0xFFE21A23);
  }
}

class _StatusBanner extends StatelessWidget {
  final String label;
  final Color color;

  const _StatusBanner({required this.label, required this.color});

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.symmetric(vertical: 10, horizontal: 16),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.2),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: color, width: 1),
      ),
      child: Text(
        label,
        style: Theme.of(context).textTheme.titleSmall?.copyWith(
              color: color,
              fontWeight: FontWeight.w600,
              letterSpacing: 0.5,
            ),
      ),
    );
  }
}

class _E39WireframePlaceholder extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    const mRed = Color(0xFFE21A23);
    return Container(
      height: 160,
      width: 200,
      decoration: BoxDecoration(
        color: const Color(0xFF1E1E1E),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: mRed.withValues(alpha: 0.5)),
      ),
      child: CustomPaint(
        painter: _E39WireframePainter(color: mRed),
        size: const Size(200, 160),
      ),
    );
  }
}

class _E39WireframePainter extends CustomPainter {
  final Color color;

  _E39WireframePainter({required this.color});

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = color
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2;
    final w = size.width;
    final h = size.height;
    final path = Path();
    path.moveTo(w * 0.1, h * 0.75);
    path.lineTo(w * 0.1, h * 0.4);
    path.quadraticBezierTo(w * 0.2, h * 0.25, w * 0.45, h * 0.3);
    path.lineTo(w * 0.55, h * 0.3);
    path.lineTo(w * 0.6, h * 0.38);
    path.lineTo(w * 0.8, h * 0.38);
    path.quadraticBezierTo(w * 0.92, h * 0.42, w * 0.9, h * 0.75);
    path.lineTo(w * 0.1, h * 0.75);
    canvas.drawPath(path, paint);
    canvas.drawCircle(Offset(w * 0.28, h * 0.78), 14, paint);
    canvas.drawCircle(Offset(w * 0.72, h * 0.78), 14, paint);
  }

  @override
  bool shouldRepaint(covariant _E39WireframePainter old) => old.color != color;
}

class _InfoColumn extends StatelessWidget {
  final String label;
  final String value;

  const _InfoColumn({required this.label, required this.value});

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Text(
          label,
          style: Theme.of(context).textTheme.labelMedium?.copyWith(
                color: Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.7),
              ),
        ),
        const SizedBox(height: 4),
        Text(
          value,
          style: Theme.of(context).textTheme.titleLarge?.copyWith(
                fontFamily: 'monospace',
              ),
        ),
      ],
    );
  }
}

class _ActionChip extends StatelessWidget {
  final IconData icon;
  final String label;
  final VoidCallback? onPressed;

  const _ActionChip({required this.icon, required this.label, this.onPressed});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(right: 12),
      child: Material(
        color: Theme.of(context).colorScheme.surface,
        borderRadius: BorderRadius.circular(16),
        child: InkWell(
          onTap: onPressed,
          borderRadius: BorderRadius.circular(16),
          child: Container(
            width: 100,
            padding: const EdgeInsets.symmetric(vertical: 12, horizontal: 8),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Icon(
                  icon,
                  size: 28,
                  color: onPressed != null
                      ? Theme.of(context).colorScheme.primary
                      : Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.4),
                ),
                const SizedBox(height: 6),
                Text(
                  label,
                  style: Theme.of(context).textTheme.labelSmall?.copyWith(
                        color: onPressed != null
                            ? Theme.of(context).colorScheme.onSurface
                            : Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.5),
                      ),
                  textAlign: TextAlign.center,
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _TelemetryCard extends StatelessWidget {
  final String title;
  final Widget child;

  const _TelemetryCard({required this.title, required this.child});

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.fromLTRB(20, 0, 20, 12),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              title,
              style: Theme.of(context).textTheme.titleSmall?.copyWith(
                    color: Theme.of(context).colorScheme.primary,
                  ),
            ),
            const SizedBox(height: 12),
            child,
          ],
        ),
      ),
    );
  }
}

class _VehicleStatusContent extends StatelessWidget {
  final BmwStatus status;

  const _VehicleStatusContent({required this.status});

  @override
  Widget build(BuildContext context) {
    final lock = status.lockState == 0
        ? 'Unlocked'
        : status.lockState == 1
            ? 'Locked'
            : status.lockState == 2
                ? 'Double locked'
                : '--';
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _RowLabel('Doors', lock),
        _RowLabel('Windows', '--'),
        _RowLabel('Ignition', status.ignition == 0 ? 'Off' : status.ignition == 1 ? 'Pos 1' : status.ignition == 2 ? 'Pos 2' : '--'),
      ],
    );
  }
}

class _EngineVitalsContent extends StatelessWidget {
  final BmwStatus status;

  const _EngineVitalsContent({required this.status});

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _RowLabel('RPM', status.rpm >= 0 ? '${status.rpm}' : '--'),
        _RowLabel('Coolant', status.coolantC > -128 ? '${status.coolantC} °C' : '--'),
        _RowLabel('Oil', status.oilC > -128 ? '${status.oilC} °C' : '--'),
      ],
    );
  }
}

class _CheckControlContent extends StatelessWidget {
  final BmwStatus status;

  const _CheckControlContent({required this.status});

  @override
  Widget build(BuildContext context) {
    final messages = <String>[];
    if (!status.ibusSynced) messages.add('I-Bus not synced');
    if (status.odometerKm >= 0 && status.odometerKm > 300000) {
      messages.add('High mileage: ${status.odometerKm} km');
    }
    if (messages.isEmpty) messages.add('No messages');
    return Text(
      messages.join(' · '),
      style: Theme.of(context).textTheme.bodyMedium?.copyWith(
            color: Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.9),
          ),
    );
  }
}

class _RowLabel extends StatelessWidget {
  final String label;
  final String value;

  const _RowLabel(this.label, this.value);

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 6),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: Theme.of(context).textTheme.bodyMedium),
          Text(value, style: Theme.of(context).textTheme.bodyMedium?.copyWith(fontFamily: 'monospace')),
        ],
      ),
    );
  }
}
