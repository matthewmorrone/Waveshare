# Stopwatch & Timer Screens — Design Spec
**Date:** 2026-03-24
**Device:** Waveshare ESP32 wearable, 368×448 AMOLED, LVGL, ES8311 audio codec

---

## Overview

Two new self-contained screen modules added to the Waveform firmware: a stopwatch and a countdown timer. Both follow the existing screen module pattern (`ScreenModule` struct, `#define SCREEN_*` guard, registered in `screen_manager.h` / `screen_manager.cpp` / `screen_modules.h`).

---

## Stopwatch Screen (`SCREEN_STOPWATCH`)

### Layout
- Black background, full screen (368×448)
- Single large label centered vertically and horizontally displaying elapsed time in `MM:SS.cc` format (centiseconds)
- Font: `lv_font_montserrat_48`
- Color: white when running, dimmed gray (`#6080A0`) when stopped/reset

### Interaction
- **Tap while stopped/reset:** start counting
- **Tap while running:** pause
- **Long-press while stopped (not reset):** reset to `00:00.00`
  - Long-press threshold: reuse `kTapMaxDurationMs` — any touch held longer than that while stopped = reset
- Side buttons still cycle screens normally (via existing main.cpp nav)

### State machine
```
RESET → (tap) → RUNNING → (tap) → STOPPED → (tap) → RUNNING
                                            → (long-press) → RESET
```

### Timing
- Elapsed time tracked as `uint32_t elapsedMs` + `uint32_t startMs`
- Updated in `tick(uint32_t nowMs)`: if running, `displayMs = elapsedMs + (nowMs - startMs)`
- No LVGL timers — driven entirely by `tick()`

### Files
- `Waveform/src/screens/stopwatch.cpp`
- No new header needed — declarations added to `screen_callbacks.h`

---

## Timer Screen (`SCREEN_TIMER`)

### Layout
- Black background, full screen (368×448)
- Two large digit groups centered horizontally, separated by `:`
  - Left: minutes (`00`–`99`), font `lv_font_montserrat_48`
  - Right: seconds (`00`–`59`), font `lv_font_montserrat_48`
- Separator `:` label between them, same font, dimmed color
- Below digits: a circular tap-target button (~100px diameter)
  - Shows `LV_SYMBOL_PLAY` when stopped, `LV_SYMBOL_STOP` when running
  - Font: `lv_font_montserrat_24`

### Stepper interaction (set mode only)
- Each digit group is a draggable `lv_obj` with `LV_EVENT_PRESSING` handler
- Tracks cumulative drag delta Y; every 30px threshold crossing increments/decrements the value
- MM wraps 0–99, SS wraps 0–59
- Drag disabled once timer is running

### Timer run mode
- Tap play button → timer counts down from set MM:SS
- Remaining time tracked as `uint32_t remainingMs`, decremented each `tick()`
- Tap stop button (or screen tap) while running → stop and reset to original set values
- At zero: enter **alarm state**

### Alarm state
- Digits display `00:00`, color pulses white↔accent
- Ascending three-note chime (C5→E5→G5) plays and repeats up to 5 times
- Tap anywhere → cancel alarm, return to set mode with previous MM:SS restored
- After 5 complete cycles with no tap → alarm stops automatically, screen returns to set mode

### Chime implementation
- Generated as inline PCM in `stopTimer()` / `tickTimerAlarm()`
- Sample rate: 16000 Hz (matches recorder, same I2S config)
- Each note: ~150ms = 2400 samples, 16-bit signed sine wave
- Frequencies: C5 = 523 Hz, E5 = 659 Hz, G5 = 784 Hz
- Gap between notes: ~50ms silence (800 samples)
- Full chime duration: ~600ms per cycle, ~3s total for 5 cycles
- Written via `audioI2s.write()` — same path as recorder playback
- I2S initialized on alarm start (output mode only), torn down after alarm ends or is cancelled
- If I2S fails to init, alarm state still triggers visually (silent graceful degradation)

### Files
- `Waveform/src/screens/timer.cpp`
- No new header — declarations added to `screen_callbacks.h`

---

## Registration

### `screen_manager.h`
Add:
```cpp
#define SCREEN_STOPWATCH
#define SCREEN_TIMER
```
Add to `ScreenId` enum:
```cpp
#ifdef SCREEN_STOPWATCH
  Stopwatch,
#endif
#ifdef SCREEN_TIMER
  Timer,
#endif
```

### `screen_manager.cpp`
Add to `kModulesArr`:
```cpp
#ifdef SCREEN_STOPWATCH
    &stopwatchScreenModule(),
#endif
#ifdef SCREEN_TIMER
    &timerScreenModule(),
#endif
```

### `screen_modules.h`
Add:
```cpp
#ifdef SCREEN_STOPWATCH
const ScreenModule &stopwatchScreenModule();
#endif
#ifdef SCREEN_TIMER
const ScreenModule &timerScreenModule();
#endif
```

### `screen_callbacks.h`
Add declarations for all 12 lifecycle functions (root, build, refresh, enter, leave, tick) for each new screen.

---

## Constants (in `screen_constants.h`)
```cpp
// Stopwatch
constexpr uint32_t kStopwatchLongPressMs = 600;

// Timer
constexpr int kTimerDragThresholdPx      = 30;
constexpr int kTimerButtonDiameter       = 100;
constexpr uint32_t kTimerChimeSampleRate = 16000;
constexpr uint32_t kTimerChimeNoteMs     = 150;
constexpr uint32_t kTimerChimeGapMs      = 50;
constexpr int kTimerChimeRepeatCount     = 5;
```

---

## Out of scope
- No lap functionality on stopwatch
- No multiple timer presets
- No persistence of stopwatch/timer state across screen switches or sleep
