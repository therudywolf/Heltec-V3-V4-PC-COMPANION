import 'package:flutter/material.dart';

class MapPlaceholderTab extends StatelessWidget {
  const MapPlaceholderTab({super.key});

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Icon(Icons.map, size: 64, color: Theme.of(context).colorScheme.primary.withValues(alpha: 0.6)),
          const SizedBox(height: 16),
          Text(
            'Map',
            style: Theme.of(context).textTheme.headlineSmall,
          ),
          const SizedBox(height: 8),
          Text(
            'E39 navigation placeholder',
            style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  color: Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.7),
                ),
          ),
        ],
      ),
    );
  }
}
