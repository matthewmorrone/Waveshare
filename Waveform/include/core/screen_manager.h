#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "state/runtime_state.h"

// Comment/uncomment to enable or disable screens at compile time.
// Matching entry in screen_manager.cpp must also be commented out.
#define SCREEN_WATCH
#define SCREEN_MOON
#define SCREEN_MOTION
#define SCREEN_WEATHER
// #define SCREEN_GEO
// #define SCREEN_SOLAR
// #define SCREEN_SKY
// #define SCREEN_RECORDER
// #define SCREEN_QR
#define SCREEN_CALCULATOR
#define SCREEN_STOPWATCH
#define SCREEN_TIMER

enum class ScreenId : uint8_t {
#ifdef SCREEN_WATCH
  Watch,
#endif
#ifdef SCREEN_MOON
  Moon,
#endif
#ifdef SCREEN_MOTION
  Motion,
#endif
#ifdef SCREEN_WEATHER
  Weather,
#endif
#ifdef SCREEN_GEO
  Geo,
#endif
#ifdef SCREEN_SOLAR
  Solar,
#endif
#ifdef SCREEN_SKY
  Sky,
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
