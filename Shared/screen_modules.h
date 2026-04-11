#pragma once

#include "screen_manager.h"

#ifdef SCREEN_CALCULATOR 
const ScreenModule &calculatorScreenModule(); 
#endif
#ifdef SCREEN_CALENDAR 
const ScreenModule &calendarScreenModule(); 
#endif
#ifdef SCREEN_CUBE 
const ScreenModule &cubeScreenModule(); 
#endif
#ifdef SCREEN_GPS 
const ScreenModule &gpsScreenModule(); 
#endif
#ifdef SCREEN_IMU 
const ScreenModule &imuScreenModule(); 
#endif
#ifdef SCREEN_LAUNCHER 
const ScreenModule &launcherScreenModule(); 
#endif
#ifdef SCREEN_LOCKET 
const ScreenModule &locketScreenModule(); 
#endif
#ifdef SCREEN_MOTION 
const ScreenModule &motionScreenModule(); 
#endif
#ifdef SCREEN_PLANETS 
const ScreenModule &planetsScreenModule(); 
#endif
#ifdef SCREEN_QR 
const ScreenModule &qrScreenModule(); 
#endif
#ifdef SCREEN_RADIO 
const ScreenModule &radioScreenModule(); 
#endif
#ifdef SCREEN_RECORDER 
const ScreenModule &recorderScreenModule(); 
#endif
#ifdef SCREEN_SETTINGS 
const ScreenModule &settingsScreenModule(); 
#endif
#ifdef SCREEN_SOUND 
const ScreenModule &soundScreenModule(); 
#endif
#ifdef SCREEN_SPECTRUM 
const ScreenModule &spectrumScreenModule(); 
#endif
#ifdef SCREEN_STARS 
const ScreenModule &starsScreenModule(); 
#endif
#ifdef SCREEN_STOPWATCH 
const ScreenModule &stopwatchScreenModule(); 
#endif
#ifdef SCREEN_SYSTEM 
const ScreenModule &systemScreenModule(); 
#endif
#ifdef SCREEN_TIMER 
const ScreenModule &timerScreenModule(); 
#endif
#ifdef SCREEN_WATCH 
const ScreenModule &watchScreenModule(); 
#endif
#ifdef SCREEN_WEATHER 
const ScreenModule &weatherScreenModule(); 
#endif
#ifdef SCREEN_WEATHER_DAILY 
const ScreenModule &weatherDailyScreenModule(); 
#endif
#ifdef SCREEN_WEATHER_HOURLY 
const ScreenModule &weatherHourlyScreenModule(); 
#endif
