import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../ble/bmw_ble_provider.dart';
import '../ble/bmw_ble_constants.dart';

/// Dashboard: E39 wireframe vector and MD3 FABs for Lock, Unlock, Welcome Lights.
class DashboardScreen extends ConsumerWidget {
  const DashboardScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final ble = ref.watch(bmwBleStateProvider);
    final notifier = ref.read(bmwBleStateProvider.notifier);
    final enabled = ble.isConnected && ble.status.ibusSynced;
    final primary = Theme.of(context).colorScheme.primary;

    return Scaffold(
      body: CustomScrollView(
        slivers: [
          SliverAppBar(
            title: const Text('E39 Assistant'),
            actions: [
              if (ble.isConnected)
                IconButton(
                  icon: const Icon(Icons.bluetooth_connected),
                  onPressed: () => notifier.disconnect(),
                ),
            ],
          ),
          SliverToBoxAdapter(
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 16),
              child: Column(
                children: [
                  Text(
                    ble.isConnected
                        ? (ble.status.ibusSynced ? 'I-Bus OK' : 'I-Bus —')
                        : 'Not connected',
                    style: Theme.of(context).textTheme.labelLarge?.copyWith(
                          color: primary.withValues(alpha: 0.9),
                        ),
                  ),
                  const SizedBox(height: 24),
                  SizedBox(
                    height: 140,
                    child: CustomPaint(
                      painter: E39WireframePainter(
                        color: primary,
                        strokeWidth: 2.0,
                      ),
                      size: const Size(double.infinity, 140),
                    ),
                  ),
                  const SizedBox(height: 32),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      _DashboardFab(
                        icon: Icons.lock_open,
                        label: 'Unlock',
                        onPressed: enabled
                            ? () => notifier.sendControl(BmwControlCmd.unlock)
                            : null,
                      ),
                      const SizedBox(width: 20),
                      _DashboardFab(
                        icon: Icons.lock,
                        label: 'Lock',
                        onPressed: enabled
                            ? () => notifier.sendControl(BmwControlCmd.lock)
                            : null,
                      ),
                      const SizedBox(width: 20),
                      _DashboardFab(
                        icon: Icons.nightlight_round,
                        label: 'Welcome',
                        onPressed: enabled
                            ? () =>
                                notifier.sendControl(BmwControlCmd.followMeHome)
                            : null,
                      ),
                    ],
                  ),
                  const SizedBox(height: 24),
                  if (ble.error != null)
                    Padding(
                      padding: const EdgeInsets.only(top: 8),
                      child: Text(
                        ble.error!,
                        style: Theme.of(context).textTheme.bodySmall?.copyWith(
                              color: Theme.of(context).colorScheme.error,
                            ),
                        textAlign: TextAlign.center,
                      ),
                    ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _DashboardFab extends StatelessWidget {
  final IconData icon;
  final String label;
  final VoidCallback? onPressed;

  const _DashboardFab({
    required this.icon,
    required this.label,
    this.onPressed,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        FloatingActionButton(
          heroTag: label,
          onPressed: onPressed,
          child: Icon(icon),
        ),
        const SizedBox(height: 8),
        Text(
          label,
          style: Theme.of(context).textTheme.labelSmall?.copyWith(
                color: onPressed != null
                    ? Theme.of(context).colorScheme.onSurface
                    : Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.5),
              ),
        ),
      ],
    );
  }
}

/// Simple E39 side-profile wireframe: cabin, hood, trunk, wheels.
class E39WireframePainter extends CustomPainter {
  final Color color;
  final double strokeWidth;

  E39WireframePainter({required this.color, this.strokeWidth = 2.0});

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = color
      ..style = PaintingStyle.stroke
      ..strokeWidth = strokeWidth;

    final w = size.width;
    final h = size.height;
    final cx = w * 0.5;
    final top = h * 0.35;
    final bottom = h * 0.82;
    final wheelY = bottom - 4;
    final wheelR = 18.0;
    final leftWheelX = w * 0.22;
    final rightWheelX = w * 0.78;

    // Body outline (simplified sedan side)
    final path = Path();
    path.moveTo(w * 0.08, bottom);
    path.lineTo(w * 0.08, top + 12);
    path.quadraticBezierTo(w * 0.12, top - 4, w * 0.28, top);
    path.lineTo(w * 0.52, top);
    path.lineTo(w * 0.58, top + 6);
    path.lineTo(w * 0.72, top + 6);
    path.quadraticBezierTo(w * 0.88, top + 8, w * 0.92, bottom - 8);
    path.lineTo(w * 0.92, bottom);
    path.lineTo(w * 0.08, bottom);
    canvas.drawPath(path, paint);

    // Wheels
    canvas.drawCircle(Offset(leftWheelX, wheelY), wheelR, paint);
    canvas.drawCircle(Offset(rightWheelX, wheelY), wheelR, paint);
    canvas.drawLine(
      Offset(leftWheelX - wheelR, wheelY),
      Offset(leftWheelX + wheelR, wheelY),
      paint,
    );
    canvas.drawLine(
      Offset(leftWheelX, wheelY - wheelR),
      Offset(leftWheelX, wheelY + wheelR),
      paint,
    );
    canvas.drawLine(
      Offset(rightWheelX - wheelR, wheelY),
      Offset(rightWheelX + wheelR, wheelY),
      paint,
    );
    canvas.drawLine(
      Offset(rightWheelX, wheelY - wheelR),
      Offset(rightWheelX, wheelY + wheelR),
      paint,
    );
  }

  @override
  bool shouldRepaint(covariant E39WireframePainter oldDelegate) {
    return oldDelegate.color != color || oldDelegate.strokeWidth != strokeWidth;
  }
}
