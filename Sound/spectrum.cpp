#include "screen_manager.h"

#ifdef SCREEN_SPECTRUM

#include "pin_config.h"
#include "screen_constants.h"
#include "screen_callbacks.h"
#include "spectrum_core.h"

#include <Arduino.h>
#include <lvgl.h>
#include <math.h>

extern lv_obj_t *screenRoots[];
void noteActivity();

namespace
{

// ─── FFT config ────────────────────────────────────────────────────────────
constexpr size_t   kBarCount      = 32;
constexpr float    kPeakDecayRate = 0.92f;   // per-frame multiplier

// ─── Module registration ────────────────────────────────────────────────────
const ScreenModule kModule = {
    ScreenId::Spectrum,
    "Spectrum",
    waveformBuildSpectrumScreen,
    waveformRefreshSpectrumScreen,
    waveformEnterSpectrumScreen,
    waveformLeaveSpectrumScreen,
    waveformTickSpectrumScreen,
    waveformSpectrumScreenRoot,
    waveformDestroySpectrumScreen,
};

// ─── UI ─────────────────────────────────────────────────────────────────────
struct SpectrumUi
{
  lv_obj_t *screen    = nullptr;
  lv_obj_t *statusLabel = nullptr;
  lv_obj_t *bars[kBarCount]     = {};
  lv_obj_t *peaks[kBarCount]    = {};
};

SpectrumUi gUi;
bool gBuilt = false;
bool gAudioReady = false;
bool gAudioStartPending = false;
uint32_t gLastStatusMs = 0;
char gStatusText[96] = "Starting mic...";

// ─── Signal state ────────────────────────────────────────────────────────────
float  gBarHeights[kBarCount]  = {};
float  gPeakHeights[kBarCount] = {};
float  gSourceBands[spectrum_core::kBandCount] = {};
float  gLastRms = 0.0f;

// ─── Helpers ─────────────────────────────────────────────────────────────────
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b)
{
  return lv_color_make(r, g, b);
}

void setStatusText(const char *text)
{
  snprintf(gStatusText, sizeof(gStatusText), "%s", text ? text : "");
  if (gUi.statusLabel) {
    lv_label_set_text(gUi.statusLabel, gStatusText);
  }
}

float remapBand(size_t barIndex)
{
  if (spectrum_core::kBandCount == 0) {
    return 0.0f;
  }

  float position = static_cast<float>(barIndex) * static_cast<float>(spectrum_core::kBandCount - 1) /
                   static_cast<float>(kBarCount - 1);
  size_t left = static_cast<size_t>(position);
  size_t right = min(left + 1, spectrum_core::kBandCount - 1);
  float mix = position - static_cast<float>(left);
  return gSourceBands[left] * (1.0f - mix) + gSourceBands[right] * mix;
}

// Bar colour: cool blue → cyan → green → yellow → red across frequency
lv_color_t barColor(size_t barIndex)
{
  float t = static_cast<float>(barIndex) / static_cast<float>(kBarCount - 1);
  uint8_t r, g, b;
  if (t < 0.25f) {
    float u = t / 0.25f;
    r = 0; g = static_cast<uint8_t>(u * 200); b = 255;
  } else if (t < 0.5f) {
    float u = (t - 0.25f) / 0.25f;
    r = 0; g = static_cast<uint8_t>(200 + u * 55); b = static_cast<uint8_t>(255 * (1.0f - u));
  } else if (t < 0.75f) {
    float u = (t - 0.5f) / 0.25f;
    r = static_cast<uint8_t>(u * 255); g = 255; b = 0;
  } else {
    float u = (t - 0.75f) / 0.25f;
    r = 255; g = static_cast<uint8_t>(255 * (1.0f - u)); b = 0;
  }
  return lvColor(r, g, b);
}

// ─── Build ───────────────────────────────────────────────────────────────────
void buildSpectrumScreen()
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  lv_obj_set_size(screen, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_style_bg_color(screen, lvColor(4, 6, 12), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(screen, 0, 0);
  lv_obj_set_style_pad_all(screen, 0, 0);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  // Header
  lv_obj_t *eyebrow = lv_label_create(screen);
  lv_obj_set_style_text_font(eyebrow, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(eyebrow, lvColor(80, 60, 100), 0);
  lv_label_set_text(eyebrow, "WAVEFORM");
  lv_obj_align(eyebrow, LV_ALIGN_TOP_LEFT, 20, 18);

  lv_obj_t *headline = lv_label_create(screen);
  lv_obj_set_style_text_font(headline, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(headline, lvColor(244, 244, 252), 0);
  lv_label_set_text(headline, "Spectrum");
  lv_obj_align(headline, LV_ALIGN_TOP_LEFT, 20, 40);

  gUi.statusLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(gUi.statusLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(gUi.statusLabel, lvColor(120, 80, 140), 0);
  lv_label_set_text(gUi.statusLabel, "Starting mic...");
  lv_obj_align(gUi.statusLabel, LV_ALIGN_TOP_LEFT, 20, 76);

  // Bars
  constexpr lv_coord_t kAreaTop    = 106;
  constexpr lv_coord_t kAreaBottom = 420;
  constexpr lv_coord_t kAreaHeight = kAreaBottom - kAreaTop;
  constexpr lv_coord_t kBarW       = (LCD_WIDTH - 24) / (lv_coord_t)kBarCount - 1;
  constexpr lv_coord_t kGap        = 1;

  for (size_t i = 0; i < kBarCount; ++i) {
    lv_color_t col = barColor(i);
    lv_coord_t bx  = 12 + static_cast<lv_coord_t>(i) * (kBarW + kGap);

    // Bar
    lv_obj_t *bar = lv_obj_create(screen);
    lv_obj_set_style_bg_color(bar, col, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_radius(bar, 2, 0);
    lv_obj_set_size(bar, kBarW, 2);
    lv_obj_set_pos(bar, bx, kAreaBottom - 2);
    gUi.bars[i] = bar;

    // Peak dot
    lv_obj_t *pk = lv_obj_create(screen);
    lv_obj_set_style_bg_color(pk, lvColor(255, 255, 255), 0);
    lv_obj_set_style_bg_opa(pk, LV_OPA_80, 0);
    lv_obj_set_style_border_width(pk, 0, 0);
    lv_obj_set_style_pad_all(pk, 0, 0);
    lv_obj_set_style_radius(pk, 1, 0);
    lv_obj_set_size(pk, kBarW, 2);
    lv_obj_set_pos(pk, bx, kAreaBottom - 2);
    gUi.peaks[i] = pk;
  }

  gUi.screen = screen;
  screenRoots[static_cast<size_t>(ScreenId::Spectrum)] = screen;
  gBuilt = true;
}

void updateBars()
{
  constexpr lv_coord_t kAreaTop    = 106;
  constexpr lv_coord_t kAreaBottom = 420;
  constexpr lv_coord_t kAreaHeight = kAreaBottom - kAreaTop;
  constexpr lv_coord_t kBarW       = (LCD_WIDTH - 24) / (lv_coord_t)kBarCount - 1;
  constexpr lv_coord_t kGap        = 1;

  for (size_t i = 0; i < kBarCount; ++i) {
    if (!gUi.bars[i] || !gUi.peaks[i]) continue;

    lv_coord_t barH = static_cast<lv_coord_t>(gBarHeights[i] * kAreaHeight);
    if (barH < 2) barH = 2;

    lv_coord_t bx = 12 + static_cast<lv_coord_t>(i) * (kBarW + kGap);
    lv_obj_set_size(gUi.bars[i], kBarW, barH);
    lv_obj_set_pos(gUi.bars[i], bx, kAreaBottom - barH);

    lv_coord_t peakY = kAreaBottom - static_cast<lv_coord_t>(gPeakHeights[i] * kAreaHeight) - 2;
    lv_obj_set_pos(gUi.peaks[i], bx, peakY);
  }
}

} // namespace

// ─── Public interface ────────────────────────────────────────────────────────
const ScreenModule &spectrumScreenModule()
{
  return kModule;
}

lv_obj_t *waveformSpectrumScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Spectrum)];
}

bool waveformBuildSpectrumScreen()
{
  if (!waveformSpectrumScreenRoot()) {
    buildSpectrumScreen();
  }
  return gBuilt && waveformSpectrumScreenRoot();
}

bool waveformRefreshSpectrumScreen()
{
  return gBuilt;
}

void waveformEnterSpectrumScreen()
{
  memset(gBarHeights,  0, sizeof(gBarHeights));
  memset(gPeakHeights, 0, sizeof(gPeakHeights));
  memset(gSourceBands, 0, sizeof(gSourceBands));
  gLastRms = 0.0f;
  gLastStatusMs = 0;
  setStatusText("Starting mic...");
  gAudioStartPending = true;
}

void waveformLeaveSpectrumScreen()
{
  spectrum_core::end();
  gAudioReady = false;
}

void waveformTickSpectrumScreen(uint32_t nowMs)
{
  static uint32_t lastTickMs = 0;
  if (!gBuilt) return;
  if (gAudioStartPending) {
    gAudioStartPending = false;
    gAudioReady = spectrum_core::begin();
    setStatusText(gAudioReady ? "Listening" : "Mic error");
  }
  if (!gAudioReady) return;
  if (nowMs - lastTickMs < 40) return;
  lastTickMs = nowMs;

  if (!spectrum_core::poll(gSourceBands, spectrum_core::kBandCount, &gLastRms)) {
    setStatusText("Mic read error");
    return;
  }

  for (size_t i = 0; i < kBarCount; ++i) {
    gBarHeights[i] = remapBand(i);
    if (gBarHeights[i] > gPeakHeights[i]) {
      gPeakHeights[i] = gBarHeights[i];
    } else {
      gPeakHeights[i] *= kPeakDecayRate;
    }
  }

  if (nowMs - gLastStatusMs >= 1000) {
    snprintf(gStatusText, sizeof(gStatusText), "Listening  rms %.0f", gLastRms);
    if (gUi.statusLabel) {
      lv_label_set_text(gUi.statusLabel, gStatusText);
    }
    gLastStatusMs = nowMs;
  }

  updateBars();
}

void waveformDestroySpectrumScreen()
{
  spectrum_core::end();
  if (gUi.screen) {
    lv_obj_delete(gUi.screen);
  }
  screenRoots[static_cast<size_t>(ScreenId::Spectrum)] = nullptr;
  gUi = SpectrumUi{};
  gBuilt = false;
}

#endif // SCREEN_SPECTRUM
