#include "config/screen_constants.h"
#include "core/screen_manager.h"
#include "screens/screen_callbacks.h"

extern lv_obj_t *screenRoots[];
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
void noteActivity();

namespace
{
const ScreenModule kModule = {
    ScreenId::Stopwatch,
    "Stopwatch",
    waveformBuildStopwatchScreen,
    waveformRefreshStopwatchScreen,
    waveformEnterStopwatchScreen,
    waveformLeaveStopwatchScreen,
    waveformTickStopwatchScreen,
    waveformStopwatchScreenRoot,
};
} // namespace

const ScreenModule &stopwatchScreenModule()
{
  return kModule;
}

enum class StopwatchState { Reset, Running, Stopped };

struct StopwatchUi
{
  lv_obj_t *screen    = nullptr;
  lv_obj_t *timeLabel = nullptr;

  StopwatchState state   = StopwatchState::Reset;
  uint32_t       elapsedMs = 0;
  uint32_t       startMs   = 0;
};

static StopwatchUi sw;
static bool stopwatchBuilt = false;

static void updateDisplay(uint32_t totalMs)
{
  uint32_t centis = (totalMs / 10) % 100;
  uint32_t secs   = (totalMs / 1000) % 60;
  uint32_t mins   = (totalMs / 60000) % 100;

  char buf[12];
  snprintf(buf, sizeof(buf), "%02lu:%02lu.%02lu", (unsigned long)mins, (unsigned long)secs, (unsigned long)centis);
  lv_label_set_text(sw.timeLabel, buf);
}

static void buildStopwatchScreen()
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);
  lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);

  sw.timeLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(sw.timeLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(sw.timeLabel, lvColor(96, 128, 160), 0);
  lv_label_set_text(sw.timeLabel, "00:00.00");
  lv_obj_center(sw.timeLabel);

  screenRoots[static_cast<size_t>(ScreenId::Stopwatch)] = screen;
  sw.screen = screen;
  stopwatchBuilt = true;
}

// Tap: Reset → Running → Stopped → Reset → ...
void stopwatchHandleTap()
{
  if (sw.state == StopwatchState::Reset) {
    sw.startMs = millis();
    sw.elapsedMs = 0;
    sw.state   = StopwatchState::Running;
    lv_obj_set_style_text_color(sw.timeLabel, lvColor(255, 255, 255), 0);
  } else if (sw.state == StopwatchState::Running) {
    sw.elapsedMs = millis() - sw.startMs;
    sw.state     = StopwatchState::Stopped;
    lv_obj_set_style_text_color(sw.timeLabel, lvColor(96, 128, 160), 0);
  } else {
    // Stopped → reset
    sw.state     = StopwatchState::Reset;
    sw.elapsedMs = 0;
    sw.startMs   = 0;
    lv_obj_set_style_text_color(sw.timeLabel, lvColor(96, 128, 160), 0);
    updateDisplay(0);
  }
  noteActivity();
}

void stopwatchHandleLongPress() {}

lv_obj_t *waveformStopwatchScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Stopwatch)];
}

bool waveformBuildStopwatchScreen()
{
  if (!waveformStopwatchScreenRoot()) {
    buildStopwatchScreen();
  }
  return stopwatchBuilt && waveformStopwatchScreenRoot() && sw.timeLabel;
}

bool waveformRefreshStopwatchScreen()
{
  return stopwatchBuilt && sw.timeLabel;
}

void waveformEnterStopwatchScreen() {}

void waveformLeaveStopwatchScreen()
{
  // Pause if running when navigating away
  if (sw.state == StopwatchState::Running) {
    sw.elapsedMs = millis() - sw.startMs;
    sw.state     = StopwatchState::Stopped;
    lv_obj_set_style_text_color(sw.timeLabel, lvColor(96, 128, 160), 0);
  }
}

void waveformTickStopwatchScreen(uint32_t nowMs)
{
  if (!stopwatchBuilt || !sw.timeLabel) {
    return;
  }
  if (sw.state == StopwatchState::Running) {
    updateDisplay(nowMs - sw.startMs);
  }
}
