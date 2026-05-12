import 'dart:async';
import 'dart:convert';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'bmw_ble_constants.dart';
import '../debug_log.dart';

/// Parsed status packet from ESP32 (16 bytes).
class BmwStatus {
  final bool ibusSynced;
  final bool phoneConnected;
  final bool pdcValid;
  final bool obdConnected;
  final int coolantC;
  final int oilC;
  final int rpm;
  final List<int> pdcDists; // 4 values, -1 = invalid
  final int lastMflAction; // 0=none, 1=next, 2=prev, 3=play_pause, 4=vol_up, 5=vol_down
  final int lockState; // 0=unlocked, 1=locked, 2=double, 0xFF=unknown
  final int ignition; // 0=off, 1=pos1, 2=pos2, -1=unknown
  final int odometerKm;

  const BmwStatus({
    this.ibusSynced = false,
    this.phoneConnected = false,
    this.pdcValid = false,
    this.obdConnected = false,
    this.coolantC = -128,
    this.oilC = -128,
    this.rpm = -1,
    this.pdcDists = const [-1, -1, -1, -1],
    this.lastMflAction = 0,
    this.lockState = 0xFF,
    this.ignition = -1,
    this.odometerKm = -1,
  });

  static BmwStatus fromBytes(List<int> buf) {
    if (buf.length < 16) return const BmwStatus();
    return BmwStatus(
      ibusSynced: (buf[0] & 0x01) != 0,
      phoneConnected: (buf[0] & 0x02) != 0,
      pdcValid: (buf[0] & 0x04) != 0,
      obdConnected: (buf[0] & 0x08) != 0,
      coolantC: buf[1] <= 127 ? buf[1] : -128,
      oilC: buf[2] <= 127 ? buf[2] : -128,
      rpm: buf[3] != 0xFF || buf[4] != 0xFF ? ((buf[3] << 8) | buf[4]) : -1,
      pdcDists: [
        buf[5] <= 254 ? buf[5] : -1,
        buf[6] <= 254 ? buf[6] : -1,
        buf[7] <= 254 ? buf[7] : -1,
        buf[8] <= 254 ? buf[8] : -1,
      ],
      lastMflAction: buf[9] <= 5 ? buf[9] : 0,
      lockState: buf[12] <= 2 ? buf[12] : 0xFF,
      ignition: buf[13] <= 3 ? buf[13] : -1,
      odometerKm: (buf[14] != 0xFF || buf[15] != 0xFF) ? (buf[14] | (buf[15] << 8)) : -1,
    );
  }
}

/// BLE device + connection state.
class BmwBleState {
  final BluetoothDevice? device;
  final bool isConnecting;
  final bool isConnected;
  final BmwStatus status;
  final String? error;

  const BmwBleState({
    this.device,
    this.isConnecting = false,
    this.isConnected = false,
    this.status = const BmwStatus(),
    this.error,
  });
}

final bmwBleStateProvider = StateNotifierProvider<BmwBleNotifier, BmwBleState>((ref) {
  return BmwBleNotifier();
});

class BmwBleNotifier extends StateNotifier<BmwBleState> {
  StreamSubscription? _connSub;
  StreamSubscription? _statusSub;
  BluetoothCharacteristic? _controlChar;
  BluetoothCharacteristic? _statusChar;
  BluetoothCharacteristic? _nowPlayingChar;
  BluetoothCharacteristic? _clusterTextChar;

  BmwBleNotifier() : super(const BmwBleState()) {
    _listenConnection();
  }

  void _listenConnection() {
    _connSub?.cancel();
    _connSub = FlutterBluePlus.events.onConnectionStateChanged.listen((event) async {
      if (event.device.remoteId != state.device?.remoteId) return;
      final connected = event.connectionState == BluetoothConnectionState.connected;
      if (connected) {
        try {
          await _discoverAndSubscribe(event.device);
        } catch (e) {
          state = state.copyWith(isConnecting: false, isConnected: false, error: e.toString());
          return;
        }
      } else {
        _controlChar = null;
        _statusChar = null;
        _nowPlayingChar = null;
        _clusterTextChar = null;
        _statusSub?.cancel();
        _statusSub = null;
      }
      state = state.copyWith(isConnecting: false, isConnected: connected, error: null);
    });
  }

  Future<void> _discoverAndSubscribe(BluetoothDevice device) async {
    // #region agent log
    debugLog('bmw_ble_provider.dart:_discoverAndSubscribe', 'entry', data: {'deviceId': device.remoteId.toString()}, hypothesisId: 'H3');
    // #endregion
    try {
      final services = await device.discoverServices();
      // #region agent log
      debugLog('bmw_ble_provider.dart:_discoverAndSubscribe', 'services discovered', data: {'count': services.length, 'uuids': services.map((s) => s.uuid.toString()).toList()}, hypothesisId: 'H3');
      // #endregion
      for (final s in services) {
        if (s.uuid.toString().toLowerCase() != BmwBleUuids.service) continue;
        for (final c in s.characteristics) {
          final uuid = c.uuid.toString().toLowerCase();
          if (uuid == BmwBleUuids.control) _controlChar = c;
          if (uuid == BmwBleUuids.status) {
            _statusChar = c;
            try {
              await c.setNotifyValue(true);
              // #region agent log
              debugLog('bmw_ble_provider.dart:_discoverAndSubscribe', 'status notify enabled', hypothesisId: 'H4');
              // #endregion
            } catch (e) {
              // #region agent log
              debugLog('bmw_ble_provider.dart:_discoverAndSubscribe', 'setNotifyValue failed', data: {'error': e.toString()}, hypothesisId: 'H4');
              // #endregion
            }
            _statusSub?.cancel();
            _statusSub = c.lastValueStream.listen((value) {
              // #region agent log
              if (value.isNotEmpty) debugLog('bmw_ble_provider.dart:statusStream', 'status received', data: {'len': value.length, 'flags': value[0], 'ibusSynced': (value[0] & 0x01) != 0}, hypothesisId: 'H5');
              // #endregion
              if (value.length >= 16) {
                state = state.copyWith(
                  status: BmwStatus.fromBytes(value),
                  error: null,
                );
              }
            });
            final last = await c.read();
            if (last.length >= 16) {
              // #region agent log
              debugLog('bmw_ble_provider.dart:_discoverAndSubscribe', 'initial status read', data: {'len': last.length, 'flags': last[0], 'ibusSynced': (last[0] & 0x01) != 0}, hypothesisId: 'H5');
              // #endregion
              state = state.copyWith(status: BmwStatus.fromBytes(last));
            }
          }
          if (uuid == BmwBleUuids.nowPlaying) _nowPlayingChar = c;
          if (uuid == BmwBleUuids.clusterText) _clusterTextChar = c;
        }
        break;
      }
    } catch (e) {
      // #region agent log
      debugLog('bmw_ble_provider.dart:_discoverAndSubscribe', 'error', data: {'error': e.toString()}, hypothesisId: 'H3');
      // #endregion
      state = state.copyWith(error: e.toString());
    }
  }

  Future<void> scanAndConnect() async {
    state = state.copyWith(isConnecting: true, error: null);
    // #region agent log
    debugLog('bmw_ble_provider.dart:scanAndConnect', 'start', hypothesisId: 'H1');
    // #endregion
    try {
      if (await Permission.bluetoothScan.request().isDenied ||
          await Permission.bluetoothConnect.request().isDenied) {
        state = state.copyWith(isConnecting: false, error: 'Allow Bluetooth permission');
        return;
      }
      await Permission.bluetoothAdvertise.request();
      final loc = await Permission.locationWhenInUse.request();
      if (loc.isDenied && loc.isPermanentlyDenied == false) {
        state = state.copyWith(isConnecting: false, error: 'Allow location (required for BLE scan)');
        return;
      }
      final adapterOn = await FlutterBluePlus.adapterState.first == BluetoothAdapterState.on;
      if (!adapterOn) {
        state = state.copyWith(isConnecting: false, error: 'Turn on Bluetooth');
        return;
      }
    } catch (e) {
      state = state.copyWith(isConnecting: false, error: e.toString());
      return;
    }
    BluetoothDevice? found;
    try {
      await FlutterBluePlus.startScan(timeout: const Duration(seconds: 10));
      await for (final list in FlutterBluePlus.scanResults.timeout(const Duration(seconds: 11))) {
        // #region agent log
        if (list.isNotEmpty) debugLog('bmw_ble_provider.dart:scanAndConnect', 'scan results', data: {'count': list.length, 'names': list.map((r) => r.advertisementData.advName).toList()}, hypothesisId: 'H1');
        // #endregion
        for (final r in list) {
          final name = r.advertisementData.advName;
          final hasService = r.advertisementData.serviceUuids.any(
            (u) => u.toString().toLowerCase().contains('1a2b0001'),
          );
          final nameStr = name ?? '';
          final nameMatch = nameStr.isNotEmpty && (nameStr.contains('My BMW') || nameStr.contains('BMW E39 Key') || nameStr.contains('E39'));
          if (nameMatch || hasService) {
            found = r.device;
            break;
          }
        }
        if (found != null) break;
      }
    } catch (e) {
      state = state.copyWith(isConnecting: false, error: e.toString());
      return;
    } finally {
      await FlutterBluePlus.stopScan();
    }
    if (found == null) {
      // #region agent log
      debugLog('bmw_ble_provider.dart:scanAndConnect', 'device not found', hypothesisId: 'H1');
      // #endregion
      state = state.copyWith(isConnecting: false, error: 'My BMW / BMW Key не найден');
      return;
    }
    try {
      // #region agent log
      debugLog('bmw_ble_provider.dart:scanAndConnect', 'connecting', data: {'deviceId': found.remoteId.toString()}, hypothesisId: 'H2');
      // #endregion
      await found.connect();
      // #region agent log
      debugLog('bmw_ble_provider.dart:scanAndConnect', 'connect success', hypothesisId: 'H2');
      // #endregion
      state = state.copyWith(device: found, isConnecting: false);
    } catch (e) {
      // #region agent log
      debugLog('bmw_ble_provider.dart:scanAndConnect', 'connect failed', data: {'error': e.toString()}, hypothesisId: 'H2');
      // #endregion
      state = state.copyWith(isConnecting: false, error: e.toString());
    }
  }

  Future<void> disconnect() async {
    if (state.device != null) await state.device!.disconnect();
    _statusSub?.cancel();
    _statusSub = null;
    _controlChar = null;
    _statusChar = null;
    _nowPlayingChar = null;
    _clusterTextChar = null;
    state = const BmwBleState();
  }

  Future<void> sendControl(int cmd) async {
    final c = _controlChar;
    if (c == null || !state.isConnected) return;
    try {
      await c.write([cmd], withoutResponse: false);
    } catch (e) {
      state = state.copyWith(error: e.toString());
    }
  }

  Future<void> sendNowPlaying(String track, String artist) async {
    final c = _nowPlayingChar;
    if (c == null || !state.isConnected) return;
    final trackBytes = utf8.encode(track);
    final artistBytes = utf8.encode(artist);
    const maxTotal = 200;
    var t = trackBytes;
    var a = artistBytes;
    if (t.length + 1 + a.length > maxTotal) {
      if (t.length + 1 > maxTotal) {
        t = t.length > maxTotal - 1 ? t.sublist(0, maxTotal - 1) : t;
        a = <int>[];
      } else {
        a = a.sublist(0, maxTotal - 1 - t.length);
      }
    }
    final bytes = [...t, 0, ...a];
    try {
      await c.write(bytes, withoutResponse: false);
    } catch (e) {
      state = state.copyWith(error: e.toString());
    }
  }

  Future<void> sendClusterText(String text) async {
    final c = _clusterTextChar;
    if (c == null || !state.isConnected) return;
    final encoded = utf8.encode(text);
    final toSend = encoded.length > 20 ? encoded.sublist(0, 20) : encoded;
    try {
      await c.write(toSend, withoutResponse: false);
    } catch (e) {
      state = state.copyWith(error: e.toString());
    }
  }

  Future<void> sendStrobeCommand() async {
    await sendControl(BmwControlCmd.strobeToggle);
  }

  Future<void> sendSensoryDarkMode(bool enable) async {
    await sendControl(enable ? BmwControlCmd.sensoryDarkModeOn : BmwControlCmd.sensoryDarkModeOff);
  }

  Future<void> sendComfortBlink(bool enable) async {
    await sendControl(enable ? BmwControlCmd.comfortBlinkOn : BmwControlCmd.comfortBlinkOff);
  }

  Future<void> sendPanicCommand() async {
    await sendControl(BmwControlCmd.panicMode);
  }

  Future<void> sendMirrorFoldPreference(bool on) async {
    await sendControl(on ? BmwControlCmd.mirrorFoldOn : BmwControlCmd.mirrorFoldOff);
  }

  /// Sends 0x98 (set greeting) then the text via cluster characteristic so ESP32 can store it.
  Future<void> sendStartupGreeting(String text) async {
    await sendControl(BmwControlCmd.startupGreeting);
    await sendClusterText(text.length > 20 ? text.substring(0, 20) : text);
  }

  @override
  void dispose() {
    _connSub?.cancel();
    _statusSub?.cancel();
    super.dispose();
  }
}

extension _BmwBleStateCopy on BmwBleState {
  BmwBleState copyWith({
    BluetoothDevice? device,
    bool? isConnecting,
    bool? isConnected,
    BmwStatus? status,
    String? error,
  }) {
    return BmwBleState(
      device: device ?? this.device,
      isConnecting: isConnecting ?? this.isConnecting,
      isConnected: isConnected ?? this.isConnected,
      status: status ?? this.status,
      error: error,
    );
  }
}
