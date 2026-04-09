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

#ifdef SCREEN_LAUNCHER
lv_obj_t *waveformLauncherScreenRoot();
bool waveformBuildLauncherScreen();
bool waveformRefreshLauncherScreen();
void waveformEnterLauncherScreen();
void waveformLeaveLauncherScreen();
void waveformTickLauncherScreen(uint32_t nowMs);
void waveformLauncherScrollBy(int dy);
void waveformLauncherScrollFling();
#endif

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

#ifdef SCREEN_CALENDAR
lv_obj_t *waveformCalendarScreenRoot();
bool waveformBuildCalendarScreen();
bool waveformRefreshCalendarScreen();
void waveformEnterCalendarScreen();
void waveformLeaveCalendarScreen();
void waveformTickCalendarScreen(uint32_t nowMs);
#endif

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

#ifdef SCREEN_WEATHER_HOURLY
lv_obj_t *waveformWeatherHourlyScreenRoot();
bool waveformBuildWeatherHourlyScreen();
bool waveformRefreshWeatherHourlyScreen();
void waveformEnterWeatherHourlyScreen();
void waveformLeaveWeatherHourlyScreen();
void waveformTickWeatherHourlyScreen(uint32_t nowMs);
#endif

#ifdef SCREEN_WEATHER_DAILY
lv_obj_t *waveformWeatherDailyScreenRoot();
bool waveformBuildWeatherDailyScreen();
bool waveformRefreshWeatherDailyScreen();
void waveformEnterWeatherDailyScreen();
void waveformLeaveWeatherDailyScreen();
void waveformTickWeatherDailyScreen(uint32_t nowMs);
#endif

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
bool recorderStartVoiceArm();
void recorderCancelVoiceArm();
bool recorderVoiceArmActive();
bool recorderVoiceClipCaptured();

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

#ifdef SCREEN_SYSTEM
lv_obj_t *waveformSystemScreenRoot();
bool waveformBuildSystemScreen();
bool waveformRefreshSystemScreen();
void waveformEnterSystemScreen();
void waveformLeaveSystemScreen();
void waveformTickSystemScreen(uint32_t nowMs);
#endif

#ifdef SCREEN_SETTINGS
lv_obj_t *waveformSettingsScreenRoot();
bool waveformBuildSettingsScreen();
bool waveformRefreshSettingsScreen();
void waveformEnterSettingsScreen();
void waveformLeaveSettingsScreen();
void waveformTickSettingsScreen(uint32_t nowMs);
void waveformDestroySettingsScreen();
#endif

#ifdef SCREEN_SPECTRUM
lv_obj_t *waveformSpectrumScreenRoot();
bool waveformBuildSpectrumScreen();
bool waveformRefreshSpectrumScreen();
void waveformEnterSpectrumScreen();
void waveformLeaveSpectrumScreen();
void waveformTickSpectrumScreen(uint32_t nowMs);
void waveformDestroySpectrumScreen();
#endif

#ifdef SCREEN_RADIO
lv_obj_t *waveformRadioScreenRoot();
bool waveformBuildRadioScreen();
bool waveformRefreshRadioScreen();
void waveformEnterRadioScreen();
void waveformLeaveRadioScreen();
void waveformTickRadioScreen(uint32_t nowMs);
void waveformDestroyRadioScreen();
#endif

#ifdef SCREEN_LOCKET
lv_obj_t *waveformLocketScreenRoot();
bool waveformBuildLocketScreen();
bool waveformRefreshLocketScreen();
void waveformEnterLocketScreen();
void waveformLeaveLocketScreen();
void waveformTickLocketScreen(uint32_t nowMs);
void waveformDestroyLocketScreen();
void locketHandleOrbTap();
#endif

#ifdef SCREEN_ORB
lv_obj_t *waveformOrbScreenRoot();
bool waveformBuildOrbScreen();
bool waveformRefreshOrbScreen();
void waveformEnterOrbScreen();
void waveformLeaveOrbScreen();
void waveformTickOrbScreen(uint32_t nowMs);
#endif

// --- Timezone picker (standalone overlay screen) ---
void showTimezonePicker();
void hideTimezonePicker();
void timezonePickerSetInitial(int utcOffset);
void timezonePickerHandleSwipe(int deltaX);
