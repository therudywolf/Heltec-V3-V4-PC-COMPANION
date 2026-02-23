/*
 * NOCTURNE_OS — BADWOLF USB HID: PSYCHOLOGICAL WARFARE EDITION.
 * Target: Windows 10/11
 * Fixed: Restored legacy runBackdoor() for main.cpp compatibility.
 */
#pragma once
#include <Arduino.h>

// Тайминги
#define BADWOLF_DELAY_AFTER_RUN_MS 1500
#define BADWOLF_DELAY_AFTER_POWERSHELL_MS 800

class UsbManager {
public:
  UsbManager();
  void begin();
  void stop();
  bool isActive() const { return active_; }

  // --- BASIC PAYLOADS ---
  void runMatrix();  // Классика (зеленый текст)
  void runSniffer(); // Сбор инфы

  // --- INSANE PAYLOADS (PSY-OPS) ---
  void runVoice();       // TTS: Компьютер говорит
  void runPoltergeist(); // Хаос мыши
  void runFakeUpdate();  // Фейковый экран

  // --- LEGACY / UTILS ---
  void runBackdoor(); // Win+R -> cmd (Restored for compatibility)

  /** * Ducky Script Index:
   * 0: Matrix (Style)
   * 1: Sniffer (Recon)
   * 2: The Voice (Fear)
   * 3: Poltergeist (Chaos)
   * 4: Fake Update (Panic)
   */
  void runDuckyScript(int index);

  static constexpr int DUCKY_SCRIPT_COUNT = 5;

private:
  bool active_ = false;
  void openPowerShell();
  void openRunDialog();
};