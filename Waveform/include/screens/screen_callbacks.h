#pragma once

#include <Arduino.h>
#include <lvgl.h>

struct QrEntry
{
  const char *title;
  const char *payload;
};

extern const QrEntry kQrEntries[];
extern const size_t kQrEntryCount;

lv_obj_t *waveformWatchfaceScreenRoot();
bool waveformBuildWatchfaceScreen();
bool waveformRefreshWatchfaceScreen();
void waveformEnterWatchfaceScreen();
void waveformLeaveWatchfaceScreen();
void waveformTickWatchfaceScreen(uint32_t nowMs);

lv_obj_t *waveformMotionScreenRoot();
bool waveformBuildMotionScreen();
bool waveformRefreshMotionScreen();
void waveformEnterMotionScreen();
void waveformLeaveMotionScreen();
void waveformTickMotionScreen(uint32_t nowMs);

lv_obj_t *waveformWeatherScreenRoot();
bool waveformBuildWeatherScreen();
bool waveformRefreshWeatherScreen();
void waveformEnterWeatherScreen();
void waveformLeaveWeatherScreen();
void waveformTickWeatherScreen(uint32_t nowMs);

lv_obj_t *waveformGeoScreenRoot();
bool waveformBuildGeoScreen();
bool waveformRefreshGeoScreen();
void waveformEnterGeoScreen();
void waveformLeaveGeoScreen();
void waveformTickGeoScreen(uint32_t nowMs);

lv_obj_t *waveformSolarScreenRoot();
bool waveformBuildSolarScreen();
bool waveformRefreshSolarScreen();
void waveformEnterSolarScreen();
void waveformLeaveSolarScreen();
void waveformTickSolarScreen(uint32_t nowMs);

lv_obj_t *waveformSkyScreenRoot();
bool waveformBuildSkyScreen();
bool waveformRefreshSkyScreen();
void waveformEnterSkyScreen();
void waveformLeaveSkyScreen();
void waveformTickSkyScreen(uint32_t nowMs);

lv_obj_t *waveformRecorderScreenRoot();
bool waveformBuildRecorderScreen();
bool waveformRefreshRecorderScreen();
void waveformEnterRecorderScreen();
void waveformLeaveRecorderScreen();
void waveformTickRecorderScreen(uint32_t nowMs);

lv_obj_t *waveformQrScreenRoot();
bool waveformBuildQrScreen();
bool waveformRefreshQrScreen();
void waveformEnterQrScreen();
void waveformLeaveQrScreen();
void waveformTickQrScreen(uint32_t nowMs);

lv_obj_t *waveformCalculatorScreenRoot();
bool waveformBuildCalculatorScreen();
bool waveformRefreshCalculatorScreen();
void waveformEnterCalculatorScreen();
void waveformLeaveCalculatorScreen();
void waveformTickCalculatorScreen(uint32_t nowMs);
