#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "runtime_state.h"

// Default builds enable the full firmware. Standalone screen projects define
// `WAVEFORM_STANDALONE_PROFILE` and then opt into specific screen flags with
// `WAVEFORM_ENABLE_SCREEN_*` build flags.
#if !defined(WAVEFORM_STANDALONE_PROFILE)
#define SCREEN_CALCULATOR
#define SCREEN_CALENDAR
#define SCREEN_CUBE
#define SCREEN_GPS
#define SCREEN_IMU
#define SCREEN_LAUNCHER
#define SCREEN_LOCKET
#define SCREEN_MOTION
#define SCREEN_PLANETS
#define SCREEN_QR
#define SCREEN_RADIO
#define SCREEN_RECORDER
#define SCREEN_SETTINGS
#define SCREEN_SHARED
#define SCREEN_SOUND
#define SCREEN_SPECTRUM
#define SCREEN_STARS
#define SCREEN_STOPWATCH
#define SCREEN_SYSTEM
#define SCREEN_TIMER
#define SCREEN_WATCH
#define SCREEN_WEATHER
#define SCREEN_WEATHER_DAILY
#define SCREEN_WEATHER_HOURLY
#else
#ifdef WAVEFORM_ENABLE_SCREEN_LAUNCHER
#define SCREEN_LAUNCHER
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_WATCH
#define SCREEN_WATCH
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_CALENDAR
#define SCREEN_CALENDAR
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_MOTION
#define SCREEN_MOTION
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_CUBE
#define SCREEN_CUBE
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_IMU
#define SCREEN_IMU
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_WEATHER
#define SCREEN_WEATHER
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_WEATHER_HOURLY
#define SCREEN_WEATHER_HOURLY
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_WEATHER_DAILY
#define SCREEN_WEATHER_DAILY
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_GPS
#define SCREEN_GPS
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_PLANETS
#define SCREEN_PLANETS
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_STARS
#define SCREEN_STARS
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_RECORDER
#define SCREEN_RECORDER
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_QR
#define SCREEN_QR
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_CALCULATOR
#define SCREEN_CALCULATOR
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_STOPWATCH
#define SCREEN_STOPWATCH
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_TIMER
#define SCREEN_TIMER
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_SYSTEM
#define SCREEN_SYSTEM
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_SETTINGS
#define SCREEN_SETTINGS
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_SPECTRUM
#define SCREEN_SPECTRUM
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_RADIO
#define SCREEN_RADIO
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_LOCKET
#define SCREEN_LOCKET
#endif
#ifdef WAVEFORM_ENABLE_SCREEN_ORB
#define SCREEN_ORB
#endif
#endif

enum class ScreenId : uint8_t {
#ifdef SCREEN_LAUNCHER
  Launcher,
#endif
#ifdef SCREEN_WATCH
  Watch,
#endif
#ifdef SCREEN_CALENDAR
  Calendar,
#endif
#ifdef SCREEN_MOTION
  Motion,
#endif
#ifdef SCREEN_CUBE
  Cube,
#endif
#ifdef SCREEN_IMU
  Imu,
#endif
#ifdef SCREEN_WEATHER
  Weather,
#endif
#ifdef SCREEN_WEATHER_HOURLY
  WeatherHourly,
#endif
#ifdef SCREEN_WEATHER_DAILY
  WeatherDaily,
#endif
#ifdef SCREEN_GPS
  Gps,
#endif
#ifdef SCREEN_PLANETS
  Planets,
#endif
#ifdef SCREEN_STARS
  Stars,
#endif
#ifdef SCREEN_RECORDER
  Recorder,
#endif
#ifdef SCREEN_QR
  Qr,
#endif
#ifdef SCREEN_CALCULATOR
  Calculator,
#endif
#ifdef SCREEN_STOPWATCH
  Stopwatch,
#endif
#ifdef SCREEN_TIMER
  Timer,
#endif
#ifdef SCREEN_SYSTEM
  System,
#endif
#ifdef SCREEN_SETTINGS
  Settings,
#endif
#ifdef SCREEN_SPECTRUM
  Spectrum,
#endif
#ifdef SCREEN_RADIO
  Radio,
#endif
#ifdef SCREEN_LOCKET
  Locket,
#endif
#ifdef SCREEN_ORB
  Orb,
#endif
};

extern const size_t kScreenCount;

struct ScreenModule
{
  ScreenId id;
  const char *name;
  bool (*build)();
  bool (*refresh)();
  void (*enter)();
  void (*leave)();
  void (*tick)(uint32_t nowMs);
  lv_obj_t *(*root)();
  void (*destroy)();  // optional: free the screen and reset its state so it rebuilds next visit
};

const ScreenModule &screenModule(ScreenId id);
const ScreenModule &screenModuleByIndex(size_t index);

bool screenManagerEnsureBuilt(ScreenId id);
bool screenManagerRefresh(ScreenId id);
void screenManagerEnter(ScreenId id);
void screenManagerLeave(ScreenId id);
void screenManagerTick(ScreenId id, uint32_t nowMs);

bool screenManagerIsEnabled(ScreenId id);
bool screenManagerHasFailure(ScreenId id);
const char *screenManagerFailureReason(ScreenId id);
lv_obj_t *screenManagerRoot(ScreenId id);

size_t screenManagerNextEnabledIndex(size_t fromIndex);
size_t screenManagerPreviousEnabledIndex(size_t fromIndex);
void screenManagerShowFallback(ScreenId id, const char *reason);
void screenManagerDestroy(ScreenId id);

bool showScreenById(ScreenId id);
