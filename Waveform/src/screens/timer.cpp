#include "config/screen_constants.h"
#include "core/screen_manager.h"
#include "drivers/es8311.h"
#include "screens/screen_callbacks.h"
#include <ESP_I2S.h>
#include <cmath>

extern lv_obj_t *screenRoots[];
extern I2SClass  audioI2s;
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
void noteActivity();

namespace
{
const ScreenModule kModule = {
    ScreenId::Timer,
    "Timer",
    waveformBuildTimerScreen,
    waveformRefreshTimerScreen,
    waveformEnterTimerScreen,
    waveformLeaveTimerScreen,
    waveformTickTimerScreen,
    waveformTimerScreenRoot,
};
} // namespace

const ScreenModule &timerScreenModule()
{
  return kModule;
}

// ---------------------------------------------------------------------------
// Chime synthesis
// ---------------------------------------------------------------------------

// Three ascending notes: C5, E5, G5
static constexpr float     kChimeFreqs[3]     = {523.25f, 659.25f, 783.99f};
static constexpr uint32_t  kChimeSampleRate   = 16000;
static constexpr uint32_t  kNoteMs            = 150;
static constexpr uint32_t  kGapMs             = 50;
static constexpr uint32_t  kNoteSamples       = kChimeSampleRate * kNoteMs / 1000;   // 2400
static constexpr uint32_t  kGapSamples        = kChimeSampleRate * kGapMs  / 1000;   // 800
// Buffer for one note at a time: stereo 16-bit = 4 bytes/frame
static constexpr size_t    kNoteBufFrames     = kNoteSamples;
static constexpr size_t    kNoteBufBytes      = kNoteBufFrames * 4;
static constexpr size_t    kGapBufBytes       = kGapSamples   * 4;

static int16_t  chimeBuf[kNoteBufFrames * 2]  = {};   // L+R interleaved
static int16_t  gapBuf[kGapSamples * 2]       = {};   // silence

static bool i2sChimeReady = false;

static bool initChimeI2s()
{
  audioI2s.end();
  audioI2s.setPins(AUDIO_I2S_BCLK, AUDIO_I2S_WS, AUDIO_I2S_DOUT, AUDIO_I2S_DIN, AUDIO_I2S_MCLK);
  audioI2s.setTimeout(80);
  if (!audioI2s.begin(I2S_MODE_STD,
                      kChimeSampleRate,
                      I2S_DATA_BIT_WIDTH_16BIT,
                      I2S_SLOT_MODE_STEREO)) {
    return false;
  }
  return true;
}

static void teardownChimeI2s()
{
  audioI2s.end();
  i2sChimeReady = false;
}

// Fills chimeBuf with a sine wave at freq, with a simple linear fade-out in the last 20%
static void synthesizeNote(float freq)
{
  const float twoPiF = 2.0f * (float)M_PI * freq / (float)kChimeSampleRate;
  const uint32_t fadeStart = (uint32_t)(kNoteSamples * 0.80f);
  for (uint32_t i = 0; i < kNoteSamples; ++i) {
    float env = 1.0f;
    if (i >= fadeStart) {
      env = 1.0f - (float)(i - fadeStart) / (float)(kNoteSamples - fadeStart);
    }
    int16_t sample = (int16_t)(sinf(twoPiF * (float)i) * 28000.0f * env);
    chimeBuf[i * 2]     = sample;  // L
    chimeBuf[i * 2 + 1] = sample;  // R
  }
}

// Play the full three-note chime synchronously. Called from a chime tick loop
// spread across multiple tick() calls to avoid long blocking.
// Returns false if I2S unavailable — callers handle graceful degradation.
static bool playChimeNote(int noteIndex)
{
  if (!i2sChimeReady) {
    return false;
  }
  synthesizeNote(kChimeFreqs[noteIndex]);
  audioI2s.write(reinterpret_cast<uint8_t *>(chimeBuf), kNoteBufBytes);
  audioI2s.write(reinterpret_cast<uint8_t *>(gapBuf),   kGapBufBytes);
  return true;
}

// ---------------------------------------------------------------------------
// Timer state
// ---------------------------------------------------------------------------

enum class TimerState { Set, Running, Alarm };

struct TimerUi
{
  lv_obj_t *screen        = nullptr;
  lv_obj_t *minsLabel     = nullptr;
  lv_obj_t *secsLabel     = nullptr;
  lv_obj_t *colonLabel    = nullptr;
  lv_obj_t *minsArea      = nullptr;
  lv_obj_t *secsArea      = nullptr;
  lv_obj_t *goButton      = nullptr;
  lv_obj_t *goLabel       = nullptr;

  TimerState state         = TimerState::Set;
  uint8_t    setMins       = 0;
  uint8_t    setSecs       = 0;
  uint32_t   remainingMs   = 0;
  uint32_t   lastTickMs    = 0;

  // Drag tracking
  int16_t    minsDragStartY = 0;
  int16_t    secsDragStartY = 0;
  int16_t    minsDragAccum  = 0;
  int16_t    secsDragAccum  = 0;

  // Alarm state
  int        chimeRepeat    = 0;   // which repeat cycle (0–4)
  int        chimeNote      = 0;   // which note within cycle (0–2), -1 = idle
  uint32_t   chimeNextMs    = 0;   // when to play the next note
  bool       alarmPulse     = false;
  uint32_t   lastPulseMs    = 0;
};

static TimerUi   timer;
static bool      timerBuilt = false;

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------

static void updateTimerDisplay()
{
  char mBuf[4], sBuf[4];
  uint32_t totalSecs = timer.remainingMs / 1000;
  uint8_t  dispMins  = (uint8_t)(totalSecs / 60);
  uint8_t  dispSecs  = (uint8_t)(totalSecs % 60);

  if (timer.state == TimerState::Set) {
    dispMins = timer.setMins;
    dispSecs = timer.setSecs;
  }

  snprintf(mBuf, sizeof(mBuf), "%02d", dispMins);
  snprintf(sBuf, sizeof(sBuf), "%02d", dispSecs);
  lv_label_set_text(timer.minsLabel, mBuf);
  lv_label_set_text(timer.secsLabel, sBuf);
}

static void setTimerColor(lv_color_t col)
{
  lv_obj_set_style_text_color(timer.minsLabel, col, 0);
  lv_obj_set_style_text_color(timer.secsLabel, col, 0);
  lv_obj_set_style_text_color(timer.colonLabel, col, 0);
}

// ---------------------------------------------------------------------------
// Arrow tap callbacks
// ---------------------------------------------------------------------------

static void stepMins(int delta)
{
  if (timer.state != TimerState::Set) return;
  int val = (int)timer.setMins + delta;
  if (val < 0) val = 0;
  if (val > 59) val = 59;
  timer.setMins = (uint8_t)val;
  updateTimerDisplay();
  noteActivity();
}

static void stepSecs(int delta)
{
  if (timer.state != TimerState::Set) return;
  int val = (int)timer.setSecs + delta;
  if (val < 0) val = 0;
  if (val > 59) val = 59;
  timer.setSecs = (uint8_t)val;
  updateTimerDisplay();
  noteActivity();
}

static void onMinsUp(lv_event_t *)   { stepMins(+1); }
static void onMinsDown(lv_event_t *) { stepMins(-1); }
static void onSecsUp(lv_event_t *)   { stepSecs(+1); }
static void onSecsDown(lv_event_t *) { stepSecs(-1); }

// ---------------------------------------------------------------------------
// Drag stepper callbacks
// ---------------------------------------------------------------------------

static void onMinsDrag(lv_event_t *e)
{
  if (timer.state != TimerState::Set) {
    return;
  }
  lv_indev_t *indev = lv_indev_get_act();
  if (!indev) {
    return;
  }
  lv_point_t pos;
  lv_indev_get_point(indev, &pos);

  if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
    timer.minsDragStartY = pos.y;
    timer.minsDragAccum  = 0;
    return;
  }

  int16_t delta = timer.minsDragStartY - pos.y;  // up = positive = increment
  int16_t steps = (delta - timer.minsDragAccum) / kTimerDragThresholdPx;
  if (steps != 0) {
    timer.minsDragAccum += steps * kTimerDragThresholdPx;
    int val = (int)timer.setMins + steps;
    if (val < 0) val = 0;
    if (val > 59) val = 59;
    timer.setMins = (uint8_t)val;
    updateTimerDisplay();
    noteActivity();
  }
}

static void onSecsDrag(lv_event_t *e)
{
  if (timer.state != TimerState::Set) {
    return;
  }
  lv_indev_t *indev = lv_indev_get_act();
  if (!indev) {
    return;
  }
  lv_point_t pos;
  lv_indev_get_point(indev, &pos);

  if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
    timer.secsDragStartY = pos.y;
    timer.secsDragAccum  = 0;
    return;
  }

  int16_t delta = timer.secsDragStartY - pos.y;
  int16_t steps = (delta - timer.secsDragAccum) / kTimerDragThresholdPx;
  if (steps != 0) {
    timer.secsDragAccum += steps * kTimerDragThresholdPx;
    int val = (int)timer.setSecs + steps;
    if (val < 0) val = 0;
    if (val > 59) val = 59;
    timer.setSecs = (uint8_t)val;
    updateTimerDisplay();
    noteActivity();
  }
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

// Make a transparent full-height half-screen touch zone (no visible styling)
void timerHandleTap();  // forward declaration
static void onGoButton(lv_event_t *) { timerHandleTap(); }

static lv_obj_t *makeTouchZone(lv_obj_t *parent, lv_align_t align,
                                lv_event_cb_t cb)
{
  lv_obj_t *zone = lv_obj_create(parent);
  lv_obj_remove_style_all(zone);
  constexpr lv_coord_t kInteractiveTop = 88;
  lv_obj_set_size(zone, LCD_WIDTH / 2, LCD_HEIGHT - kInteractiveTop);
  lv_obj_align(zone, align, 0, kInteractiveTop / 2);
  lv_obj_set_style_bg_opa(zone, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(zone, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(zone, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(zone, cb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(zone, cb, LV_EVENT_PRESSING, nullptr);
  return zone;
}

// Large transparent button with a centered symbol — proper finger-sized hit area
static lv_obj_t *makeArrowBtn(lv_obj_t *parent, const char *symbol,
                               lv_align_t align, int xOfs, int yOfs,
                               lv_event_cb_t cb)
{
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_set_size(btn, 100, 72);
  lv_obj_align(btn, align, xOfs, yOfs);
  lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_10, LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(btn, lvColor(255, 255, 255), LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn, 12, 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *lbl = lv_label_create(btn);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(lbl, lvColor(80, 96, 116), 0);
  lv_label_set_text(lbl, symbol);
  lv_obj_center(lbl);
  return btn;
}

static void buildTimerScreen()
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);
  lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);

  // Left half = minutes drag zone, right half = seconds drag zone
  timer.minsArea = makeTouchZone(screen, LV_ALIGN_LEFT_MID, onMinsDrag);
  timer.secsArea = makeTouchZone(screen, LV_ALIGN_RIGHT_MID, onSecsDrag);

  constexpr int kMinsX      = -60;
  constexpr int kSecsX      =  60;
  constexpr int kDigitY     = -60;
  constexpr int kArrowOffset = 72;

  // Arrow buttons — 100×72 transparent, centered on each column
  makeArrowBtn(screen, LV_SYMBOL_UP,   LV_ALIGN_CENTER, kMinsX, kDigitY - kArrowOffset, onMinsUp);
  makeArrowBtn(screen, LV_SYMBOL_UP,   LV_ALIGN_CENTER, kSecsX, kDigitY - kArrowOffset, onSecsUp);
  makeArrowBtn(screen, LV_SYMBOL_DOWN, LV_ALIGN_CENTER, kMinsX, kDigitY + kArrowOffset, onMinsDown);
  makeArrowBtn(screen, LV_SYMBOL_DOWN, LV_ALIGN_CENTER, kSecsX, kDigitY + kArrowOffset, onSecsDown);

  // Digits — parented to screen, rendered above touch zones
  timer.minsLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(timer.minsLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(timer.minsLabel, lvColor(255, 255, 255), 0);
  lv_label_set_text(timer.minsLabel, "00");
  lv_obj_align(timer.minsLabel, LV_ALIGN_CENTER, kMinsX, kDigitY);

  timer.colonLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(timer.colonLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(timer.colonLabel, lvColor(120, 120, 120), 0);
  lv_label_set_text(timer.colonLabel, ":");
  lv_obj_align(timer.colonLabel, LV_ALIGN_CENTER, 0, kDigitY - 4);

  timer.secsLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(timer.secsLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(timer.secsLabel, lvColor(255, 255, 255), 0);
  lv_label_set_text(timer.secsLabel, "00");
  lv_obj_align(timer.secsLabel, LV_ALIGN_CENTER, kSecsX, kDigitY);

  // Go / stop button — below stepper cluster with breathing room
  timer.goButton = lv_button_create(screen);
  lv_obj_set_size(timer.goButton, 100, 100);
  lv_obj_align(timer.goButton, LV_ALIGN_CENTER, 0, kDigitY + kArrowOffset + 100);
  lv_obj_set_style_radius(timer.goButton, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(timer.goButton, lvColor(30, 120, 60), 0);
  lv_obj_set_style_bg_color(timer.goButton, lvColor(40, 150, 80), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(timer.goButton, 0, 0);
  lv_obj_set_style_shadow_width(timer.goButton, 0, 0);
  lv_obj_add_event_cb(timer.goButton, onGoButton, LV_EVENT_CLICKED, nullptr);

  timer.goLabel = lv_label_create(timer.goButton);
  lv_obj_set_style_text_font(timer.goLabel, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(timer.goLabel, lvColor(255, 255, 255), 0);
  lv_label_set_text(timer.goLabel, LV_SYMBOL_PLAY);
  lv_obj_center(timer.goLabel);

  screenRoots[static_cast<size_t>(ScreenId::Timer)] = screen;
  timer.screen = screen;
  timerBuilt = true;
}

// ---------------------------------------------------------------------------
// Alarm helpers
// ---------------------------------------------------------------------------

static void startAlarm(uint32_t nowMs)
{
  timer.state       = TimerState::Alarm;
  timer.chimeRepeat = 0;
  timer.chimeNote   = 0;
  timer.chimeNextMs = nowMs;
  timer.alarmPulse  = false;
  timer.lastPulseMs = nowMs;

  i2sChimeReady = initChimeI2s();

  lv_label_set_text(timer.minsLabel, "00");
  lv_label_set_text(timer.secsLabel, "00");
}

static void cancelAlarm()
{
  teardownChimeI2s();
  timer.state     = TimerState::Set;
  timer.chimeNote = -1;
  setTimerColor(lvColor(255, 255, 255));
  updateTimerDisplay();
  if (timer.goLabel) lv_label_set_text(timer.goLabel, LV_SYMBOL_PLAY);
  if (timer.goButton) {
    lv_obj_set_style_bg_color(timer.goButton, lvColor(30, 120, 60), 0);
    lv_obj_set_style_bg_color(timer.goButton, lvColor(40, 150, 80), LV_STATE_PRESSED);
  }
}

// ---------------------------------------------------------------------------
// Public tap handler — called from main.cpp
// ---------------------------------------------------------------------------

void timerHandleTap()
{
  if (timer.state == TimerState::Alarm) {
    cancelAlarm();
    noteActivity();
    return;
  }

  if (timer.state == TimerState::Set) {
    if (timer.setMins == 0 && timer.setSecs == 0) {
      return;  // nothing set, ignore
    }
    timer.remainingMs = ((uint32_t)timer.setMins * 60 + timer.setSecs) * 1000;
    timer.lastTickMs  = millis();
    timer.state       = TimerState::Running;
    setTimerColor(lvColor(255, 255, 255));
    if (timer.goLabel) lv_label_set_text(timer.goLabel, LV_SYMBOL_STOP);
    if (timer.goButton) {
      lv_obj_set_style_bg_color(timer.goButton, lvColor(160, 30, 30), 0);
      lv_obj_set_style_bg_color(timer.goButton, lvColor(200, 50, 50), LV_STATE_PRESSED);
    }
    noteActivity();
    return;
  }

  if (timer.state == TimerState::Running) {
    // Stop and reset
    timer.state = TimerState::Set;
    setTimerColor(lvColor(255, 255, 255));
    updateTimerDisplay();
    if (timer.goLabel) lv_label_set_text(timer.goLabel, LV_SYMBOL_PLAY);
    if (timer.goButton) {
      lv_obj_set_style_bg_color(timer.goButton, lvColor(30, 120, 60), 0);
      lv_obj_set_style_bg_color(timer.goButton, lvColor(40, 150, 80), LV_STATE_PRESSED);
    }
    noteActivity();
    return;
  }
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

lv_obj_t *waveformTimerScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Timer)];
}

bool waveformBuildTimerScreen()
{
  if (!waveformTimerScreenRoot()) {
    buildTimerScreen();
  }
  return timerBuilt && waveformTimerScreenRoot() && timer.minsLabel && timer.secsLabel;
}

bool waveformRefreshTimerScreen()
{
  return timerBuilt && timer.minsLabel && timer.secsLabel;
}

void waveformEnterTimerScreen() {}

void waveformLeaveTimerScreen()
{
  if (timer.state == TimerState::Alarm) {
    cancelAlarm();
  }
}

void waveformTickTimerScreen(uint32_t nowMs)
{
  if (!timerBuilt || !timer.minsLabel) {
    return;
  }

  if (timer.state == TimerState::Running) {
    uint32_t delta = nowMs - timer.lastTickMs;
    timer.lastTickMs = nowMs;
    if (delta >= timer.remainingMs) {
      timer.remainingMs = 0;
      startAlarm(nowMs);
    } else {
      timer.remainingMs -= delta;
      updateTimerDisplay();
    }
    return;
  }

  if (timer.state == TimerState::Alarm) {
    // Pulse color
    if (nowMs - timer.lastPulseMs >= 400) {
      timer.alarmPulse  = !timer.alarmPulse;
      timer.lastPulseMs = nowMs;
      lv_color_t col    = timer.alarmPulse ? lvColor(255, 160, 40) : lvColor(120, 60, 0);
      setTimerColor(col);
    }

    // Chime sequencing — play one note per tick pass, spaced by note+gap duration
    if (timer.chimeRepeat < kTimerChimeRepeatCount && nowMs >= timer.chimeNextMs) {
      playChimeNote(timer.chimeNote);  // writes to I2S (brief blocking write)

      timer.chimeNote++;
      if (timer.chimeNote >= 3) {
        timer.chimeNote    = 0;
        timer.chimeRepeat++;
        timer.chimeNextMs  = nowMs + kNoteMs * 3 + kGapMs * 3 + 300;  // pause between repeats
      } else {
        timer.chimeNextMs  = nowMs + kNoteMs + kGapMs;
      }
    }

    if (timer.chimeRepeat >= kTimerChimeRepeatCount) {
      cancelAlarm();
    }
  }
}
