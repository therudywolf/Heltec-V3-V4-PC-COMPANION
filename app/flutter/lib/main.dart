import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'theme/nocturne_theme.dart';
import 'ble/bmw_ble_provider.dart';
import 'screens/main_screen.dart';

void main() {
  runApp(const ProviderScope(child: MyE39App()));
}

class MyE39App extends StatelessWidget {
  const MyE39App({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'My E39',
      theme: nocturneNoirTheme,
      debugShowCheckedModeBanner: false,
      home: const MainScreen(),
    );
  }
}
