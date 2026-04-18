#include "screen_manager.h"

#include "pin_config.h"
#include "screen_modules.h"

extern lv_obj_t *screenRoots[];

// --- Active screens --- controlled by #defines in screen_manager.h
static const ScreenModule *const kModulesArr[] = {
#ifdef SCREEN_LAUNCHER
    &launcherScreenModule(),
#endif
#ifdef SCREEN_WATCH
    &watchScreenModule(),
#endif
#ifdef SCREEN_CALENDAR
    &calendarScreenModule(),
#endif
#ifdef SCREEN_MOTION
    &motionScreenModule(),
#endif
#ifdef SCREEN_CUBE
    &cubeScreenModule(),
#endif
#ifdef SCREEN_IMU
    &imuScreenModule(),
#endif
#ifdef SCREEN_WEATHER
    &weatherScreenModule(),
#endif
#ifdef SCREEN_GPS
    &gpsScreenModule(),
#endif
#ifdef SCREEN_PLANETS
    &planetsScreenModule(),
#endif
#ifdef SCREEN_STARS
    &starsScreenModule(),
#endif
#ifdef SCREEN_STARMAP
    &starmapScreenModule(),
#endif
#ifdef SCREEN_RECORDER
    &recorderScreenModule(),
#endif
#ifdef SCREEN_QR
    &qrScreenModule(),
#endif
#ifdef SCREEN_CALCULATOR
    &calculatorScreenModule(),
#endif
#ifdef SCREEN_STOPWATCH
    &stopwatchScreenModule(),
#endif
#ifdef SCREEN_TIMER
    &timerScreenModule(),
#endif
#ifdef SCREEN_SYSTEM
    &systemScreenModule(),
#endif
#ifdef SCREEN_SETTINGS
    &settingsScreenModule(),
#endif
#ifdef SCREEN_SPECTRUM
    &spectrumScreenModule(),
#endif
#ifdef SCREEN_RADIO
    &radioScreenModule(),
#endif
#ifdef SCREEN_LOCKET
    &locketScreenModule(),
#endif
#ifdef SCREEN_ORB
    &orbScreenModule(),
#endif
};

#include <stdio.h>
#include <string.h>

namespace
{
struct ScreenRuntime
{
  bool built = false;
  bool enabled = true;
  char failureReason[96] = "";
};

ScreenRuntime gScreenRuntimes[sizeof(kModulesArr) / sizeof(kModulesArr[0])] = {};

lv_obj_t *gFallbackScreen = nullptr;
lv_obj_t *gFallbackTitleLabel = nullptr;
lv_obj_t *gFallbackBodyLabel = nullptr;
lv_obj_t *gFallbackFooterLabel = nullptr;

size_t screenIndex(ScreenId id)
{
  return static_cast<size_t>(id);
}

ScreenRuntime &screenRuntime(ScreenId id)
{
  return gScreenRuntimes[screenIndex(id)];
}

lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b)
{
  return lv_color_make(r, g, b);
}

void applyRootStyle(lv_obj_t *obj)
{
  lv_obj_set_size(obj, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_style_bg_color(obj, lvColor(0, 0, 0), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_outline_width(obj, 0, 0);
  lv_obj_set_style_radius(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void disableScreen(ScreenId id, const char *reason)
{
  ScreenRuntime &runtime = screenRuntime(id);
  runtime.enabled = false;
  runtime.built = false;
  snprintf(runtime.failureReason,
           sizeof(runtime.failureReason),
           "%s",
           (reason && reason[0] != '\0') ? reason : "Screen disabled after a failed lifecycle step.");
}

void ensureFallbackScreen()
{
  if (gFallbackScreen) {
    return;
  }

  gFallbackScreen = lv_obj_create(nullptr);
  applyRootStyle(gFallbackScreen);

  gFallbackTitleLabel = lv_label_create(gFallbackScreen);
  lv_obj_set_width(gFallbackTitleLabel, LCD_WIDTH - 40);
  lv_obj_set_style_text_align(gFallbackTitleLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(gFallbackTitleLabel, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(gFallbackTitleLabel, lvColor(255, 255, 255), 0);
  lv_obj_align(gFallbackTitleLabel, LV_ALIGN_TOP_MID, 0, 56);

  gFallbackBodyLabel = lv_label_create(gFallbackScreen);
  lv_obj_set_width(gFallbackBodyLabel, LCD_WIDTH - 56);
  lv_obj_set_style_text_align(gFallbackBodyLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(gFallbackBodyLabel, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(gFallbackBodyLabel, lvColor(176, 184, 196), 0);
  lv_label_set_long_mode(gFallbackBodyLabel, LV_LABEL_LONG_WRAP);
  lv_obj_align(gFallbackBodyLabel, LV_ALIGN_CENTER, 0, 8);

  gFallbackFooterLabel = lv_label_create(gFallbackScreen);
  lv_obj_set_width(gFallbackFooterLabel, LCD_WIDTH - 56);
  lv_obj_set_style_text_align(gFallbackFooterLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(gFallbackFooterLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(gFallbackFooterLabel, lvColor(108, 120, 136), 0);
  lv_label_set_long_mode(gFallbackFooterLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(gFallbackFooterLabel, "Use the side controls to move to another screen.");
  lv_obj_align(gFallbackFooterLabel, LV_ALIGN_BOTTOM_MID, 0, -42);
}
} // namespace

const size_t kScreenCount = sizeof(kModulesArr) / sizeof(kModulesArr[0]);

const ScreenModule &screenModule(ScreenId id)
{
  return *kModulesArr[screenIndex(id)];
}

const ScreenModule &screenModuleByIndex(size_t index)
{
  return *kModulesArr[index];
}

bool screenManagerEnsureBuilt(ScreenId id)
{
  ScreenRuntime &runtime = screenRuntime(id);
  if (!runtime.enabled) {
    return false;
  }

  if (screenManagerRoot(id)) {
    runtime.built = true;
    return true;
  }

  const ScreenModule &module = screenModule(id);
  if (!module.build || !module.build()) {
    snprintf(runtime.failureReason, sizeof(runtime.failureReason), "The screen module could not build its LVGL root.");
    return false;
  }

  if (!screenManagerRoot(id)) {
    snprintf(runtime.failureReason, sizeof(runtime.failureReason), "The screen module built without returning a usable root.");
    return false;
  }

  runtime.built = true;
  runtime.failureReason[0] = '\0';
  return true;
}

bool screenManagerRefresh(ScreenId id)
{
  if (!screenManagerEnsureBuilt(id)) {
    return false;
  }

  const ScreenModule &module = screenModule(id);
  if (!module.refresh) {
    return true;
  }

  if (!module.refresh()) {
    disableScreen(id, "The screen module failed its refresh health check.");
    return false;
  }

  return true;
}

void screenManagerEnter(ScreenId id)
{
  if (!screenManagerEnsureBuilt(id)) {
    return;
  }

  const ScreenModule &module = screenModule(id);
  if (module.enter) {
    module.enter();
  }
}

void screenManagerLeave(ScreenId id)
{
  ScreenRuntime &runtime = screenRuntime(id);
  if (!runtime.enabled) {
    return;
  }

  const ScreenModule &module = screenModule(id);
  if (module.leave) {
    module.leave();
  }
}

void screenManagerTick(ScreenId id, uint32_t nowMs)
{
  ScreenRuntime &runtime = screenRuntime(id);
  if (!runtime.enabled) {
    return;
  }

  const ScreenModule &module = screenModule(id);
  if (module.tick) {
    module.tick(nowMs);
  }
}

bool screenManagerIsEnabled(ScreenId id)
{
  return screenRuntime(id).enabled;
}

bool screenManagerHasFailure(ScreenId id)
{
  return screenRuntime(id).failureReason[0] != '\0';
}

const char *screenManagerFailureReason(ScreenId id)
{
  ScreenRuntime &runtime = screenRuntime(id);
  return runtime.failureReason[0] != '\0' ? runtime.failureReason : "Screen unavailable.";
}

lv_obj_t *screenManagerRoot(ScreenId id)
{
  const ScreenModule &module = screenModule(id);
  return module.root ? module.root() : nullptr;
}

size_t screenManagerNextEnabledIndex(size_t fromIndex)
{
  for (size_t offset = 1; offset <= kScreenCount; ++offset) {
    size_t candidate = (fromIndex + offset) % kScreenCount;
    if (screenManagerIsEnabled(static_cast<ScreenId>(candidate))) {
      return candidate;
    }
  }

  return fromIndex;
}

size_t screenManagerPreviousEnabledIndex(size_t fromIndex)
{
  for (size_t offset = 1; offset <= kScreenCount; ++offset) {
    size_t candidate = (fromIndex + kScreenCount - offset) % kScreenCount;
    if (screenManagerIsEnabled(static_cast<ScreenId>(candidate))) {
      return candidate;
    }
  }

  return fromIndex;
}

void screenManagerDestroy(ScreenId id)
{
  ScreenRuntime &runtime = screenRuntime(id);
  if (!runtime.built) return;

  const ScreenModule &module = screenModule(id);
  lv_obj_t *root = module.root ? module.root() : nullptr;

  if (module.destroy) {
    // Screen handles its own teardown (nulls internal pointers, deletes object)
    module.destroy();
  } else if (root) {
    // Generic teardown: delete the LVGL tree and null the screenRoots entry
    // so the build() function will recreate it on next visit
    lv_obj_delete(root);
    screenRoots[static_cast<size_t>(id)] = nullptr;
  }

  runtime.built = false;
  runtime.failureReason[0] = '\0';
}

void screenManagerShowFallback(ScreenId id, const char *reason)
{
  ensureFallbackScreen();

  const ScreenModule &module = screenModule(id);
  lv_label_set_text_fmt(gFallbackTitleLabel, "%s unavailable", module.name);
  lv_label_set_text(gFallbackBodyLabel,
                    (reason && reason[0] != '\0') ? reason : "This screen was disabled.");
  lv_screen_load(gFallbackScreen);
}
