#include "core/screen_manager.h"

#ifdef SCREEN_SPECTRUM

#include "config/pin_config.h"
#include "config/screen_constants.h"
#include "drivers/es8311.h"
#include "screens/screen_callbacks.h"

#include <Arduino.h>
#include <ESP_I2S.h>
#include <arduinoFFT.h>
#include <lvgl.h>
#include <math.h>

extern lv_obj_t *screenRoots[];
extern I2SClass audioI2s;
void noteActivity();

namespace
{

// ─── FFT config ────────────────────────────────────────────────────────────
constexpr uint32_t kSampleRate    = 16000;
constexpr size_t   kFftSamples    = 256;
constexpr size_t   kBarCount      = 32;
constexpr float    kPeakDecayRate = 0.92f;   // per-frame multiplier
constexpr float    kBarDecayRate  = 0.75f;
constexpr float    kGain          = 120.0f;  // visual amplification

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

// ─── Signal state ────────────────────────────────────────────────────────────
double vReal[kFftSamples];
double vImag[kFftSamples];
float  gBarHeights[kBarCount]  = {};
float  gPeakHeights[kBarCount] = {};

ArduinoFFT<double> fft(vReal, vImag, kFftSamples, kSampleRate);

es8311_handle_t gCodec = nullptr;

// ─── Helpers ─────────────────────────────────────────────────────────────────
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b)
{
  return lv_color_make(r, g, b);
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

// ─── Audio init ──────────────────────────────────────────────────────────────
bool initAudio()
{
  audioI2s.end();
  audioI2s.setPins(AUDIO_I2S_BCLK, AUDIO_I2S_WS, AUDIO_I2S_DOUT, AUDIO_I2S_DIN, AUDIO_I2S_MCLK);
  audioI2s.setTimeout(100);
  if (!audioI2s.begin(I2S_MODE_STD,
                      kSampleRate,
                      I2S_DATA_BIT_WIDTH_16BIT,
                      I2S_SLOT_MODE_MONO,
                      I2S_STD_SLOT_LEFT)) {
    return false;
  }

  if (!gCodec) {
    gCodec = es8311_create(0, ES8311_ADDRRES_0);
  }
  if (!gCodec) {
    audioI2s.end();
    return false;
  }

  const es8311_clock_config_t clk = {
      .mclk_inverted      = false,
      .sclk_inverted      = false,
      .mclk_from_mclk_pin = true,
      .mclk_frequency     = (int)(kSampleRate * 256U),
      .sample_frequency   = (int)(kSampleRate),
  };

  esp_err_t err = es8311_init(gCodec, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
  if (err == ESP_OK) err = es8311_sample_frequency_config(gCodec, clk.mclk_frequency, clk.sample_frequency);
  if (err == ESP_OK) err = es8311_microphone_config(gCodec, false);
  if (err == ESP_OK) err = es8311_microphone_gain_set(gCodec, ES8311_MIC_GAIN_18DB);
  if (err == ESP_OK) err = es8311_voice_mute(gCodec, false);
  if (err != ESP_OK) {
    es8311_delete(gCodec);
    gCodec = nullptr;
    audioI2s.end();
    return false;
  }

  return true;
}

void stopAudio()
{
  audioI2s.end();
  if (gCodec) {
    es8311_delete(gCodec);
    gCodec = nullptr;
  }
  gAudioReady = false;
}

// ─── FFT + bar update ────────────────────────────────────────────────────────
void processSamples()
{
  // Read mono 16-bit samples
  static int16_t buf[kFftSamples];
  size_t bytesRead = audioI2s.readBytes(reinterpret_cast<char *>(buf), sizeof(buf));
  if (bytesRead < sizeof(buf)) return;

  // Load real, apply Hann window
  for (size_t i = 0; i < kFftSamples; ++i) {
    double sample = static_cast<double>(buf[i]) / 32768.0;
    double window = 0.5 * (1.0 - cos(2.0 * M_PI * i / (kFftSamples - 1)));
    vReal[i] = sample * window;
    vImag[i] = 0.0;
  }

  fft.compute(FFTDirection::Forward);
  fft.complexToMagnitude();

  // Map FFT bins to bars (logarithmic spacing)
  size_t binCount = kFftSamples / 2;
  float logMin = log2f(1.0f);
  float logMax = log2f(static_cast<float>(binCount));

  for (size_t bar = 0; bar < kBarCount; ++bar) {
    float logLo = logMin + (logMax - logMin) * bar / kBarCount;
    float logHi = logMin + (logMax - logMin) * (bar + 1) / kBarCount;
    size_t binLo = static_cast<size_t>(powf(2.0f, logLo));
    size_t binHi = static_cast<size_t>(powf(2.0f, logHi));
    if (binLo < 1) binLo = 1;
    if (binHi >= binCount) binHi = binCount - 1;
    if (binHi < binLo) binHi = binLo;

    float peak = 0.0f;
    for (size_t bin = binLo; bin <= binHi; ++bin) {
      float mag = static_cast<float>(vReal[bin]);
      if (mag > peak) peak = mag;
    }

    // Convert to 0..1 via log scale
    float norm = log10f(1.0f + peak * kGain * 9.0f) / log10f(10.0f);
    if (norm > 1.0f) norm = 1.0f;

    // Smooth decay
    if (norm > gBarHeights[bar]) {
      gBarHeights[bar] = norm;
    } else {
      gBarHeights[bar] *= kBarDecayRate;
    }

    if (gBarHeights[bar] > gPeakHeights[bar]) {
      gPeakHeights[bar] = gBarHeights[bar];
    } else {
      gPeakHeights[bar] *= kPeakDecayRate;
    }
  }
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
  if (gUi.statusLabel) {
    lv_label_set_text(gUi.statusLabel, "Starting mic...");
  }
  gAudioStartPending = true;
}

void waveformLeaveSpectrumScreen()
{
  stopAudio();
}

void waveformTickSpectrumScreen(uint32_t nowMs)
{
  static uint32_t lastTickMs = 0;
  if (!gBuilt) return;
  if (gAudioStartPending) {
    gAudioStartPending = false;
    gAudioReady = initAudio();
    if (gUi.statusLabel) {
      lv_label_set_text(gUi.statusLabel, gAudioReady ? "Listening" : "Mic error");
    }
  }
  if (!gAudioReady) return;
  if (nowMs - lastTickMs < 40) return;
  lastTickMs = nowMs;
  processSamples();
  updateBars();
}

void waveformDestroySpectrumScreen()
{
  stopAudio();
  if (gUi.screen) {
    lv_obj_delete(gUi.screen);
  }
  screenRoots[static_cast<size_t>(ScreenId::Spectrum)] = nullptr;
  gUi = SpectrumUi{};
  gBuilt = false;
}

#endif // SCREEN_SPECTRUM
