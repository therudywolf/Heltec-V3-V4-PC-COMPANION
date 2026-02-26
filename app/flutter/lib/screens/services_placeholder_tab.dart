import 'package:flutter/material.dart';

class ServicesPlaceholderTab extends StatelessWidget {
  const ServicesPlaceholderTab({super.key});

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Icon(Icons.build_circle, size: 64, color: Theme.of(context).colorScheme.primary.withValues(alpha: 0.6)),
          const SizedBox(height: 16),
          Text(
            'Services',
            style: Theme.of(context).textTheme.headlineSmall,
          ),
          const SizedBox(height: 8),
          Text(
            'E39 services placeholder',
            style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  color: Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.7),
                ),
          ),
        ],
      ),
    );
  }
}
