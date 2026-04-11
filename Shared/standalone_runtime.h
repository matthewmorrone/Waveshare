#pragma once

#include <Adafruit_XCA9554.h>
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <SensorPCF85063.hpp>
#include <XPowersLib.h>
#include <lvgl.h>

#include "runtime_state.h"
#include "screen_manager.h"

extern Arduino_SH8601 *gfx;
extern XPowersAXP2101 power;
extern Adafruit_XCA9554 expander;
extern SensorPCF85063 rtc;
extern Preferences preferences;

extern lv_display_t *display;
extern lv_indev_t *touchInput;
extern lv_obj_t *screenRoots[24];

extern bool rtcReady;
extern bool expanderReady;
extern bool powerReady;
extern bool sdMounted;

extern size_t currentScreenIndex;
extern ConnectivityState connectivityState;
extern uint32_t nextWifiRetryAtMs;
extern uint32_t lastAutoCycleAtMs;
extern bool autoCycleEnabled;

bool networkIsOnline();
void noteActivity();
void setDisplayBrightness(uint8_t brightness);
void setOfflineMode(const char *reason);
void initRtc();
void applyConfiguredTimezone();
void showTimezonePicker();
void hideTimezonePicker();
void timezonePickerSetInitial(int utcOffset);
void timezonePickerHandleSwipe(int deltaX);
bool showScreenById(ScreenId id);

struct StandaloneRuntimeConfig
{
  ScreenId initialScreen;
  bool enableWifi = true;
  bool enableBle = true;
  bool enableOta = false;
  bool enableSd = false;
  uint32_t refreshIntervalMs = 250;
  uint32_t tickIntervalMs = 90;
};

void standaloneRuntimeSetup(const StandaloneRuntimeConfig &config);
void standaloneRuntimeLoop();
