import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../ble/bmw_ble_provider.dart';
import '../ble/bmw_ble_constants.dart';

/// Quick actions: Lock, Lights, Windows, Wipers, Interior, Light show. Grouped.
class QuickActionsScreen extends ConsumerWidget {
  const QuickActionsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final ble = ref.watch(bmwBleStateProvider);
    final notifier = ref.read(bmwBleStateProvider.notifier);
    final enabled = ble.isConnected && ble.status.ibusSynced;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Text(
          'Действия',
          style: Theme.of(context).textTheme.headlineSmall,
        ),
        const SizedBox(height: 8),
        Text(
          enabled
              ? 'Отправка команд в автомобиль'
              : 'Подключитесь и дождитесь синхронизации I-Bus',
          style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                color: Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.7),
              ),
        ),
        const SizedBox(height: 20),
        _SectionTitle('Замки'),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            _ActionChip(label: 'Открыть', icon: Icons.lock_open, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.unlock) : null),
            _ActionChip(label: 'Закрыть', icon: Icons.lock, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.lock) : null),
            _ActionChip(label: 'Центр. откр.', icon: Icons.door_front_door, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.doorsUnlockGM) : null),
            _ActionChip(label: 'Салон откр.', icon: Icons.door_sliding, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.doorsUnlockInterior) : null),
            _ActionChip(label: 'Багажник', icon: Icons.directions_car, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.trunkOpen) : null),
            _ActionChip(label: 'Двойное', icon: Icons.lock, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.doorsHardLock) : null),
            _ActionChip(label: 'Кроме вод.', icon: Icons.lock, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.allExceptDriverLock) : null),
            _ActionChip(label: 'Вод. водит.', icon: Icons.lock, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.driverDoorLock) : null),
            _ActionChip(label: 'Замок люка', icon: Icons.local_gas_station, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.doorsFuelTrunk) : null),
          ],
        ),
        const SizedBox(height: 16),
        _SectionTitle('Свет'),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            _ActionChip(label: 'Прощай', icon: Icons.lightbulb_outline, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.goodbyeLights) : null),
            _ActionChip(label: 'Проводи', icon: Icons.nightlight_round, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.followMeHome) : null),
            _ActionChip(label: 'Парковый', icon: Icons.lightbulb_outline, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.parkLights) : null),
            _ActionChip(label: 'Аварийка', icon: Icons.warning_amber, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.hazardLights) : null),
            _ActionChip(label: 'Ближний', icon: Icons.lightbulb, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.lowBeams) : null),
            _ActionChip(label: 'Выкл', icon: Icons.lightbulb, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.lightsOff) : null),
            _ActionChip(label: 'Клоун', icon: Icons.flash_on, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.clownFlash) : null),
            _ActionChip(label: 'Шоу старт', icon: Icons.play_arrow, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.startLightShow) : null),
            _ActionChip(label: 'Шоу стоп', icon: Icons.stop, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.stopLightShow) : null),
          ],
        ),
        const SizedBox(height: 16),
        _SectionTitle('Окна'),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            _ActionChip(label: 'Вод. откр.', icon: Icons.door_front_door, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.windowFrontDriverOpen) : null),
            _ActionChip(label: 'Вод. закр.', icon: Icons.door_front_door, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.windowFrontDriverClose) : null),
            _ActionChip(label: 'Пас. откр.', icon: Icons.door_front_door, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.windowFrontPassengerOpen) : null),
            _ActionChip(label: 'Пас. закр.', icon: Icons.door_front_door, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.windowFrontPassengerClose) : null),
            _ActionChip(label: 'Зад. вод. откр.', icon: Icons.door_front_door, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.windowRearDriverOpen) : null),
            _ActionChip(label: 'Зад. вод. закр.', icon: Icons.door_front_door, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.windowRearDriverClose) : null),
            _ActionChip(label: 'Зад. пас. откр.', icon: Icons.door_front_door, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.windowRearPassengerOpen) : null),
            _ActionChip(label: 'Зад. пас. закр.', icon: Icons.door_front_door, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.windowRearPassengerClose) : null),
          ],
        ),
        const SizedBox(height: 16),
        _SectionTitle('Прочее'),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            _ActionChip(label: 'Дворники', icon: Icons.water_drop, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.wipersFront) : null),
            _ActionChip(label: 'Омыватель', icon: Icons.clean_hands, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.washerFront) : null),
            _ActionChip(label: 'Салон выкл', icon: Icons.lightbulb_outline, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.interiorOff) : null),
            _ActionChip(label: 'Салон 3 с', icon: Icons.lightbulb, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.interiorOn3s) : null),
            _ActionChip(label: 'NOCT', icon: Icons.text_fields, onPressed: enabled ? () => notifier.sendControl(BmwControlCmd.clusterNoct) : null),
          ],
        ),
        const SizedBox(height: 24),
      ],
    );
  }
}

class _SectionTitle extends StatelessWidget {
  final String title;

  const _SectionTitle(this.title);

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Text(
        title,
        style: Theme.of(context).textTheme.titleMedium?.copyWith(
              color: Theme.of(context).colorScheme.primary,
            ),
      ),
    );
  }
}

class _ActionChip extends StatelessWidget {
  final String label;
  final IconData icon;
  final VoidCallback? onPressed;

  const _ActionChip({required this.label, required this.icon, this.onPressed});

  @override
  Widget build(BuildContext context) {
    return FilterChip(
      label: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(icon, size: 18, color: onPressed != null ? null : Theme.of(context).colorScheme.onSurface.withValues(alpha: 0.5)),
          const SizedBox(width: 6),
          Text(label),
        ],
      ),
      onSelected: onPressed != null ? (_) => onPressed!() : null,
      selected: false,
    );
  }
}
