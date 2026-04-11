#include <Adafruit_XCA9554.h>
#include <Arduino.h>
#include <Arduino_DriveBus_Library.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <arduinoFFT.h>
#include <i2s.h>
#include <lvgl.h>

#include "pin_config.h"

extern "C" {
#include "es8311.h"
}

namespace
{
constexpr uint32_t kI2cFreq = 400000;
constexpr uint32_t kSampleRate = 16000;
constexpr uint16_t kSampleCount = 128;
constexpr int kBands = 24;
constexpr int kVisualBars = 32;
constexpr float kMinHz = 300.0f;
constexpr float kNoiseGate = 0.02f;
constexpr float kCompressionExponent = 1.35f;
constexpr float kTrebleBoost = 1.4f;
constexpr uint32_t kUiRefreshMs = 40;

constexpr lv_coord_t kAreaHeight = 314;
constexpr lv_coord_t kAreaBottom = LCD_HEIGHT;
constexpr lv_coord_t kAreaTop = kAreaBottom - kAreaHeight;
constexpr lv_coord_t kBarWidth = ((LCD_WIDTH - 24) / kVisualBars) - 1;
constexpr lv_coord_t kBarGap = 1;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_SH8601 *gfx = new Arduino_SH8601(bus, GFX_NOT_DEFINED, 0, LCD_WIDTH, LCD_HEIGHT);
Adafruit_XCA9554 expander;

lv_display_t *display = nullptr;
lv_indev_t *touchInput = nullptr;
uint8_t *lvBuffer = nullptr;

es8311_handle_t gCodec = nullptr;
int16_t gI2sBuffer[kSampleCount];
float gReal[kSampleCount];
float gImag[kSampleCount];
uint8_t gBandsOut[kBands];
float gBarHeights[kVisualBars] = {};
float gPeakHeights[kVisualBars] = {};
float gVuRms = 0.0f;

ArduinoFFT<float> gFft(gReal, gImag, kSampleCount, kSampleRate);

struct UiState
{
  lv_obj_t *screen = nullptr;
  lv_obj_t *status = nullptr;
  lv_obj_t *bars[kVisualBars] = {};
  lv_obj_t *peaks[kVisualBars] = {};
} ui;

void flushDisplay(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap)
{
  LV_UNUSED(disp);
  uint16_t width = static_cast<uint16_t>(area->x2 - area->x1 + 1);
  uint16_t height = static_cast<uint16_t>(area->y2 - area->y1 + 1);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(pxMap), width, height);
  lv_display_flush_ready(display);
}

void readTouch(lv_indev_t *indev, lv_indev_data_t *data)
{
  LV_UNUSED(indev);
  data->state = LV_INDEV_STATE_RELEASED;
  data->point.x = LCD_WIDTH / 2;
  data->point.y = LCD_HEIGHT / 2;
}

lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b)
{
  return lv_color_make(r, g, b);
}

lv_color_t barColor(int index)
{
  float t = static_cast<float>(index) / static_cast<float>(kVisualBars - 1);
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  if (t < 0.25f) {
    float u = t / 0.25f;
    g = static_cast<uint8_t>(u * 200.0f);
    b = 255;
  } else if (t < 0.5f) {
    float u = (t - 0.25f) / 0.25f;
    g = static_cast<uint8_t>(200.0f + u * 55.0f);
    b = static_cast<uint8_t>(255.0f * (1.0f - u));
  } else if (t < 0.75f) {
    float u = (t - 0.5f) / 0.25f;
    r = static_cast<uint8_t>(u * 255.0f);
    g = 255;
  } else {
    float u = (t - 0.75f) / 0.25f;
    r = 255;
    g = static_cast<uint8_t>(255.0f * (1.0f - u));
  }

  return lvColor(r, g, b);
}

float visualBandValue(int index)
{
  float position = static_cast<float>(index) * static_cast<float>(kBands - 1) / static_cast<float>(kVisualBars - 1);
  int left = static_cast<int>(position);
  int right = min(left + 1, kBands - 1);
  float mix = position - static_cast<float>(left);
  float leftValue = static_cast<float>(gBandsOut[left]) / 255.0f;
  float rightValue = static_cast<float>(gBandsOut[right]) / 255.0f;
  return leftValue * (1.0f - mix) + rightValue * mix;
}

void initExpander()
{
  bool ready = false;
  for (int attempt = 0; attempt < 3 && !ready; ++attempt) {
    if (attempt > 0) {
      Wire.end();
      delay(10);
      Wire.begin(IIC_SDA, IIC_SCL, kI2cFreq);
      delay(10);
    }
    ready = expander.begin(0x20);
  }

  if (!ready) {
    Serial.println("I2C expander not found");
    return;
  }

  expander.pinMode(PMU_IRQ_PIN, INPUT);
  expander.pinMode(TOP_BUTTON_PIN, INPUT);
  expander.pinMode(0, OUTPUT);
  expander.pinMode(1, OUTPUT);
  expander.pinMode(2, OUTPUT);
  expander.pinMode(6, OUTPUT);
  expander.pinMode(7, OUTPUT);
  expander.digitalWrite(0, LOW);
  expander.digitalWrite(1, LOW);
  expander.digitalWrite(2, LOW);
  expander.digitalWrite(6, LOW);
  delay(20);
  expander.digitalWrite(0, HIGH);
  expander.digitalWrite(1, HIGH);
  expander.digitalWrite(2, HIGH);
  expander.digitalWrite(6, HIGH);
  expander.digitalWrite(7, HIGH);
  delay(20);
}

bool initCodecForMic()
{
  Wire.begin(IIC_SDA, IIC_SCL, kI2cFreq);
  gCodec = es8311_create(0, ES8311_ADDRESS_0);
  if (!gCodec) {
    return false;
  }

  es8311_clock_config_t clk = {};
  clk.mclk_from_mclk_pin = true;
  clk.mclk_frequency = 12288000;
  clk.sample_frequency = kSampleRate;

  if (es8311_init(gCodec, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) {
    return false;
  }
  if (es8311_microphone_config(gCodec, false) != ESP_OK) {
    return false;
  }
  es8311_microphone_gain_set(gCodec, ES8311_MIC_GAIN_18DB);
  return true;
}

void initI2sRx()
{
  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = kSampleRate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = 128;
  cfg.use_apll = true;
  cfg.fixed_mclk = 12288000;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = AUDIO_I2S_BCLK;
  pins.ws_io_num = AUDIO_I2S_WS;
  pins.data_in_num = AUDIO_I2S_DIN;
  pins.data_out_num = AUDIO_I2S_DOUT;
  pins.mck_io_num = AUDIO_I2S_MCLK;

  i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_set_clk(I2S_NUM_0, kSampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void fftToBands(const float *mag, uint8_t *out)
{
  float maxHz = kSampleRate / 2.0f;
  float binHz = static_cast<float>(kSampleRate) / static_cast<float>(kSampleCount);
  static float smooth[kBands] = {};

  for (int band = 0; band < kBands; ++band) {
    float t0 = static_cast<float>(band) / static_cast<float>(kBands);
    float t1 = static_cast<float>(band + 1) / static_cast<float>(kBands);

    float f0 = powf(10.0f, log10f(kMinHz) + t0 * (log10f(maxHz) - log10f(kMinHz)));
    float f1 = powf(10.0f, log10f(kMinHz) + t1 * (log10f(maxHz) - log10f(kMinHz)));

    int k0 = max(2, static_cast<int>(f0 / binHz));
    int k1 = min(static_cast<int>((kSampleCount / 2) - 1), static_cast<int>(f1 / binHz));
    if (k1 <= k0) {
      k1 = k0 + 1;
    }

    float magnitude = 0.0f;
    for (int bin = k0; bin <= k1; ++bin) {
      magnitude += mag[bin];
    }
    magnitude /= static_cast<float>(k1 - k0 + 1);

    float value = magnitude / 7000.0f;
    value = constrain(value, 0.0f, 1.0f);
    value = powf(value, 0.60f);

    float px = value * 255.0f;
    if (px > smooth[band]) {
      smooth[band] = smooth[band] * 0.15f + px * 0.85f;
    } else {
      smooth[band] = smooth[band] * 0.88f + px * 0.12f;
    }

    out[band] = constrain(static_cast<int>(smooth[band]), 0, 255);
  }
}

void processBands(uint8_t *vals)
{
  for (int i = 0; i < kBands; ++i) {
    float value = vals[i] / 255.0f;
    if (value < kNoiseGate) {
      value = 0.0f;
    }
    value = powf(value, kCompressionExponent);
    value *= (1.0f + static_cast<float>(i) / static_cast<float>(kBands) * kTrebleBoost);
    if (value > 1.0f) {
      value = 1.0f;
    }
    vals[i] = static_cast<uint8_t>(value * 255.0f);
  }
}

bool pollAudio()
{
  size_t bytesRead = 0;
  if (i2s_read(I2S_NUM_0, gI2sBuffer, sizeof(gI2sBuffer), &bytesRead, portMAX_DELAY) != ESP_OK) {
    return false;
  }
  if (bytesRead < kSampleCount * sizeof(int16_t)) {
    return false;
  }

  float sum = 0.0f;
  for (int i = 0; i < kSampleCount; ++i) {
    sum += gI2sBuffer[i];
  }
  float mean = sum / static_cast<float>(kSampleCount);

  float sumSq = 0.0f;
  for (int i = 0; i < kSampleCount; ++i) {
    gReal[i] = static_cast<float>(gI2sBuffer[i]) - mean;
    gImag[i] = 0.0f;
    sumSq += gReal[i] * gReal[i];
  }
  gVuRms = sqrtf(sumSq / static_cast<float>(kSampleCount));

  gFft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  gFft.compute(FFTDirection::Forward);
  gFft.complexToMagnitude();

  fftToBands(gReal, gBandsOut);
  processBands(gBandsOut);
  return true;
}

void setupLvgl()
{
  lv_init();
  lv_tick_set_cb(millis);

  display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
  size_t bufferPixelCount = LCD_WIDTH * 40;
  lvBuffer = static_cast<uint8_t *>(malloc(bufferPixelCount * sizeof(lv_color16_t)));
  if (!lvBuffer) {
    while (true) {
      delay(1000);
    }
  }

  lv_display_set_buffers(display, lvBuffer, nullptr, bufferPixelCount * sizeof(lv_color16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flushDisplay);

  touchInput = lv_indev_create();
  lv_indev_set_type(touchInput, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(touchInput, readTouch);
}

void buildUi()
{
  ui.screen = lv_obj_create(nullptr);
  lv_obj_set_size(ui.screen, LCD_WIDTH - 2, LCD_HEIGHT - 2);
  lv_obj_set_style_bg_color(ui.screen, lvColor(4, 6, 12), 0);
  lv_obj_set_style_bg_opa(ui.screen, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ui.screen, 1, 0);
  lv_obj_set_style_border_color(ui.screen, lvColor(36, 44, 60), 0);
  lv_obj_set_style_border_opa(ui.screen, LV_OPA_70, 0);
  lv_obj_set_style_pad_all(ui.screen, 0, 0);
  lv_obj_clear_flag(ui.screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(ui.screen, 1, 1);

  lv_obj_t *eyebrow = lv_label_create(ui.screen);
  lv_obj_set_style_text_font(eyebrow, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(eyebrow, lvColor(80, 60, 100), 0);
  lv_label_set_text(eyebrow, "WAVEFORM");
  lv_obj_align(eyebrow, LV_ALIGN_TOP_LEFT, 20, 18);

  lv_obj_t *headline = lv_label_create(ui.screen);
  lv_obj_set_style_text_font(headline, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(headline, lvColor(244, 244, 252), 0);
  lv_label_set_text(headline, "Spectrum");
  lv_obj_align(headline, LV_ALIGN_TOP_LEFT, 20, 40);

  ui.status = lv_label_create(ui.screen);
  lv_obj_set_style_text_font(ui.status, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ui.status, lvColor(120, 80, 140), 0);
  lv_label_set_text(ui.status, "Listening");
  lv_obj_align(ui.status, LV_ALIGN_TOP_LEFT, 20, 76);

  for (int i = 0; i < kVisualBars; ++i) {
    lv_coord_t bx = 12 + static_cast<lv_coord_t>(i) * (kBarWidth + kBarGap);

    lv_obj_t *bar = lv_obj_create(ui.screen);
    lv_obj_set_style_bg_color(bar, barColor(i), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_radius(bar, 2, 0);
    lv_obj_set_size(bar, kBarWidth, 2);
    lv_obj_set_pos(bar, bx, kAreaBottom - 2);
    ui.bars[i] = bar;

    lv_obj_t *peak = lv_obj_create(ui.screen);
    lv_obj_set_style_bg_color(peak, lvColor(255, 255, 255), 0);
    lv_obj_set_style_bg_opa(peak, LV_OPA_80, 0);
    lv_obj_set_style_border_width(peak, 0, 0);
    lv_obj_set_style_pad_all(peak, 0, 0);
    lv_obj_set_style_radius(peak, 1, 0);
    lv_obj_set_size(peak, kBarWidth, 2);
    lv_obj_set_pos(peak, bx, kAreaBottom - 2);
    ui.peaks[i] = peak;
  }

  lv_screen_load(ui.screen);
  lv_refr_now(display);
}

void updateUi()
{
  char status[32];
  snprintf(status, sizeof(status), "Listening  rms %.0f", gVuRms);
  lv_label_set_text(ui.status, status);

  for (int i = 0; i < kVisualBars; ++i) {
    float value = visualBandValue(i);
    gBarHeights[i] = value;
    if (value > gPeakHeights[i]) {
      gPeakHeights[i] = value;
    } else {
      gPeakHeights[i] *= 0.92f;
    }

    lv_coord_t barH = static_cast<lv_coord_t>(gBarHeights[i] * kAreaHeight);
    if (barH < 2) {
      barH = 2;
    }

    lv_coord_t bx = 12 + static_cast<lv_coord_t>(i) * (kBarWidth + kBarGap);
    lv_obj_set_size(ui.bars[i], kBarWidth, barH);
    lv_obj_set_pos(ui.bars[i], bx, kAreaBottom - barH);

    lv_coord_t peakY = kAreaBottom - static_cast<lv_coord_t>(gPeakHeights[i] * kAreaHeight) - 2;
    lv_obj_set_pos(ui.peaks[i], bx, peakY);
  }
}

} // namespace

void setup()
{
  Serial.begin(115200);
  delay(250);

  Wire.begin(IIC_SDA, IIC_SCL, kI2cFreq);
  initExpander();

  if (!gfx->begin()) {
    while (true) {
      delay(1000);
    }
  }

  gfx->setBrightness(255);
  gfx->fillScreen(0x0000);
  gfx->displayOn();

  setupLvgl();
  buildUi();

  bool ok = initCodecForMic();
  initI2sRx();
  i2s_zero_dma_buffer(I2S_NUM_0);

  if (!ok) {
    lv_label_set_text(ui.status, "Mic init failed");
    lv_refr_now(display);
  }
}

void loop()
{
  static uint32_t lastRefreshMs = 0;

  if (millis() - lastRefreshMs >= kUiRefreshMs) {
    if (pollAudio()) {
      updateUi();
    } else {
      lv_label_set_text(ui.status, "Mic read error");
    }
    lastRefreshMs = millis();
  }

  lv_timer_handler();
  delay(5);
}
