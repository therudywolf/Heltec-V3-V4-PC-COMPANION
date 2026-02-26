import 'package:flutter/material.dart';

/// M-Red flashing container when RPM exceeds threshold. Reusable across Dashboard/Telemetry.
const int kShiftLightRpmThreshold = 5500;

class ShiftLightWidget extends StatefulWidget {
  final int rpm;

  const ShiftLightWidget({super.key, required this.rpm});

  @override
  State<ShiftLightWidget> createState() => _ShiftLightWidgetState();
}

class _ShiftLightWidgetState extends State<ShiftLightWidget>
    with SingleTickerProviderStateMixin {
  late AnimationController _controller;

  @override
  void initState() {
    super.initState();
    _controller = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 400),
    )..repeat(reverse: true);
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final show = widget.rpm >= kShiftLightRpmThreshold;
    const mRed = Color(0xFFE21A23);
    if (!show) return const SizedBox.shrink();
    return AnimatedBuilder(
      animation: _controller,
      builder: (context, child) {
        return Container(
          width: double.infinity,
          padding: const EdgeInsets.symmetric(vertical: 12, horizontal: 16),
          decoration: BoxDecoration(
            color: mRed.withValues(
              alpha: 0.15 + 0.35 * _controller.value,
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
                      fontFamily: 'monospace',
                    ),
              ),
            ],
          ),
        );
      },
    );
  }
}
