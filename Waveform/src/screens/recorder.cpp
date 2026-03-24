#include "config/pin_config.h"
#include "config/screen_constants.h"
#include "core/screen_manager.h"
#include "drivers/es8311.h"
#include <ESP_I2S.h>
#include <SD_MMC.h>

#include "modules/storage.h"
#include "screens/screen_callbacks.h"

constexpr es8311_mic_gain_t kRecorderMicGain = ES8311_MIC_GAIN_18DB;

extern I2SClass audioI2s;
void refreshRecorderScreen();
extern bool sdMounted;
extern lv_obj_t *screenRoots[];
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
void noteActivity();

struct RecorderUi
{
  lv_obj_t *screen = nullptr;
  lv_obj_t *statusLabel = nullptr;
  lv_obj_t *detailLabel = nullptr;
  lv_obj_t *button = nullptr;
  lv_obj_t *buttonLabel = nullptr;
  lv_obj_t *spinnerSegments[kRecorderSpinnerSegmentCount] = {};
  lv_obj_t *waveformPanel = nullptr;
  lv_obj_t *waveformBars[kRecorderWaveformBarCount] = {};
};

RecorderUi recorderUi;
bool recorderBuilt = false;
String recorderStatusText = "Insert SD card to record";
String recorderDetailText = "The last take is overwritten every time.";
bool audioHardwareReady = false;
bool audioClipAvailable = false;
bool audioRecordingActive = false;
bool audioPlaybackActive = false;
uint8_t sdCardType = CARD_NONE;
uint64_t sdCardSizeMb = 0;
uint32_t audioClipBytes = 0;
uint32_t audioRecordedBytes = 0;
uint32_t audioPlaybackBytes = 0;
es8311_handle_t audioCodec = nullptr;
File audioRecordingFile;
File audioPlaybackFile;
uint8_t audioBuffer[kRecorderAudioChunkBytes] = {};
uint8_t recorderWaveformLevels[kRecorderWaveformBarCount] = {};
float recorderLiveLevel = 0.0f;
float recorderDisplayLevel = 0.0f;

namespace
{
const ScreenModule kModule = {
    ScreenId::Recorder,
    "Recorder",
    waveformBuildRecorderScreen,
    waveformRefreshRecorderScreen,
    waveformEnterRecorderScreen,
    waveformLeaveRecorderScreen,
    waveformTickRecorderScreen,
    waveformRecorderScreenRoot,
};
} // namespace

const ScreenModule &recorderScreenModule()
{
  return kModule;
}


String recorderClipFullPath()
{
  return String(kSdMountPath) + kRecorderClipPath;
}

String formatRecorderDuration(uint32_t bytes)
{
  uint32_t totalSeconds = bytes / kRecorderBytesPerSecond;
  char text[12];
  snprintf(text, sizeof(text), "%02lu:%02lu",
           static_cast<unsigned long>(totalSeconds / 60U),
           static_cast<unsigned long>(totalSeconds % 60U));
  return String(text);
}

float recorderSampleLevel(const uint8_t *buffer, size_t bytes)
{
  if (!buffer || bytes < sizeof(int16_t)) {
    return 0.0f;
  }

  const int16_t *samples = reinterpret_cast<const int16_t *>(buffer);
  size_t sampleCount = bytes / sizeof(int16_t);
  int32_t peak = 0;
  for (size_t i = 0; i < sampleCount; i += 2) {
    int32_t value = samples[i];
    if (value < 0) {
      value = -value;
    }
    if (value > peak) {
      peak = value;
    }
  }

  return fminf(static_cast<float>(peak) / 32767.0f, 1.0f);
}

void pushRecorderWaveformLevel(float normalizedLevel)
{
  normalizedLevel = fminf(fmaxf(normalizedLevel, 0.0f), 1.0f);
  memmove(recorderWaveformLevels,
          recorderWaveformLevels + 1,
          sizeof(recorderWaveformLevels) - sizeof(recorderWaveformLevels[0]));
  recorderWaveformLevels[kRecorderWaveformBarCount - 1] =
      static_cast<uint8_t>(roundf(normalizedLevel * 255.0f));
}

void updateRecorderLevelFromSamples(const uint8_t *buffer, size_t bytes)
{
  float level = recorderSampleLevel(buffer, bytes);
  recorderLiveLevel = level;
  recorderDisplayLevel = fmaxf(level, recorderDisplayLevel * 0.72f);
  pushRecorderWaveformLevel(level);
}

void clearRecorderVisualState()
{
  memset(recorderWaveformLevels, 0, sizeof(recorderWaveformLevels));
  recorderLiveLevel = 0.0f;
  recorderDisplayLevel = 0.0f;
}

void refreshRecorderClipState()
{
  audioClipAvailable = false;
  audioClipBytes = 0;
  if (!sdMounted) {
    return;
  }

  File clip = SD_MMC.open(recorderClipFullPath().c_str(), FILE_READ);
  if (!clip) {
    return;
  }

  audioClipBytes = static_cast<uint32_t>(clip.size());
  audioClipAvailable = audioClipBytes > 0;
  clip.close();
}

void closeRecorderFiles()
{
  if (audioRecordingFile) {
    audioRecordingFile.close();
  }
  if (audioPlaybackFile) {
    audioPlaybackFile.close();
  }
}


bool ensureAudioHardwareReady()
{
  if (audioHardwareReady) {
    return true;
  }

  recorderStatusText = "Starting audio...";
  recorderDetailText = "Bringing up microphone and speaker.";

  audioI2s.end();
  audioI2s.setPins(AUDIO_I2S_BCLK, AUDIO_I2S_WS, AUDIO_I2S_DOUT, AUDIO_I2S_DIN, AUDIO_I2S_MCLK);
  audioI2s.setTimeout(kRecorderAudioTimeoutMs);
  if (!audioI2s.begin(I2S_MODE_STD,
                      kRecorderSampleRate,
                      I2S_DATA_BIT_WIDTH_16BIT,
                      I2S_SLOT_MODE_STEREO,
                      I2S_STD_SLOT_BOTH)) {
    recorderStatusText = "Audio bus failed";
    recorderDetailText = "I2S did not start.";
    return false;
  }

  if (!audioCodec) {
    audioCodec = es8311_create(0, ES8311_ADDRRES_0);
  }
  if (!audioCodec) {
    recorderStatusText = "Codec missing";
    recorderDetailText = "ES8311 was not detected.";
    audioI2s.end();
    return false;
  }

  const es8311_clock_config_t clockConfig = {
      .mclk_inverted = false,
      .sclk_inverted = false,
      .mclk_from_mclk_pin = true,
      .mclk_frequency = static_cast<int>(kRecorderSampleRate * 256U),
      .sample_frequency = static_cast<int>(kRecorderSampleRate),
  };

  esp_err_t result = es8311_init(audioCodec, &clockConfig, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
  if (result == ESP_OK) {
    result = es8311_sample_frequency_config(audioCodec, clockConfig.mclk_frequency, clockConfig.sample_frequency);
  }
  if (result == ESP_OK) {
    result = es8311_microphone_config(audioCodec, false);
  }
  if (result == ESP_OK) {
    result = es8311_voice_volume_set(audioCodec, kRecorderCodecVolume, nullptr);
  }
  if (result == ESP_OK) {
    result = es8311_microphone_gain_set(audioCodec, kRecorderMicGain);
  }
  if (result == ESP_OK) {
    result = es8311_voice_mute(audioCodec, false);
  }

  if (result != ESP_OK) {
    recorderStatusText = "Codec init failed";
    recorderDetailText = esp_err_to_name(result);
    audioI2s.end();
    return false;
  }

  audioHardwareReady = true;
  recorderStatusText = "Ready to record";
  recorderDetailText = "The last take is overwritten every time.";
  return true;
}

void stopPlayback()
{
  if (!audioPlaybackActive) {
    return;
  }

  audioPlaybackActive = false;
  audioPlaybackBytes = 0;
  if (audioPlaybackFile) {
    audioPlaybackFile.close();
  }
  clearRecorderVisualState();
  refreshRecorderClipState();
  recorderStatusText = audioClipAvailable ? "Ready to record" : "No clip saved";
  recorderDetailText = audioClipAvailable ? "Hold to replace the last take." : "Hold to record a clip.";
}

bool startPlayback()
{
  if (!sdMounted) {
    recorderStatusText = "Insert SD card";
    recorderDetailText = "Recording and playback use the SD card.";
    return false;
  }

  if (!audioClipAvailable) {
    refreshRecorderClipState();
  }
  if (!audioClipAvailable) {
    recorderStatusText = "No clip saved";
    recorderDetailText = "Record something first.";
    return false;
  }

  if (!ensureAudioHardwareReady()) {
    return false;
  }

  if (audioRecordingActive) {
    return false;
  }

  if (audioPlaybackActive) {
    stopPlayback();
  }

  audioPlaybackFile = SD_MMC.open(recorderClipFullPath().c_str(), FILE_READ);
  if (!audioPlaybackFile) {
    recorderStatusText = "Playback open failed";
    recorderDetailText = "The saved clip could not be read.";
    return false;
  }

  audioPlaybackBytes = 0;
  audioPlaybackActive = true;
  clearRecorderVisualState();
  recorderStatusText = "Playing back";
  recorderDetailText = "Listening to the last take.";
  return true;
}

void stopRecording(bool playAfterStop)
{
  if (!audioRecordingActive) {
    return;
  }

  audioRecordingActive = false;
  if (audioRecordingFile) {
    audioRecordingFile.close();
  }
  refreshRecorderClipState();

  if (audioRecordedBytes == 0) {
    recorderStatusText = "No audio captured";
    recorderDetailText = "Hold again to retry.";
    return;
  }

  audioClipBytes = audioRecordedBytes;
  recorderStatusText = "Saved " + formatRecorderDuration(audioRecordedBytes);
  recorderDetailText = "Playing the latest take.";
  if (playAfterStop) {
    if (!startPlayback()) {
      recorderDetailText = "Hold to replace the last take.";
    }
  } else {
    recorderDetailText = "Hold to replace the last take.";
  }
}

bool startRecording()
{
  if (!sdMounted) {
    initSdCard();
  }
  if (!sdMounted) {
    recorderStatusText = "Insert SD card";
    recorderDetailText = "Recording and playback use the SD card.";
    return false;
  }

  if (!ensureAudioHardwareReady()) {
    return false;
  }

  if (audioPlaybackActive) {
    stopPlayback();
  }

  if (SD_MMC.exists(recorderClipFullPath().c_str())) {
    SD_MMC.remove(recorderClipFullPath().c_str());
  }

  audioRecordingFile = SD_MMC.open(recorderClipFullPath().c_str(), FILE_WRITE);
  if (!audioRecordingFile) {
    recorderStatusText = "Record open failed";
    recorderDetailText = "Could not create the clip on SD.";
    return false;
  }

  audioRecordedBytes = 0;
  audioRecordingActive = true;
  audioClipAvailable = false;
  clearRecorderVisualState();
  recorderStatusText = "Recording";
  recorderDetailText = "Release to stop and play it back.";
  return true;
}

void updateRecorderAudio()
{
  if (audioRecordingActive) {
    noteActivity();
    size_t bytesRead = audioI2s.readBytes(reinterpret_cast<char *>(audioBuffer), sizeof(audioBuffer));
    if (bytesRead == 0) {
      recorderStatusText = "Mic read failed";
      recorderDetailText = "Recording stopped before playback.";
      stopRecording(false);
      return;
    }

    size_t bytesWritten = audioRecordingFile.write(audioBuffer, bytesRead);
    if (bytesWritten != bytesRead) {
      recorderStatusText = "SD write failed";
      recorderDetailText = "Recording stopped before playback.";
      stopRecording(false);
      return;
    }

    updateRecorderLevelFromSamples(audioBuffer, bytesRead);
    audioRecordedBytes += static_cast<uint32_t>(bytesWritten);
    return;
  }

  if (audioPlaybackActive) {
    noteActivity();
    int bytesRead = audioPlaybackFile.read(audioBuffer, sizeof(audioBuffer));
    if (bytesRead <= 0) {
      stopPlayback();
      return;
    }

    size_t bytesWritten = audioI2s.write(audioBuffer, static_cast<size_t>(bytesRead));
    if (bytesWritten != static_cast<size_t>(bytesRead)) {
      recorderStatusText = "Speaker write failed";
      recorderDetailText = "Playback stopped early.";
      stopPlayback();
      return;
    }

    updateRecorderLevelFromSamples(audioBuffer, static_cast<size_t>(bytesRead));
    audioPlaybackBytes += static_cast<uint32_t>(bytesRead);
  }
}

void applyRecorderButtonPulse(bool active)
{
  if (!recorderUi.button) {
    return;
  }

  if (active) {
    recorderDisplayLevel = fmaxf(recorderLiveLevel, recorderDisplayLevel * 0.82f);
  } else {
    recorderDisplayLevel *= 0.76f;
    if (recorderDisplayLevel < 0.01f) {
      recorderDisplayLevel = 0.0f;
    }
  }

  int pulseOffset = static_cast<int>(roundf(recorderDisplayLevel * 28.0f));
  int diameter = static_cast<int>(kRecorderButtonDiameter) + pulseOffset;
  lv_obj_set_size(recorderUi.button, diameter, diameter);
  lv_obj_align(recorderUi.button, LV_ALIGN_CENTER, 0, kRecorderButtonCenterYOffset);
}

void updateRecorderVisuals()
{
  if (!recorderBuilt || !recorderUi.button) {
    return;
  }

  bool active = audioRecordingActive || audioPlaybackActive;
  applyRecorderButtonPulse(active);

  lv_color_t activeColor = audioPlaybackActive ? lvColor(88, 164, 255) : lvColor(255, 96, 104);
  int centerX = LCD_WIDTH / 2;
  int centerY = (LCD_HEIGHT / 2) + kRecorderButtonCenterYOffset;
  float orbitRadius = (static_cast<float>(lv_obj_get_width(recorderUi.button)) * 0.5f) + 22.0f + (recorderDisplayLevel * 8.0f);
  uint32_t spinnerHead = (millis() / kRecorderPulseStepMs) % kRecorderSpinnerSegmentCount;

  for (size_t i = 0; i < kRecorderSpinnerSegmentCount; ++i) {
    lv_obj_t *segment = recorderUi.spinnerSegments[i];
    if (!segment) {
      continue;
    }

    float angle = ((360.0f / static_cast<float>(kRecorderSpinnerSegmentCount)) * static_cast<float>(i)) - 90.0f;
    float radians = angle * (PI / 180.0f);
    int x = centerX + static_cast<int>(roundf(cosf(radians) * orbitRadius)) - 5;
    int y = centerY + static_cast<int>(roundf(sinf(radians) * orbitRadius)) - 5;
    lv_obj_set_pos(segment, x, y);

    uint8_t opacity = 0;
    if (active) {
      size_t distance = (i + kRecorderSpinnerSegmentCount - spinnerHead) % kRecorderSpinnerSegmentCount;
      if (distance == 0) {
        opacity = 255;
      } else if (distance == 1) {
        opacity = 200;
      } else if (distance == 2) {
        opacity = 132;
      } else if (distance == 3) {
        opacity = 88;
      } else {
        opacity = 34;
      }
    }

    lv_obj_set_style_bg_color(segment, activeColor, 0);
    lv_obj_set_style_bg_opa(segment, opacity, 0);
  }

  if (!recorderUi.waveformPanel) {
    return;
  }

  lv_color_t waveformColor = audioPlaybackActive ? lvColor(90, 170, 255) : lvColor(244, 98, 106);
  int panelHeight = kRecorderWaveformHeight;
  int barWidth = 6;
  int gap = 3;
  int baseY = panelHeight / 2;
  for (size_t i = 0; i < kRecorderWaveformBarCount; ++i) {
    lv_obj_t *bar = recorderUi.waveformBars[i];
    if (!bar) {
      continue;
    }

    float normalized = static_cast<float>(recorderWaveformLevels[i]) / 255.0f;
    int barHeight = active ? max(6, static_cast<int>(roundf(8.0f + (normalized * 64.0f)))) : 6;
    int x = static_cast<int>(i) * (barWidth + gap);
    int y = baseY - (barHeight / 2);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, barWidth, barHeight);
    lv_obj_set_style_bg_color(bar, waveformColor, 0);
    lv_obj_set_style_bg_opa(bar, active ? static_cast<lv_opa_t>(90 + static_cast<int>(normalized * 165.0f)) : LV_OPA_20, 0);
  }
}

void handleRecorderButtonEvent(lv_event_t *event)
{
  noteActivity();
  lv_event_code_t code = lv_event_get_code(event);

  if (code == LV_EVENT_PRESSED) {
    if (!audioPlaybackActive && !audioRecordingActive) {
      startRecording();
      refreshRecorderScreen();
    }
    return;
  }

  if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    if (audioRecordingActive) {
      stopRecording(true);
      refreshRecorderScreen();
    }
  }
}

lv_obj_t *buildRecorderScreen()
{
  recorderUi.screen = lv_obj_create(nullptr);
  applyRootStyle(recorderUi.screen);

  recorderUi.button = lv_button_create(recorderUi.screen);
  lv_obj_set_size(recorderUi.button, kRecorderButtonDiameter, kRecorderButtonDiameter);
  lv_obj_align(recorderUi.button, LV_ALIGN_CENTER, 0, kRecorderButtonCenterYOffset);
  lv_obj_set_style_radius(recorderUi.button, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(recorderUi.button, lvColor(154, 24, 30), 0);
  lv_obj_set_style_bg_color(recorderUi.button, lvColor(184, 38, 44), LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(recorderUi.button, lvColor(34, 42, 54), LV_STATE_DISABLED);
  lv_obj_set_style_border_width(recorderUi.button, 2, 0);
  lv_obj_set_style_border_color(recorderUi.button, lvColor(214, 84, 90), 0);
  lv_obj_set_style_border_color(recorderUi.button, lvColor(56, 68, 82), LV_STATE_DISABLED);
  lv_obj_set_style_shadow_width(recorderUi.button, 28, 0);
  lv_obj_set_style_shadow_color(recorderUi.button, lvColor(154, 24, 30), 0);
  lv_obj_set_style_shadow_opa(recorderUi.button, LV_OPA_40, 0);
  lv_obj_add_event_cb(recorderUi.button, handleRecorderButtonEvent, LV_EVENT_ALL, nullptr);

  for (size_t i = 0; i < kRecorderSpinnerSegmentCount; ++i) {
    recorderUi.spinnerSegments[i] = lv_obj_create(recorderUi.screen);
    lv_obj_set_size(recorderUi.spinnerSegments[i], 10, 10);
    lv_obj_set_style_radius(recorderUi.spinnerSegments[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(recorderUi.spinnerSegments[i], 0, 0);
    lv_obj_set_style_pad_all(recorderUi.spinnerSegments[i], 0, 0);
    lv_obj_set_style_bg_opa(recorderUi.spinnerSegments[i], LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(recorderUi.spinnerSegments[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(recorderUi.spinnerSegments[i], LV_OBJ_FLAG_CLICKABLE);
  }

  recorderUi.waveformPanel = lv_obj_create(recorderUi.screen);
  lv_obj_set_size(recorderUi.waveformPanel, kRecorderWaveformWidth, kRecorderWaveformHeight);
  lv_obj_align(recorderUi.waveformPanel, LV_ALIGN_CENTER, 0, kRecorderWaveformYOffset);
  lv_obj_set_style_bg_color(recorderUi.waveformPanel, lvColor(8, 12, 18), 0);
  lv_obj_set_style_bg_opa(recorderUi.waveformPanel, LV_OPA_30, 0);
  lv_obj_set_style_radius(recorderUi.waveformPanel, 20, 0);
  lv_obj_set_style_border_width(recorderUi.waveformPanel, 1, 0);
  lv_obj_set_style_border_color(recorderUi.waveformPanel, lvColor(20, 26, 34), 0);
  lv_obj_set_style_pad_all(recorderUi.waveformPanel, 0, 0);
  lv_obj_clear_flag(recorderUi.waveformPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(recorderUi.waveformPanel, LV_OBJ_FLAG_CLICKABLE);

  for (size_t i = 0; i < kRecorderWaveformBarCount; ++i) {
    recorderUi.waveformBars[i] = lv_obj_create(recorderUi.waveformPanel);
    lv_obj_set_size(recorderUi.waveformBars[i], 6, 6);
    lv_obj_set_style_radius(recorderUi.waveformBars[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(recorderUi.waveformBars[i], 0, 0);
    lv_obj_set_style_pad_all(recorderUi.waveformBars[i], 0, 0);
    lv_obj_clear_flag(recorderUi.waveformBars[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(recorderUi.waveformBars[i], LV_OBJ_FLAG_CLICKABLE);
  }

  recorderUi.statusLabel = nullptr;
  recorderUi.detailLabel = nullptr;
  recorderUi.buttonLabel = nullptr;
  clearRecorderVisualState();
  applyRecorderButtonPulse(false);
  updateRecorderVisuals();
  recorderBuilt = true;
  return recorderUi.screen;
}

void buildRecorderScreenRoot()
{
  screenRoots[static_cast<size_t>(ScreenId::Recorder)] = buildRecorderScreen();
}

void refreshRecorderScreen()
{
  if (!recorderBuilt || !recorderUi.button) {
    return;
  }

  if (!sdMounted) {
    recorderStatusText = "Insert SD card";
    recorderDetailText = "Recording and playback use the SD card.";
  } else if (!audioRecordingActive && !audioPlaybackActive) {
    refreshRecorderClipState();
    if (audioClipAvailable && recorderStatusText == "Ready to record") {
      recorderDetailText = "Last take length " + formatRecorderDuration(audioClipBytes);
    }
  }

  bool disableButton = false;
  lv_color_t buttonColor = lvColor(154, 24, 30);
  lv_color_t borderColor = lvColor(214, 84, 90);
  bool shouldPulse = false;

  if (!sdMounted) {
    disableButton = true;
    buttonColor = lvColor(34, 42, 54);
    borderColor = lvColor(56, 68, 82);
  } else if (audioPlaybackActive) {
    buttonColor = lvColor(26, 82, 154);
    borderColor = lvColor(76, 138, 214);
    shouldPulse = true;
  } else if (audioRecordingActive) {
    buttonColor = lvColor(184, 38, 44);
    borderColor = lvColor(240, 98, 104);
    shouldPulse = true;
    recorderStatusText = "Recording " + formatRecorderDuration(audioRecordedBytes);
    recorderDetailText = "Release to stop and play it back.";
  }

  lv_obj_set_style_bg_color(recorderUi.button, buttonColor, 0);
  lv_obj_set_style_border_color(recorderUi.button, borderColor, 0);
  lv_obj_set_style_shadow_color(recorderUi.button, buttonColor, 0);
  lv_obj_set_style_shadow_opa(recorderUi.button, shouldPulse ? LV_OPA_70 : LV_OPA_40, 0);

  if (disableButton) {
    lv_obj_add_state(recorderUi.button, LV_STATE_DISABLED);
  } else {
    lv_obj_clear_state(recorderUi.button, LV_STATE_DISABLED);
  }
  updateRecorderVisuals();
}

lv_obj_t *waveformRecorderScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Recorder)];
}

bool waveformBuildRecorderScreen()
{
  if (!waveformRecorderScreenRoot()) {
    buildRecorderScreenRoot();
  }

  return recorderBuilt && waveformRecorderScreenRoot() && recorderUi.screen && recorderUi.statusLabel &&
         recorderUi.detailLabel && recorderUi.button;
}

bool waveformRefreshRecorderScreen()
{
  if (!recorderBuilt || !recorderUi.screen || !recorderUi.statusLabel || !recorderUi.detailLabel || !recorderUi.button) {
    return false;
  }

  refreshRecorderScreen();
  return true;
}

void waveformEnterRecorderScreen()
{
}

void waveformLeaveRecorderScreen()
{
  if (audioRecordingActive) {
    stopRecording(false);
  }
  if (audioPlaybackActive) {
    stopPlayback();
  }
}

void waveformTickRecorderScreen(uint32_t nowMs)
{
  (void)nowMs;
  refreshRecorderScreen();
}
