#include "spectrum_core.h"

#include "pin_config.h"
#include "drivers/es8311.h"

#include <Arduino.h>
#include <Wire.h>
#include <arduinoFFT.h>
#include <driver/i2s.h>
#include <math.h>

namespace spectrum_core
{
namespace
{

constexpr uint32_t kI2cFreq = 400000;
constexpr uint32_t kSampleRate = 16000;
constexpr uint16_t kSampleCount = 128;
constexpr float kMinHz = 300.0f;
constexpr float kNoiseGate = 0.003f;
constexpr float kCompressionExponent = 1.35f;
constexpr float kTrebleBoost = 1.4f;

es8311_handle_t gCodec = nullptr;
bool gI2sInstalled = false;
bool gStarted = false;

int16_t gI2sBuffer[kSampleCount];
float gReal[kSampleCount];
float gImag[kSampleCount];
float gSmooth[kBandCount] = {};
uint8_t gBands[kBandCount] = {};

ArduinoFFT<float> gFft(gReal, gImag, kSampleCount, kSampleRate);

void i2sInitRx()
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

  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) == ESP_OK) {
    gI2sInstalled = true;
    if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
      i2s_driver_uninstall(I2S_NUM_0);
      gI2sInstalled = false;
      return;
    }
    if (i2s_set_clk(I2S_NUM_0, kSampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO) != ESP_OK) {
      i2s_driver_uninstall(I2S_NUM_0);
      gI2sInstalled = false;
      return;
    }
    i2s_zero_dma_buffer(I2S_NUM_0);
  }
}

bool codecInitForMic()
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
  es8311_microphone_gain_set(gCodec, ES8311_MIC_GAIN_MAX);
  return true;
}

void fftToBands24(const float *mag, uint8_t *out)
{
  float maxHz = kSampleRate / 2.0f;
  float binHz = static_cast<float>(kSampleRate) / static_cast<float>(kSampleCount);

  for (size_t band = 0; band < kBandCount; ++band) {
    float t0 = static_cast<float>(band) / static_cast<float>(kBandCount);
    float t1 = static_cast<float>(band + 1) / static_cast<float>(kBandCount);

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

    float value = magnitude / 900.0f;
    value = constrain(value, 0.0f, 1.0f);
    value = powf(value, 0.60f);

    float px = value * 255.0f;
    if (px > gSmooth[band]) {
      gSmooth[band] = gSmooth[band] * 0.15f + px * 0.85f;
    } else {
      gSmooth[band] = gSmooth[band] * 0.88f + px * 0.12f;
    }

    out[band] = constrain(static_cast<int>(gSmooth[band]), 0, 255);
  }
}

void processBands24(uint8_t *vals)
{
  for (size_t i = 0; i < kBandCount; ++i) {
    float value = vals[i] / 255.0f;
    if (value < kNoiseGate) {
      value = 0.0f;
    }
    value = powf(value, kCompressionExponent);
    value *= 1.0f + static_cast<float>(i) / static_cast<float>(kBandCount) * kTrebleBoost;
    if (value > 1.0f) {
      value = 1.0f;
    }
    vals[i] = static_cast<uint8_t>(value * 255.0f);
  }
}

} // namespace

bool begin()
{
  end();
  memset(gSmooth, 0, sizeof(gSmooth));
  memset(gBands, 0, sizeof(gBands));

  if (!codecInitForMic()) {
    end();
    return false;
  }

  i2sInitRx();
  if (!gI2sInstalled) {
    end();
    return false;
  }

  gStarted = true;
  return true;
}

void end()
{
  if (gI2sInstalled) {
    i2s_driver_uninstall(I2S_NUM_0);
    gI2sInstalled = false;
  }
  if (gCodec) {
    es8311_delete(gCodec);
    gCodec = nullptr;
  }
  gStarted = false;
}

bool poll(float *bandsOut, size_t bandCount, float *rmsOut)
{
  if (!gStarted || !bandsOut || bandCount < kBandCount) {
    return false;
  }

  size_t bytesRead = 0;
  if (i2s_read(I2S_NUM_0, gI2sBuffer, sizeof(gI2sBuffer), &bytesRead, portMAX_DELAY) != ESP_OK) {
    return false;
  }
  if (bytesRead < sizeof(gI2sBuffer)) {
    return false;
  }

  float sum = 0.0f;
  float sumSq = 0.0f;
  for (size_t i = 0; i < kSampleCount; ++i) {
    sum += gI2sBuffer[i];
    float sample = static_cast<float>(gI2sBuffer[i]);
    sumSq += sample * sample;
  }
  float mean = sum / static_cast<float>(kSampleCount);
  if (rmsOut) {
    *rmsOut = sqrtf(sumSq / static_cast<float>(kSampleCount));
  }

  for (size_t i = 0; i < kSampleCount; ++i) {
    gReal[i] = static_cast<float>(gI2sBuffer[i]) - mean;
    gImag[i] = 0.0f;
  }

  gFft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  gFft.compute(FFTDirection::Forward);
  gFft.complexToMagnitude();

  fftToBands24(gReal, gBands);
  processBands24(gBands);

  for (size_t i = 0; i < kBandCount; ++i) {
    bandsOut[i] = static_cast<float>(gBands[i]) / 255.0f;
  }
  return true;
}

} // namespace spectrum_core
