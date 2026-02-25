/// My BMW — BLE UUIDs (must match ESP32 BleKeyService).
class BmwBleUuids {
  BmwBleUuids._();

  static const String service = '1a2b0001-5e6f-4a5b-8c9d-0e1f2a3b4c5d';
  static const String control = '1a2b0002-5e6f-4a5b-8c9d-0e1f2a3b4c5d';
  static const String status = '1a2b0003-5e6f-4a5b-8c9d-0e1f2a3b4c5d';
  static const String nowPlaying = '1a2b0004-5e6f-4a5b-8c9d-0e1f2a3b4c5d';
  static const String clusterText = '1a2b0005-5e6f-4a5b-8c9d-0e1f2a3b4c5d';
}

/// Control commands (one byte write to control characteristic).
/// Matches BmwManager lightCommandCb switch.
class BmwControlCmd {
  static const int goodbyeLights = 0;
  static const int followMeHome = 1;
  static const int parkLights = 2;
  static const int hazardLights = 3;
  static const int lowBeams = 4;
  static const int lightsOff = 5;
  static const int unlock = 6;
  static const int lock = 7;
  static const int trunkOpen = 8;
  static const int clusterNoct = 9;
  static const int doorsUnlockInterior = 10;
  static const int doorsLockKey = 11;
  static const int windowFrontDriverOpen = 12;
  static const int windowFrontDriverClose = 13;
  static const int windowFrontPassengerOpen = 14;
  static const int windowFrontPassengerClose = 15;
  static const int windowRearDriverOpen = 16;
  static const int windowRearDriverClose = 17;
  static const int windowRearPassengerOpen = 18;
  static const int windowRearPassengerClose = 19;
  static const int wipersFront = 20;
  static const int washerFront = 21;
  static const int interiorOff = 22;
  static const int interiorOn3s = 23;
  static const int clownFlash = 24;
  static const int doorsHardLock = 25;
  static const int allExceptDriverLock = 26;
  static const int driverDoorLock = 27;
  static const int doorsFuelTrunk = 28;
  static const int doorsUnlockGM = 29;
  static const int startLightShow = 0x80;
  static const int stopLightShow = 0x81;
}
