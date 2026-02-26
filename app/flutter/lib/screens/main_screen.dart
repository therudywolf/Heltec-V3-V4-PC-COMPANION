import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../theme/nocturne_theme.dart';
import '../ble/bmw_ble_provider.dart';
import '../ble/bmw_ble_constants.dart';
import 'dashboard_tab.dart';
import 'instincts_tab.dart';
import 'voice_tab.dart';
import 'flex_tab.dart';
import 'sonar_tab.dart';

/// M-Power Noir: 5-tab navigation — Dashboard, Instincts, Voice, Flex, Sonar.
class MainScreen extends ConsumerStatefulWidget {
  const MainScreen({super.key});

  @override
  ConsumerState<MainScreen> createState() => _MainScreenState();
}

class _MainScreenState extends ConsumerState<MainScreen> {
  int _currentIndex = 0;

  static const List<NavigationDestination> _destinations = [
    NavigationDestination(icon: Icon(Icons.dashboard), label: 'Dashboard'),
    NavigationDestination(icon: Icon(Icons.tune), label: 'Instincts'),
    NavigationDestination(icon: Icon(Icons.music_note), label: 'Voice'),
    NavigationDestination(icon: Icon(Icons.flash_on), label: 'Flex'),
    NavigationDestination(icon: Icon(Icons.radar), label: 'Sonar'),
  ];

  @override
  Widget build(BuildContext context) {
    final ble = ref.watch(bmwBleStateProvider);
    return Scaffold(
      body: IndexedStack(
        index: _currentIndex,
        children: const [
          DashboardTab(),
          InstinctsTab(),
          VoiceTab(),
          FlexTab(),
          SonarTab(),
        ],
      ),
      bottomNavigationBar: NavigationBar(
        selectedIndex: _currentIndex,
        onDestinationSelected: (i) => setState(() => _currentIndex = i),
        destinations: _destinations,
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
                      child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white),
                    )
                  : const Icon(Icons.bluetooth_searching),
              label: Text(ble.isConnecting ? 'Connecting…' : 'Connect car'),
            ),
    );
  }
}
