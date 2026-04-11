#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct SettingsState
{
  uint8_t brightness = 255;      // 0-255
  bool wifiEnabled = true;
  bool bleEnabled = true;
  bool use24hClock = true;
  bool useCelsius = false;
  bool autoCycleEnabled = false;
  uint32_t autoCycleIntervalMs = 5000;
  uint32_t sleepAfterMs = 5UL * 60UL * 1000UL;
  int utcOffsetHours = 0;
  bool faceDownBlackout = true;
};

SettingsState &settingsState();
void settingsLoad(Preferences &prefs);
void settingsSave(Preferences &prefs);
