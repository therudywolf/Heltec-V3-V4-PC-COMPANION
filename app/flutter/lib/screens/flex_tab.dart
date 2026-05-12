import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../ble/bmw_ble_provider.dart';
import '../ble/bmw_ble_constants.dart';

/// Tab 4: Flex — Wig-Wag, Comfort Blink, Valet Mode, Panic, Troll (IKE), Startup Greeting.
class FlexTab extends ConsumerStatefulWidget {
  const FlexTab({super.key});

  @override
  ConsumerState<FlexTab> createState() => _FlexTabState();
}

class _FlexTabState extends ConsumerState<FlexTab> {
  bool _comfortBlink = false;
  bool _valetMode = false;
  final _greetingController = TextEditingController();
  static const int _maxGreetingChars = 20;
  static const List<String> _trollPhrases = [
    'EJECT SEAT ARMED',
    'TURBO BOOST',
    'POLICE INTERCEPT',
    'SYSTEM FAILURE',
  ];

  @override
  void dispose() {
    _greetingController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final ble = ref.watch(bmwBleStateProvider);
    final notifier = ref.read(bmwBleStateProvider.notifier);
    final status = ble.status;
    final enabled = ble.isConnected && status.ibusSynced;
    const mRed = Color(0xFFE21A23);
    final showValetAlert = _valetMode && status.rpm > 4000;

    return Container(
      color: showValetAlert ? mRed.withValues(alpha: 0.15) : null,
      child: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          if (showValetAlert) ...[
            Card(
              color: mRed.withValues(alpha: 0.3),
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Row(
                  children: [
                    Icon(Icons.warning_amber, color: mRed, size: 28),
                    const SizedBox(width: 12),
                    Expanded(
                      child: Text(
                        'Valet alert: RPM > 4000',
                        style: Theme.of(context).textTheme.titleSmall?.copyWith(color: mRed, fontWeight: FontWeight.bold),
                      ),
                    ),
                  ],
                ),
              ),
            ),
            const SizedBox(height: 8),
          ],
          SizedBox(
            width: double.infinity,
            child: FilledButton.icon(
              onPressed: enabled ? () => notifier.sendStrobeCommand() : null,
              icon: const Icon(Icons.flash_on, size: 28),
              label: const Text('Wig-Wag Strobes'),
              style: FilledButton.styleFrom(
                backgroundColor: mRed,
                foregroundColor: Colors.white,
                padding: const EdgeInsets.symmetric(vertical: 20),
              ),
            ),
          ),
          const SizedBox(height: 16),
          SwitchListTile(
            title: const Text('Comfort Blink'),
            subtitle: const Text('1-touch = 3 blinks'),
            value: _comfortBlink,
            onChanged: enabled
                ? (v) {
                    setState(() => _comfortBlink = v);
                    notifier.sendComfortBlink(v);
                  }
                : null,
            activeColor: const Color(0xFF00A1E4),
          ),
          SwitchListTile(
            title: const Text('Valet Mode Alert'),
            subtitle: const Text('Red UI if RPM > 4000'),
            value: _valetMode,
            onChanged: (v) => setState(() => _valetMode = v),
            activeColor: const Color(0xFF00A1E4),
          ),
          const SizedBox(height: 12),
          SizedBox(
            width: double.infinity,
            child: FilledButton.icon(
              onPressed: enabled ? () => notifier.sendPanicCommand() : null,
              icon: const Icon(Icons.warning, size: 24),
              label: const Text('Panic Mode'),
              style: FilledButton.styleFrom(
                backgroundColor: mRed,
                foregroundColor: Colors.white,
                padding: const EdgeInsets.symmetric(vertical: 14),
              ),
            ),
          ),
          const SizedBox(height: 24),
          Text(
            'Troll Mode (IKE)',
            style: Theme.of(context).textTheme.titleSmall?.copyWith(color: Theme.of(context).colorScheme.primary),
          ),
          const SizedBox(height: 8),
          DropdownButtonFormField<String>(
            value: null,
            decoration: const InputDecoration(
              border: OutlineInputBorder(),
              filled: true,
              fillColor: Color(0xFF1E1E1E),
            ),
            hint: const Text('Select message to inject'),
            items: _trollPhrases.map((s) => DropdownMenuItem(value: s, child: Text(s))).toList(),
            onChanged: enabled
                ? (s) {
                    if (s != null && s.isNotEmpty) notifier.sendClusterText(s);
                  }
                : null,
          ),
          const SizedBox(height: 24),
          Text(
            'Startup Greeting',
            style: Theme.of(context).textTheme.titleSmall?.copyWith(color: Theme.of(context).colorScheme.primary),
          ),
          const SizedBox(height: 8),
          TextField(
            controller: _greetingController,
            maxLength: _maxGreetingChars,
            decoration: const InputDecoration(
              hintText: 'Text on ignition (max 20 chars)',
              border: OutlineInputBorder(),
              filled: true,
              fillColor: Color(0xFF1E1E1E),
            ),
            style: const TextStyle(fontFamily: 'monospace'),
          ),
          const SizedBox(height: 8),
          SizedBox(
            width: double.infinity,
            child: OutlinedButton.icon(
              onPressed: enabled && _greetingController.text.trim().isNotEmpty
                  ? () => notifier.sendStartupGreeting(_greetingController.text.trim())
                  : null,
              icon: const Icon(Icons.save),
              label: const Text('Save to ESP32'),
              style: OutlinedButton.styleFrom(
                foregroundColor: const Color(0xFF00A1E4),
                side: const BorderSide(color: Color(0xFF00A1E4)),
                padding: const EdgeInsets.symmetric(vertical: 12),
              ),
            ),
          ),
          const SizedBox(height: 32),
        ],
      ),
    );
  }
}
