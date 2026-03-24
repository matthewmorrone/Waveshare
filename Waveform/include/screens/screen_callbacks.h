#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "state/runtime_state.h"

// --- Motion view public interface ---
MotionViewMode motionGetViewMode();
void motionSetViewMode(MotionViewMode mode);
void motionAdvanceView();
void motionReverseView();
void motionCenterDot();

struct QrEntry
{
  const char *title;
  const char *payload;
};

extern const QrEntry kQrEntries[];
extern const size_t kQrEntryCount;

lv_obj_t *waveformWatchScreenRoot();
bool waveformBuildWatchScreen();
bool waveformRefreshWatchScreen();
void waveformEnterWatchScreen();
void waveformLeaveWatchScreen();
void waveformTickWatchScreen(uint32_t nowMs);
void watchShowCalendar();
void watchDismissCalendar();
void watchDismissCalendarSilent();
bool watchCalendarIsVisible();

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
void recorderHandleTap();

#ifdef SCREEN_QR
lv_obj_t *waveformQrScreenRoot();
bool waveformBuildQrScreen();
bool waveformRefreshQrScreen();
void waveformEnterQrScreen();
void waveformLeaveQrScreen();
void waveformTickQrScreen(uint32_t nowMs);
#endif

lv_obj_t *waveformCalculatorScreenRoot();
bool waveformBuildCalculatorScreen();
bool waveformRefreshCalculatorScreen();
void waveformEnterCalculatorScreen();
void waveformLeaveCalculatorScreen();
void waveformTickCalculatorScreen(uint32_t nowMs);

#ifdef SCREEN_MOON
lv_obj_t *waveformMoonScreenRoot();
bool waveformBuildMoonScreen();
bool waveformRefreshMoonScreen();
void waveformEnterMoonScreen();
void waveformLeaveMoonScreen();
void waveformTickMoonScreen(uint32_t nowMs);
#endif

#ifdef SCREEN_STOPWATCH
lv_obj_t *waveformStopwatchScreenRoot();
bool waveformBuildStopwatchScreen();
bool waveformRefreshStopwatchScreen();
void waveformEnterStopwatchScreen();
void waveformLeaveStopwatchScreen();
void waveformTickStopwatchScreen(uint32_t nowMs);
void stopwatchHandleTap();
void stopwatchHandleLongPress();
#endif

#ifdef SCREEN_TIMER
lv_obj_t *waveformTimerScreenRoot();
bool waveformBuildTimerScreen();
bool waveformRefreshTimerScreen();
void waveformEnterTimerScreen();
void waveformLeaveTimerScreen();
void waveformTickTimerScreen(uint32_t nowMs);
void timerHandleTap();
#endif
