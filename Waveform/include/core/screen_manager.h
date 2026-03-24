#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "state/runtime_state.h"

enum class ScreenId : uint8_t
{
  Watch,
  Motion,
  Weather,
  Geo,
  Solar,
  Sky,
  Recorder,
  Qr,
  Calculator,
};

constexpr size_t kScreenCount = 9;

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
