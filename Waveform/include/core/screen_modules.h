#pragma once

#include "core/screen_manager.h"

#ifdef SCREEN_LAUNCHER
const ScreenModule &launcherScreenModule();
#endif
#ifdef SCREEN_WATCH
const ScreenModule &watchScreenModule();
#endif
#ifdef SCREEN_CALENDAR
const ScreenModule &calendarScreenModule();
#endif
#ifdef SCREEN_MOTION
const ScreenModule &motionScreenModule();
#endif
#ifdef SCREEN_WEATHER
const ScreenModule &weatherScreenModule();
#endif
#ifdef SCREEN_WEATHER_HOURLY
const ScreenModule &weatherHourlyScreenModule();
#endif
#ifdef SCREEN_WEATHER_DAILY
const ScreenModule &weatherDailyScreenModule();
#endif
#ifdef SCREEN_GEO
const ScreenModule &geoScreenModule();
#endif
#ifdef SCREEN_SOLAR
const ScreenModule &solarScreenModule();
#endif
#ifdef SCREEN_SKY
const ScreenModule &skyScreenModule();
#endif
#ifdef SCREEN_RECORDER
const ScreenModule &recorderScreenModule();
#endif
#ifdef SCREEN_QR
const ScreenModule &qrScreenModule();
#endif
#ifdef SCREEN_CALCULATOR
const ScreenModule &calculatorScreenModule();
#endif
#ifdef SCREEN_STOPWATCH
const ScreenModule &stopwatchScreenModule();
#endif
#ifdef SCREEN_TIMER
const ScreenModule &timerScreenModule();
#endif
#ifdef SCREEN_SYSTEM
const ScreenModule &systemScreenModule();
#endif
#ifdef SCREEN_SETTINGS
const ScreenModule &settingsScreenModule();
#endif
#ifdef SCREEN_SPECTRUM
const ScreenModule &spectrumScreenModule();
#endif
#ifdef SCREEN_RADIO
const ScreenModule &radioScreenModule();
#endif
#ifdef SCREEN_ORB
const ScreenModule &orbScreenModule();
#endif
