/*
 * NOCTURNE_OS — WolfPet: tamagotchi-style background pet (#3).
 * Three stats (food/joy/energy) decay over REAL time and persist across reboots
 * (NVS "wolfpet"), so the pet "lives in the background" whether or not its scene
 * is on screen. Feed/play/rest restore stats; neglect makes it faint.
 */
#ifndef NOCTURNE_WOLF_PET_H
#define NOCTURNE_WOLF_PET_H

#include "nocturne/config.h"
#if NOCT_FEATURE_WOLFPET
#include <Arduino.h>

class WolfPet {
public:
  // Only FEED and PLAY are manual. The wolf RESTS on its own (a real tamagotchi
  // sleeps autonomously): when tired it falls asleep and regenerates energy with
  // no button, then wakes.
  enum Action { ACT_FEED = 0, ACT_PLAY, ACT_COUNT };

  void begin();                  // load persisted state
  void tick(unsigned long now);  // decay/sleep over time + autosave (call every loop)
  void doAction(int action);     // ACT_FEED / ACT_PLAY

  int hunger() const { return hunger_; }  // 0..100 (100 = well fed)
  int happy() const { return happy_; }    // 0..100
  int energy() const { return energy_; }   // 0..100
  uint32_t ageDays() const { return ageDays_; }
  bool isAlive() const { return alive_; }
  bool isSleeping() const { return sleeping_; } // auto rest cycle
  int mood() const;              // 0 = sad/fainted, 1 = ok, 2 = happy
  const char *statusText() const;

private:
  void save();
  void clampAll();

  int hunger_ = 70, happy_ = 70, energy_ = 70;
  uint32_t ageDays_ = 0;
  bool alive_ = true;
  bool sleeping_ = false; // autonomous rest: true while regenerating energy
  unsigned long lastDecayMs_ = 0;
  unsigned long lastSaveMs_ = 0;
  uint32_t ageAccumMs_ = 0;
};

#endif // NOCT_FEATURE_WOLFPET
#endif
