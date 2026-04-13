#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct WeatherModuleConfig
{
  Preferences *preferences = nullptr;
  const char *cacheKey = nullptr;
  uint32_t refreshIntervalMs = 0;
  uint32_t retryIntervalMs = 0;
  uint32_t fetchTimeoutMs = 0;
  bool fetchEnabled = true;
  bool (*networkIsOnline)() = nullptr;
  bool (*otaInProgress)() = nullptr;
  bool (*lightSleepActive)() = nullptr;
  String (*apiUrl)() = nullptr;
  String (*updatedLabel)() = nullptr;
};

enum class WeatherModuleView : uint8_t
{
  Current,
  Hourly,
  Daily,
};

void weatherModuleConfigure(const WeatherModuleConfig &config);
bool weatherModuleRestoreCache();
bool weatherModuleHasData();
void weatherModuleSetUnavailable(const char *updatedLabel);
void weatherModuleMarkOffline();
void weatherModuleScheduleRefreshIn(uint32_t delayMs);
void weatherModuleUpdate();
void weatherModuleCycleDebugScene(int direction);
void weatherModuleClearDebugOverride();
void weatherModuleRefreshUi();
bool weatherModuleSetActiveView(WeatherModuleView view, bool loadScreen);
WeatherModuleView weatherModuleActiveView();
String weatherApiUrl();
String weatherUpdatedLabel();
void handleWeatherStackSwipe(int deltaY);
