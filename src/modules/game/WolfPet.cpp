#include "modules/game/WolfPet.h"
#if NOCT_FEATURE_WOLFPET

#include <Preferences.h>

static const unsigned long WOLF_DECAY_INTERVAL_MS = 90000UL;  // -stats every 90s
static const unsigned long WOLF_SAVE_INTERVAL_MS = 300000UL;  // autosave every 5 min
static const unsigned long WOLF_DAY_MS = 3600000UL;           // 1 pet-day = 1 real hour

void WolfPet::begin() {
  Preferences p;
  p.begin("wolfpet", true);
  hunger_ = p.getInt("h", 70);
  happy_ = p.getInt("j", 70);
  energy_ = p.getInt("e", 70);
  ageDays_ = p.getUInt("age", 0);
  alive_ = p.getBool("alive", true);
  p.end();
  clampAll();
  lastDecayMs_ = millis();
  lastSaveMs_ = millis();
}

void WolfPet::clampAll() {
  if (hunger_ < 0) hunger_ = 0;
  if (hunger_ > 100) hunger_ = 100;
  if (happy_ < 0) happy_ = 0;
  if (happy_ > 100) happy_ = 100;
  if (energy_ < 0) energy_ = 0;
  if (energy_ > 100) energy_ = 100;
}

void WolfPet::save() {
  Preferences p;
  p.begin("wolfpet", false);
  p.putInt("h", hunger_);
  p.putInt("j", happy_);
  p.putInt("e", energy_);
  p.putUInt("age", ageDays_);
  p.putBool("alive", alive_);
  p.end();
}

void WolfPet::tick(unsigned long now) {
  while (now - lastDecayMs_ >= WOLF_DECAY_INTERVAL_MS) {
    lastDecayMs_ += WOLF_DECAY_INTERVAL_MS;
    if (alive_) {
      hunger_ -= 2;
      happy_ -= 1;
      energy_ -= 1;
      ageAccumMs_ += WOLF_DECAY_INTERVAL_MS;
      if (ageAccumMs_ >= WOLF_DAY_MS) {
        ageAccumMs_ -= WOLF_DAY_MS;
        ageDays_++;
      }
      clampAll();
      if (hunger_ == 0 && happy_ == 0 && energy_ == 0) {
        alive_ = false;
        save();
      }
    }
  }
  if (now - lastSaveMs_ >= WOLF_SAVE_INTERVAL_MS) {
    lastSaveMs_ = now;
    save();
  }
}

void WolfPet::doAction(int action) {
  if (!alive_) { // any care revives a fainted pet
    alive_ = true;
    hunger_ = happy_ = energy_ = 40;
    clampAll();
    save();
    return;
  }
  switch (action) {
  case ACT_FEED:
    hunger_ += 30;
    happy_ += 5;
    break;
  case ACT_PLAY:
    happy_ += 25;
    energy_ -= 10;
    hunger_ -= 5;
    break;
  case ACT_REST:
    energy_ += 35;
    happy_ += 3;
    break;
  default:
    break;
  }
  clampAll();
  save();
}

int WolfPet::mood() const {
  if (!alive_) return 0;
  int avg = (hunger_ + happy_ + energy_) / 3;
  if (avg < 30) return 0;
  if (avg < 65) return 1;
  return 2;
}

const char *WolfPet::statusText() const {
  if (!alive_) return "fainted-feed!";
  if (hunger_ < 25) return "Hungry!";
  if (energy_ < 25) return "Tired...";
  if (happy_ < 25) return "Bored...";
  return mood() == 2 ? "Happy!" : "OK";
}

#endif // NOCT_FEATURE_WOLFPET
