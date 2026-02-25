import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../theme/nocturne_theme.dart';
import '../ble/bmw_ble_provider.dart';
import '../ble/bmw_ble_constants.dart';
import 'screens/hero_screen.dart';
import 'screens/quick_actions_screen.dart';
import 'screens/telemetry_screen.dart';
import 'screens/media_screen.dart';

void main() {
  runApp(const ProviderScope(child: BmwAssistantApp()));
}

class BmwAssistantApp extends StatelessWidget {
  const BmwAssistantApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'My BMW',
      theme: nocturneNoirTheme,
      debugShowCheckedModeBanner: false,
      home: const HomeScaffold(),
    );
  }
}

class HomeScaffold extends ConsumerStatefulWidget {
  const HomeScaffold({super.key});

  @override
  ConsumerState<HomeScaffold> createState() => _HomeScaffoldState();
}

class _HomeScaffoldState extends ConsumerState<HomeScaffold> {
  int _index = 0;

  @override
  Widget build(BuildContext context) {
    final ble = ref.watch(bmwBleStateProvider);
    return Scaffold(
      body: IndexedStack(
        index: _index,
        children: const [
          HeroScreen(),
          QuickActionsScreen(),
          TelemetryScreen(),
          MediaScreen(),
        ],
      ),
      bottomNavigationBar: NavigationBar(
        selectedIndex: _index,
        onDestinationSelected: (i) => setState(() => _index = i),
        destinations: const [
          NavigationDestination(icon: Icon(Icons.dashboard), label: 'Статус'),
          NavigationDestination(icon: Icon(Icons.flash_on), label: 'Действия'),
          NavigationDestination(icon: Icon(Icons.speed), label: 'Телеметрия'),
          NavigationDestination(icon: Icon(Icons.music_note), label: 'Медиа'),
        ],
      ),
      floatingActionButton: ble.isConnected
          ? null
          : FloatingActionButton.extended(
              onPressed: ble.isConnecting
                  ? null
                  : () => ref.read(bmwBleStateProvider.notifier).scanAndConnect(),
              icon: ble.isConnecting
                  ? const SizedBox(
                      width: 20,
                      height: 20,
                      child: CircularProgressIndicator(strokeWidth: 2, color: Colors.black),
                    )
                  : const Icon(Icons.bluetooth_searching),
              label: Text(ble.isConnecting ? 'Поиск…' : 'Подключить'),
            ),
    );
  }
}
