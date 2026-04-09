#include "config/pin_config.h"
#include "config/screen_constants.h"
#include "core/screen_manager.h"
#ifdef SCREEN_RECORDER
#include "drivers/es8311.h"
#include <ESP_I2S.h>
#include <SD_MMC.h>

#include "modules/storage.h"
#include "screens/screen_callbacks.h"

constexpr es8311_mic_gain_t kRecorderMicGain = ES8311_MIC_GAIN_18DB;

extern I2SClass audioI2s;
extern bool sdMounted;
extern lv_obj_t *screenRoots[];
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
void noteActivity();

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

namespace
{
constexpr uint32_t kVoiceArmWindowMs = 5000;
constexpr uint32_t kVoiceSilenceStopMs = 1400;
constexpr uint32_t kVoiceMinCaptureMs = 900;
constexpr int32_t kVoiceTriggerLevel = 2200;
constexpr int32_t kVoiceHoldLevel = 900;
}

const ScreenModule &recorderScreenModule()
{
  return kModule;
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

struct RecorderUi
{
  lv_obj_t *screen      = nullptr;
  lv_obj_t *button      = nullptr;
  lv_obj_t *statusLabel = nullptr;
};

static RecorderUi rec;
static bool recorderBuilt = false;

static bool audioHardwareReady  = false;
bool audioRecordingActive = false;
bool audioPlaybackActive  = false;
bool audioClipAvailable   = false;
static bool voiceArmActive = false;
static bool voiceTriggeredRecording = false;
static bool voiceClipCaptured = false;
static uint32_t audioRecordedBytes = 0;
static uint32_t audioPlaybackBytes = 0;
static uint32_t voiceArmStartedAtMs = 0;
static uint32_t voiceLastSpeechAtMs = 0;
static int32_t voiceLastLevel = 0;
uint32_t audioClipBytes     = 0;
uint8_t  sdCardType         = CARD_NONE;
uint64_t sdCardSizeMb       = 0;
static es8311_handle_t audioCodec  = nullptr;
static File audioRecordingFile;
static File audioPlaybackFile;
static uint8_t audioBuffer[kRecorderAudioChunkBytes] = {};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static String clipPath()
{
  return String(kRecorderClipPath);
}

static String fmtDuration(uint32_t bytes)
{
  uint32_t secs = bytes / kRecorderBytesPerSecond;
  char buf[12];
  snprintf(buf, sizeof(buf), "%02lu:%02lu",
           (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  return String(buf);
}

static int32_t audioLevelFromBuffer(const uint8_t *data, size_t size)
{
  if (!data || size < sizeof(int16_t)) return 0;

  const int16_t *samples = reinterpret_cast<const int16_t *>(data);
  size_t sampleCount = size / sizeof(int16_t);
  if (sampleCount == 0) return 0;

  uint32_t sum = 0;
  for (size_t i = 0; i < sampleCount; ++i) {
    int32_t v = samples[i];
    sum += static_cast<uint32_t>(v < 0 ? -v : v);
  }

  return static_cast<int32_t>(sum / sampleCount);
}

static void setStatus(const char *text)
{
  if (rec.statusLabel) {
    lv_label_set_text(rec.statusLabel, text);
  }
}

static void setButtonColor(lv_color_t bg, lv_color_t border)
{
  if (!rec.button) return;
  lv_obj_set_style_bg_color(rec.button, bg, 0);
  lv_obj_set_style_border_color(rec.button, border, 0);
}

// ---------------------------------------------------------------------------
// Audio hardware
// ---------------------------------------------------------------------------

static bool ensureAudioReady()
{
  if (audioHardwareReady) return true;

  audioI2s.end();
  audioI2s.setPins(AUDIO_I2S_BCLK, AUDIO_I2S_WS, AUDIO_I2S_DOUT, AUDIO_I2S_DIN, AUDIO_I2S_MCLK);
  audioI2s.setTimeout(kRecorderAudioTimeoutMs);
  if (!audioI2s.begin(I2S_MODE_STD,
                      kRecorderSampleRate,
                      I2S_DATA_BIT_WIDTH_16BIT,
                      I2S_SLOT_MODE_STEREO,
                      I2S_STD_SLOT_BOTH)) {
    setStatus("I2S failed");
    return false;
  }

  if (!audioCodec) {
    audioCodec = es8311_create(0, ES8311_ADDRRES_0);
  }
  if (!audioCodec) {
    setStatus("No codec");
    audioI2s.end();
    return false;
  }

  const es8311_clock_config_t clk = {
      .mclk_inverted       = false,
      .sclk_inverted       = false,
      .mclk_from_mclk_pin  = true,
      .mclk_frequency      = (int)(kRecorderSampleRate * 256U),
      .sample_frequency    = (int)(kRecorderSampleRate),
  };

  esp_err_t err = es8311_init(audioCodec, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
  if (err == ESP_OK) err = es8311_sample_frequency_config(audioCodec, clk.mclk_frequency, clk.sample_frequency);
  if (err == ESP_OK) err = es8311_microphone_config(audioCodec, false);
  if (err == ESP_OK) err = es8311_voice_volume_set(audioCodec, kRecorderCodecVolume, nullptr);
  if (err == ESP_OK) err = es8311_microphone_gain_set(audioCodec, kRecorderMicGain);
  if (err == ESP_OK) err = es8311_voice_mute(audioCodec, false);

  if (err != ESP_OK) {
    char msg[48];
    snprintf(msg, sizeof(msg), "Codec err: %s", esp_err_to_name(err));
    setStatus(msg);
    audioI2s.end();
    return false;
  }

  audioHardwareReady = true;
  return true;
}

// ---------------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------------

static void stopPlayback()
{
  if (!audioPlaybackActive) return;
  audioPlaybackActive = false;
  audioPlaybackBytes  = 0;
  if (audioPlaybackFile) audioPlaybackFile.close();
}

void refreshRecorderClipState()
{
  audioClipAvailable = false;
  audioClipBytes     = 0;
  if (!sdMounted) return;
  File f = SD_MMC.open(clipPath().c_str(), FILE_READ);
  if (!f) return;
  audioClipBytes     = (uint32_t)f.size();
  audioClipAvailable = audioClipBytes > 0;
  f.close();
}

static bool startPlayback()
{
  voiceArmActive = false;
  voiceTriggeredRecording = false;
  if (!sdMounted) { setStatus("No SD card"); return false; }
  refreshRecorderClipState();
  if (!audioClipAvailable) { setStatus("No clip"); return false; }
  if (!ensureAudioReady()) return false;
  if (audioRecordingActive) return false;
  stopPlayback();

  audioPlaybackFile = SD_MMC.open(clipPath().c_str(), FILE_READ);
  if (!audioPlaybackFile) { setStatus("Open failed"); return false; }

  audioPlaybackBytes  = 0;
  audioPlaybackActive = true;
  setStatus(("Playing " + fmtDuration(audioClipBytes)).c_str());
  setButtonColor(lvColor(26, 82, 154), lvColor(76, 138, 214));
  return true;
}

// ---------------------------------------------------------------------------
// Recording
// ---------------------------------------------------------------------------

static void stopRecording(bool playAfter)
{
  if (!audioRecordingActive) return;
  audioRecordingActive = false;
  if (audioRecordingFile) audioRecordingFile.close();
  refreshRecorderClipState();

  if (audioRecordedBytes == 0) {
    setStatus("Nothing captured");
    setButtonColor(lvColor(154, 24, 30), lvColor(214, 84, 90));
    return;
  }

  setStatus(("Saved " + fmtDuration(audioRecordedBytes)).c_str());
  setButtonColor(lvColor(154, 24, 30), lvColor(214, 84, 90));
  if (voiceTriggeredRecording) {
    voiceClipCaptured = audioRecordedBytes > 0;
    voiceTriggeredRecording = false;
    voiceArmActive = false;
    if (audioRecordedBytes > 0) {
      setStatus("Voice clip saved");
    }
    return;
  }
  if (playAfter) startPlayback();
}

static bool startRecording(const uint8_t *initialData = nullptr, size_t initialBytes = 0)
{
  if (!sdMounted) initSdCard();
  if (!sdMounted) { setStatus("No SD card"); return false; }
  if (!ensureAudioReady()) return false;
  stopPlayback();

  if (SD_MMC.exists(clipPath().c_str())) SD_MMC.remove(clipPath().c_str());

  audioRecordingFile = SD_MMC.open(clipPath().c_str(), FILE_WRITE);
  if (!audioRecordingFile) { setStatus("File open failed"); return false; }

  audioRecordedBytes  = 0;
  audioRecordingActive = true;
  audioClipAvailable   = false;
  if (initialData && initialBytes > 0) {
    size_t written = audioRecordingFile.write(initialData, initialBytes);
    if (written != initialBytes) {
      audioRecordingFile.close();
      audioRecordingActive = false;
      setStatus("Seed write failed");
      return false;
    }
    audioRecordedBytes = static_cast<uint32_t>(written);
  }
  setStatus("Recording...");
  setButtonColor(lvColor(184, 38, 44), lvColor(240, 98, 104));
  return true;
}

// ---------------------------------------------------------------------------
// Audio pump — called from main.cpp loop
// ---------------------------------------------------------------------------

void updateRecorderAudio()
{
  uint32_t now = millis();
  if (voiceArmActive && !audioRecordingActive) {
    if (!ensureAudioReady()) {
      voiceArmActive = false;
      return;
    }

    if (now - voiceArmStartedAtMs >= kVoiceArmWindowMs) {
      voiceArmActive = false;
      setStatus("Voice window expired");
      setButtonColor(lvColor(154, 24, 30), lvColor(214, 84, 90));
      return;
    }

    size_t bytesRead = audioI2s.readBytes(reinterpret_cast<char *>(audioBuffer), sizeof(audioBuffer));
    if (bytesRead == 0) {
      setStatus("Mic arm failed");
      voiceArmActive = false;
      return;
    }

    noteActivity();
    voiceLastLevel = audioLevelFromBuffer(audioBuffer, bytesRead);

    char status[48];
    snprintf(status, sizeof(status), "Speak now  %ld", static_cast<long>(voiceLastLevel));
    setStatus(status);
    setButtonColor(lvColor(112, 52, 10), lvColor(255, 166, 64));

    if (voiceLastLevel >= kVoiceTriggerLevel) {
      voiceTriggeredRecording = true;
      voiceLastSpeechAtMs = now;
      if (!startRecording(audioBuffer, bytesRead)) {
        voiceTriggeredRecording = false;
        voiceArmActive = false;
      }
    }
    return;
  }

  if (audioRecordingActive) {
    noteActivity();
    size_t bytesRead = audioI2s.readBytes(reinterpret_cast<char *>(audioBuffer), sizeof(audioBuffer));
    if (bytesRead == 0) {
      stopRecording(false);
      setStatus("Mic read failed");
      return;
    }
    size_t written = audioRecordingFile.write(audioBuffer, bytesRead);
    if (written != bytesRead) {
      stopRecording(false);
      setStatus("SD write failed");
      return;
    }
    audioRecordedBytes += (uint32_t)written;
    if (voiceTriggeredRecording) {
      voiceLastLevel = audioLevelFromBuffer(audioBuffer, bytesRead);
      if (voiceLastLevel >= kVoiceHoldLevel) {
        voiceLastSpeechAtMs = now;
      } else if ((now - voiceLastSpeechAtMs) >= kVoiceSilenceStopMs &&
                 audioRecordedBytes >= (kRecorderBytesPerSecond * kVoiceMinCaptureMs) / 1000U) {
        stopRecording(false);
        return;
      }
    }

    // Update status periodically with elapsed duration
    static uint32_t lastStatusUpdateMs = 0;
    if (now - lastStatusUpdateMs >= 500) {
      lastStatusUpdateMs = now;
      if (voiceTriggeredRecording) {
        char status[48];
        snprintf(status, sizeof(status), "Voice rec %s", fmtDuration(audioRecordedBytes).c_str());
        setStatus(status);
      } else {
        setStatus(("Recording " + fmtDuration(audioRecordedBytes)).c_str());
      }
    }
    return;
  }

  if (audioPlaybackActive) {
    noteActivity();
    int bytesRead = audioPlaybackFile.read(audioBuffer, sizeof(audioBuffer));
    if (bytesRead <= 0) {
      stopPlayback();
      refreshRecorderClipState();
      setStatus("Done");
      setButtonColor(lvColor(154, 24, 30), lvColor(214, 84, 90));
      return;
    }
    size_t written = audioI2s.write(audioBuffer, (size_t)bytesRead);
    if (written != (size_t)bytesRead) {
      stopPlayback();
      setStatus("Speaker failed");
      setButtonColor(lvColor(154, 24, 30), lvColor(214, 84, 90));
      return;
    }
    audioPlaybackBytes += (uint32_t)bytesRead;
  }
}

// ---------------------------------------------------------------------------
// Button event
// ---------------------------------------------------------------------------

static void onButtonEvent(lv_event_t *e)
{
  noteActivity();
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_PRESSED) {
    voiceArmActive = false;
    voiceTriggeredRecording = false;
    if (!audioPlaybackActive && !audioRecordingActive) {
      startRecording();
    }
    return;
  }

  if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    if (audioRecordingActive) {
      stopRecording(true);
    } else if (audioPlaybackActive) {
      stopPlayback();
      refreshRecorderClipState();
      setStatus(audioClipAvailable ? "Tap to play" : "Ready");
      setButtonColor(lvColor(154, 24, 30), lvColor(214, 84, 90));
    }
  }
}

// ---------------------------------------------------------------------------
// Tap handler (screen-level, for play)
// ---------------------------------------------------------------------------

void recorderHandleTap()
{
  if (!recorderBuilt) return;
  if (voiceArmActive) {
    recorderCancelVoiceArm();
    setStatus(audioClipAvailable ? "Hold to record  •  Tap to play" : "Hold to record");
    return;
  }
  if (audioRecordingActive) return;  // button handles record stop on release

  if (audioPlaybackActive) {
    stopPlayback();
    refreshRecorderClipState();
    setStatus(audioClipAvailable ? "Hold to record  •  Tap to play" : "Ready");
    setButtonColor(lvColor(154, 24, 30), lvColor(214, 84, 90));
    noteActivity();
    return;
  }

  if (audioClipAvailable) {
    startPlayback();
    noteActivity();
  }
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

static void buildRecorderScreen()
{
  rec.screen = lv_obj_create(nullptr);
  applyRootStyle(rec.screen);

  rec.button = lv_button_create(rec.screen);
  lv_obj_set_size(rec.button, kRecorderButtonDiameter, kRecorderButtonDiameter);
  lv_obj_align(rec.button, LV_ALIGN_CENTER, 0, -40);
  lv_obj_set_style_radius(rec.button, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(rec.button, lvColor(154, 24, 30), 0);
  lv_obj_set_style_bg_color(rec.button, lvColor(184, 38, 44), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(rec.button, 2, 0);
  lv_obj_set_style_border_color(rec.button, lvColor(214, 84, 90), 0);
  lv_obj_add_event_cb(rec.button, onButtonEvent, LV_EVENT_ALL, nullptr);

  // Mic icon inside button
  lv_obj_t *icon = lv_label_create(rec.button);
  lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(icon, lvColor(255, 255, 255), 0);
  lv_label_set_text(icon, LV_SYMBOL_AUDIO);
  lv_obj_center(icon);

  rec.statusLabel = lv_label_create(rec.screen);
  lv_obj_set_style_text_font(rec.statusLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(rec.statusLabel, lvColor(180, 190, 200), 0);
  lv_obj_set_width(rec.statusLabel, LCD_WIDTH - 48);
  lv_obj_set_style_text_align(rec.statusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(rec.statusLabel, LV_LABEL_LONG_WRAP);
  lv_obj_align(rec.statusLabel, LV_ALIGN_CENTER, 0, 80);

  screenRoots[static_cast<size_t>(ScreenId::Recorder)] = rec.screen;
  recorderBuilt = true;
  setStatus("Hold to record");
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

lv_obj_t *waveformRecorderScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Recorder)];
}

bool waveformBuildRecorderScreen()
{
  if (!waveformRecorderScreenRoot()) {
    buildRecorderScreen();
  }
  return recorderBuilt && waveformRecorderScreenRoot() && rec.button && rec.statusLabel;
}

bool waveformRefreshRecorderScreen()
{
  return recorderBuilt && rec.button && rec.statusLabel;
}

void waveformEnterRecorderScreen()
{
  if (!recorderBuilt) return;
  refreshRecorderClipState();
  if (!sdMounted) {
    setStatus("Insert SD card");
    lv_obj_add_state(rec.button, LV_STATE_DISABLED);
  } else {
    lv_obj_clear_state(rec.button, LV_STATE_DISABLED);
    setStatus(audioClipAvailable ? "Hold to record  •  Tap to play" : "Hold to record");
  }
}

void waveformLeaveRecorderScreen()
{
  recorderCancelVoiceArm();
  if (audioRecordingActive) stopRecording(false);
  if (audioPlaybackActive)  stopPlayback();
  audioI2s.end();
  audioHardwareReady = false;
}

void waveformTickRecorderScreen(uint32_t nowMs)
{
  (void)nowMs;
}

bool recorderStartVoiceArm()
{
  if (!recorderBuilt) return false;
  if (!sdMounted) initSdCard();
  if (!sdMounted) {
    setStatus("No SD card");
    return false;
  }
  if (!ensureAudioReady()) return false;

  stopPlayback();
  voiceClipCaptured = false;
  voiceTriggeredRecording = false;
  voiceArmActive = true;
  voiceArmStartedAtMs = millis();
  voiceLastSpeechAtMs = voiceArmStartedAtMs;
  voiceLastLevel = 0;
  setStatus("Raise detected  •  speak");
  setButtonColor(lvColor(112, 52, 10), lvColor(255, 166, 64));
  return true;
}

void recorderCancelVoiceArm()
{
  voiceArmActive = false;
  voiceTriggeredRecording = false;
  voiceLastLevel = 0;
}

bool recorderVoiceArmActive()
{
  return voiceArmActive;
}

bool recorderVoiceClipCaptured()
{
  return voiceClipCaptured;
}

#endif // SCREEN_RECORDER
