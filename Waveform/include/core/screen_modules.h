#pragma once

#include "core/screen_manager.h"

#ifdef SCREEN_WATCH
const ScreenModule &watchScreenModule();
#endif
#ifdef SCREEN_MOTION
const ScreenModule &motionScreenModule();
#endif
#ifdef SCREEN_WEATHER
const ScreenModule &weatherScreenModule();
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
#ifdef SCREEN_MOON
const ScreenModule &moonScreenModule();
#endif
#ifdef SCREEN_STOPWATCH
const ScreenModule &stopwatchScreenModule();
#endif
#ifdef SCREEN_TIMER
const ScreenModule &timerScreenModule();
#endif
