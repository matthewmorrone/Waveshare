#include <Adafruit_XCA9554.h>
#include <Arduino.h>
#include <Arduino_DriveBus_Library.h>
#include <Arduino_GFX_Library.h>
#include <ESP_I2S.h>
#include <FS.h>
#include <SD_MMC.h>
#include <Wire.h>
#include <math.h>
#include <vector>

#include "cloud_sprite_assets_generated.h"
#include "drivers/es8311.h"
#include "drivers/es8311_reg.h"
#include "pin_config.h"
#include "star_locket_melody_pcm.h"
#include <XPowersLib.h>

namespace
{
constexpr uint8_t kExpanderAddress = 0x20;
constexpr size_t kBackgroundStarCount = 84;
constexpr size_t kCloudCount = 4;
constexpr uint32_t kFrameIntervalMs = 16;
constexpr float kSceneDriftPixelsPerSecond = 22.0f;

constexpr uint16_t kSkyTopNight = 0x006D;
constexpr uint16_t kSkyUpperMid = 0x19DE;
constexpr uint16_t kSkyLowerMid = 0x43BF;
constexpr uint16_t kSkyHorizon = 0x7DFF;
constexpr uint16_t kStarWhite = 0xFFFF;
constexpr uint16_t kStarBlue = 0xD71F;
constexpr uint16_t kCloudHighlight = 0xFFFF;
constexpr uint16_t kCloudBody = 0xE73C;
constexpr uint16_t kCloudShadow = 0xB5D9;
constexpr uint16_t kMoonWhite = 0xFFFF;
constexpr uint16_t kMoonGlow = 0xE73F;
constexpr uint16_t kOrbCore = 0xC800;
constexpr uint16_t kOrbMid = 0xE0A3;
constexpr uint16_t kOrbEdge = 0x8800;
constexpr uint16_t kOrbHighlight = 0xFBEF;
constexpr uint16_t kOrbGold = 0xFEA0;
constexpr uint16_t kOrbGoldHot = 0xFF66;
constexpr int kAudioSampleRate = 16000;
constexpr int kAudioVolumePct = 75;
constexpr es8311_mic_gain_t kAudioMicGain = static_cast<es8311_mic_gain_t>(3);
constexpr size_t kAudioReadBufferBytes = 2048;
constexpr bool kAudioDebugMode = true;
constexpr size_t kAudioDebugChunkBytes = 8192;
constexpr uint32_t kSdPollIntervalMs = 800;
constexpr uint32_t kTapMaxDurationMs = 350;
constexpr int kTapMaxTravelPx = 18;
constexpr size_t kSerialLineBufferBytes = 192;
constexpr size_t kSerialTransferChunkBytes = 512;

constexpr const char *kSdMountPath = "/sdcard";
constexpr const char *kCloudSpriteDir = "/locket/clouds";
constexpr const char *kAudioDir = "/locket/audio";
constexpr const char *kCloudSpriteFiles[kEmbeddedCloudSpriteCount] = {
    "01.cld",
    "02.cld",
    "03.cld",
};
constexpr uint8_t kUsableSpriteIndices[] = {0, 1};
constexpr const uint8_t *kEmbeddedTrackPcm =
    _Users_matthewmorrone_Documents_Arduino_Waveshare_VendorAudioDemo_output_star_locket_melody_pcm;
constexpr size_t kEmbeddedTrackPcmLen =
    _Users_matthewmorrone_Documents_Arduino_Waveshare_VendorAudioDemo_output_star_locket_melody_pcm_len;

struct BackgroundStar
{
  float x = 0.0f;
  int16_t y = 0;
  uint8_t radius = 1;
  uint16_t color = kStarWhite;
};

struct CloudSprite
{
  uint16_t fallbackWidth = 0;
  uint16_t fallbackHeight = 0;
  const uint8_t *fallbackPixels = nullptr;
  uint16_t sdWidth = 0;
  uint16_t sdHeight = 0;
  uint8_t *sdPixels = nullptr;

  const uint8_t *pixels() const
  {
    return sdPixels ? sdPixels : fallbackPixels;
  }

  uint16_t width() const
  {
    return sdPixels ? sdWidth : fallbackWidth;
  }

  uint16_t height() const
  {
    return sdPixels ? sdHeight : fallbackHeight;
  }

  bool usingSd() const
  {
    return sdPixels != nullptr;
  }
};

struct Cloud
{
  float x;
  int16_t y;
  float speed;
  uint8_t spriteIndex;
  uint8_t scalePct;
  uint8_t alphaPct;
  uint8_t lane;
  bool flipX;
};

Adafruit_XCA9554 gExpander;
XPowersPMU gPower;
std::shared_ptr<Arduino_IIC_DriveBus> gTouchBus = std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);
std::unique_ptr<Arduino_IIC> gTouchController(
    new Arduino_FT3x68(gTouchBus, FT3168_DEVICE_ADDRESS, DRIVEBUS_DEFAULT_VALUE, TP_INT, nullptr));
bool gExpanderReady = false;
bool gPowerReady = false;
bool gSdMounted = false;
uint8_t gSdSpriteCount = 0;
uint8_t gSpriteCursor = 0;
uint32_t gLastFrameAtMs = 0;
bool gUseCanvas = false;
bool gAudioReady = false;
bool gTouchReady = false;
bool gTouchPressed = false;
bool gTapTracking = false;
bool gAnimationPaused = true;
bool gSdPresenceInitialized = false;
bool gSdPresent = false;
uint32_t gLastSdPollAtMs = 0;
uint32_t gTapStartedAtMs = 0;
uint64_t gAnimationStartUs = 0;
uint64_t gAnimationPausedAtUs = 0;
uint64_t gAnimationPausedAccumUs = 0;
int16_t gTouchX = LCD_WIDTH / 2;
int16_t gTouchY = LCD_HEIGHT / 2;
int16_t gTapStartX = LCD_WIDTH / 2;
int16_t gTapStartY = LCD_HEIGHT / 2;

BackgroundStar gBackgroundStars[kBackgroundStarCount];
CloudSprite gCloudSprites[kEmbeddedCloudSpriteCount];
Cloud gClouds[kCloudCount];
es8311_handle_t gAudioCodec = nullptr;
I2SClass gAudioI2s;
const char *gActiveAudioPath = nullptr;
File gAudioFile;
uint32_t gAudioDataRemaining = 0;
volatile bool gSkipTrack = false;
uint8_t gAudioReadBuffer[kAudioReadBufferBytes] = {};
uint16_t gAudioSourceChannels = 0;
uint32_t gAudioSourceSampleRate = 0;
size_t gAudioDebugPcmOffset = 0;
bool gSerialTransferActive = false;
File gSerialTransferFile;
uint32_t gSerialTransferExpected = 0;
uint32_t gSerialTransferReceived = 0;
char gSerialLine[kSerialLineBufferBytes] = {};
size_t gSerialLineLen = 0;
uint8_t gSerialTransferBuffer[kSerialTransferChunkBytes] = {};
TaskHandle_t gAudioTaskHandle = nullptr;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS,
    LCD_SCLK,
    LCD_SDIO0,
    LCD_SDIO1,
    LCD_SDIO2,
    LCD_SDIO3);

Arduino_SH8601 *gfx = new Arduino_SH8601(
    bus,
    GFX_NOT_DEFINED,
    0,
    LCD_WIDTH,
    LCD_HEIGHT);

class PsramCanvas : public Arduino_Canvas
{
public:
  PsramCanvas(int16_t w, int16_t h, Arduino_G *output)
      : Arduino_Canvas(w, h, output)
  {
  }

  bool begin(int32_t speed = GFX_NOT_DEFINED) override
  {
    if ((speed != GFX_SKIP_OUTPUT_BEGIN) && _output) {
      if (!_output->begin(speed)) {
        return false;
      }
    }

    if (!_framebuffer) {
      const size_t size = static_cast<size_t>(_width) * static_cast<size_t>(_height) * 2U;
#if defined(ESP32)
      _framebuffer = psramFound()
                         ? static_cast<uint16_t *>(ps_malloc(size))
                         : static_cast<uint16_t *>(malloc(size));
#else
      _framebuffer = static_cast<uint16_t *>(malloc(size));
#endif
      if (!_framebuffer) {
        return false;
      }
    }

    return true;
  }

  uint16_t *framebuffer()
  {
    return _framebuffer;
  }
};

PsramCanvas *canvas = nullptr;
Arduino_GFX *scene = gfx;

bool initSdCard();
void startAudioPlayback();
void closeAudioPlayback();
bool initTouch();
void serviceTouch();
void serviceAudioDebug();
bool beginAudioI2s();
void audioTaskMain(void *arg);
void resetBackgroundStar(size_t index, bool initialPlacement);

bool initPower()
{
  gPowerReady = gPower.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (!gPowerReady) {
    Serial.println("Locket: PMU not found");
    return false;
  }

  gPower.enableBattDetection();
  gPower.enableVbusVoltageMeasure();
  gPower.enableBattVoltageMeasure();
  gPower.enableSystemVoltageMeasure();
  gPower.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  gPower.clearIrqStatus();
  gPower.setIrqLevelTime(XPOWERS_AXP2101_IRQ_TIME_1S);
  gPower.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
  gPower.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ |
                   XPOWERS_AXP2101_PKEY_LONG_IRQ |
                   XPOWERS_AXP2101_PKEY_NEGATIVE_IRQ |
                   XPOWERS_AXP2101_PKEY_POSITIVE_IRQ);
  return true;
}

bool ensureSdDir(const char *path)
{
  if (!path || path[0] != '/') {
    return false;
  }

  char partial[128];
  size_t out = 0;
  partial[out++] = '/';
  partial[out] = '\0';

  for (const char *cursor = path + 1; *cursor && out + 1 < sizeof(partial); ++cursor) {
    if (*cursor == '/') {
      partial[out] = '\0';
      SD_MMC.mkdir(partial);
    }
    partial[out++] = *cursor;
    partial[out] = '\0';
  }

  return true;
}

bool ensureParentDirs(const char *path)
{
  if (!path || path[0] != '/') {
    return false;
  }

  const char *lastSlash = strrchr(path, '/');
  if (!lastSlash || lastSlash == path) {
    return true;
  }

  char parent[128];
  const size_t len = static_cast<size_t>(lastSlash - path);
  if (len >= sizeof(parent)) {
    return false;
  }

  memcpy(parent, path, len);
  parent[len] = '\0';
  return ensureSdDir(parent);
}

uint16_t blend565(uint16_t a, uint16_t b, uint8_t amount)
{
  const uint8_t ra = (a >> 11) & 0x1F;
  const uint8_t ga = (a >> 5) & 0x3F;
  const uint8_t ba = a & 0x1F;

  const uint8_t rb = (b >> 11) & 0x1F;
  const uint8_t gb = (b >> 5) & 0x3F;
  const uint8_t bb = b & 0x1F;

  const uint8_t r = ra + (((rb - ra) * amount) / 255);
  const uint8_t g = ga + (((gb - ga) * amount) / 255);
  const uint8_t bl = ba + (((bb - ba) * amount) / 255);
  return (r << 11) | (g << 5) | bl;
}

uint8_t clampU8(int value)
{
  if (value < 0) {
    return 0;
  }
  if (value > 255) {
    return 255;
  }
  return static_cast<uint8_t>(value);
}

bool initExpanderAndRails()
{
  bool ready = false;
  for (int attempt = 0; attempt < 3 && !ready; ++attempt) {
    if (attempt > 0) {
      Wire.end();
      delay(10);
      Wire.begin(IIC_SDA, IIC_SCL);
      Wire.setClock(400000);
      delay(10);
    }
    ready = gExpander.begin(kExpanderAddress);
  }

  if (!ready) {
    return false;
  }

  gExpander.pinMode(PMU_IRQ_PIN, INPUT);
  gExpander.pinMode(TOP_BUTTON_PIN, INPUT);
  gExpander.pinMode(0, OUTPUT);
  gExpander.pinMode(1, OUTPUT);
  gExpander.pinMode(2, OUTPUT);
  gExpander.pinMode(6, OUTPUT);
  gExpander.pinMode(7, OUTPUT);

  gExpander.digitalWrite(0, LOW);
  gExpander.digitalWrite(1, LOW);
  gExpander.digitalWrite(2, LOW);
  gExpander.digitalWrite(6, LOW);
  delay(20);
  gExpander.digitalWrite(0, HIGH);
  gExpander.digitalWrite(1, HIGH);
  gExpander.digitalWrite(2, HIGH);
  gExpander.digitalWrite(6, HIGH);
  gExpander.digitalWrite(7, HIGH);
  delay(20);

  return true;
}

bool initAudioHardware()
{
  if (gAudioReady) {
    return true;
  }

  pinMode(AUDIO_POWER_AMP, OUTPUT);
  digitalWrite(AUDIO_POWER_AMP, HIGH);
  delay(10);
  Serial.printf("Locket: audio amp enable GPIO%d HIGH\n", AUDIO_POWER_AMP);

  if (!beginAudioI2s()) {
    Serial.println("Locket: I2S begin failed");
    return false;
  }

  if (!gAudioCodec) {
    gAudioCodec = es8311_create(0, ES8311_ADDRRES_0);
  }
  if (!gAudioCodec) {
    Serial.println("Locket: audio codec alloc failed");
    return false;
  }

  const es8311_clock_config_t clk = {
      .mclk_inverted = false,
      .sclk_inverted = false,
      .mclk_from_mclk_pin = true,
      .mclk_frequency = kAudioSampleRate * 256,
      .sample_frequency = kAudioSampleRate,
  };

  esp_err_t err = es8311_init(gAudioCodec, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
  if (err == ESP_OK) err = es8311_sample_frequency_config(gAudioCodec, clk.mclk_frequency, clk.sample_frequency);
  if (err == ESP_OK) err = es8311_microphone_config(gAudioCodec, false);
  if (err == ESP_OK) err = es8311_voice_volume_set(gAudioCodec, kAudioVolumePct, nullptr);
  if (err == ESP_OK) err = es8311_microphone_gain_set(gAudioCodec, kAudioMicGain);
  if (err != ESP_OK) {
    Serial.printf("Locket: codec init failed: %s\n", esp_err_to_name(err));
    gAudioI2s.end();
    return false;
  }

  gAudioReady = true;
  return true;
}

bool beginAudioI2s()
{
  gAudioI2s.end();
  gAudioI2s.setPins(AUDIO_I2S_BCLK, AUDIO_I2S_WS, AUDIO_I2S_DOUT, AUDIO_I2S_DIN, AUDIO_I2S_MCLK);
  return gAudioI2s.begin(I2S_MODE_STD,
                         kAudioSampleRate,
                         I2S_DATA_BIT_WIDTH_16BIT,
                         I2S_SLOT_MODE_STEREO,
                         I2S_STD_SLOT_BOTH);
}

void playTone(float freqHz, uint32_t durationMs)
{
  if (!initAudioHardware()) {
    return;
  }

  const uint32_t sampleCount = (kAudioSampleRate * durationMs) / 1000U;
  const uint32_t fadeSamples = min<uint32_t>(sampleCount / 8U, 320U);
  int16_t stereoFrame[2];

  for (uint32_t index = 0; index < sampleCount; ++index) {
    float env = 1.0f;
    if (fadeSamples > 0 && index < fadeSamples) {
      env = static_cast<float>(index) / static_cast<float>(fadeSamples);
    }
    if (fadeSamples > 0 && index + fadeSamples >= sampleCount) {
      const uint32_t tail = sampleCount - index;
      env = min(env, static_cast<float>(tail) / static_cast<float>(fadeSamples));
    }

    const float phase = 2.0f * static_cast<float>(PI) * freqHz * static_cast<float>(index) / static_cast<float>(kAudioSampleRate);
    const int16_t sample = static_cast<int16_t>(sinf(phase) * 22000.0f * env);
    stereoFrame[0] = sample;
    stereoFrame[1] = sample;
    gAudioI2s.write(reinterpret_cast<const uint8_t *>(stereoFrame), sizeof(stereoFrame));
  }
}

void playBootAudioDiagnostic()
{
  Serial.println("Locket: boot audio diagnostic start");
  initAudioHardware();
  Serial.println("Locket: boot audio diagnostic end");
}

void playSdInsertedChime()
{
  playTone(659.25f, 110);
  playTone(880.0f, 140);
}

void playSdRemovedChime()
{
  playTone(587.33f, 120);
  playTone(392.0f, 170);
}

void serviceAudioDebug()
{
  if (!kAudioDebugMode || !gAudioReady) {
    return;
  }

  if (gAnimationPaused) {
    return;
  }

  const size_t remaining = kEmbeddedTrackPcmLen - gAudioDebugPcmOffset;
  const size_t chunkSize = min<size_t>(kAudioDebugChunkBytes, remaining);
  if (chunkSize == 0) {
    gAudioDebugPcmOffset = kEmbeddedTrackPcmLen;
    gAnimationPaused = true;
    gAnimationPausedAtUs = esp_timer_get_time();
    return;
  }

  gAudioI2s.write(kEmbeddedTrackPcm + gAudioDebugPcmOffset, chunkSize);
  gAudioDebugPcmOffset += chunkSize;
  if (gAudioDebugPcmOffset >= kEmbeddedTrackPcmLen) {
    gAudioDebugPcmOffset = kEmbeddedTrackPcmLen;
    gAnimationPaused = true;
    gAnimationPausedAtUs = esp_timer_get_time();
  }
}

uint32_t readLe32(const uint8_t *bytes)
{
  return static_cast<uint32_t>(bytes[0])
       | (static_cast<uint32_t>(bytes[1]) << 8)
       | (static_cast<uint32_t>(bytes[2]) << 16)
       | (static_cast<uint32_t>(bytes[3]) << 24);
}

uint16_t readLe16(const uint8_t *bytes)
{
  return static_cast<uint16_t>(bytes[0])
       | (static_cast<uint16_t>(bytes[1]) << 8);
}

void dumpDir(const char *path, uint8_t depth = 0)
{
  if (depth > 2) {
    return;
  }

  File dir = SD_MMC.open(path);
  if (!dir) {
    Serial.printf("Locket: dir open failed %s\n", path);
    return;
  }
  if (!dir.isDirectory()) {
    Serial.printf("Locket: not a dir %s\n", path);
    dir.close();
    return;
  }

  Serial.printf("Locket: dir %s\n", path);
  File entry = dir.openNextFile();
  while (entry) {
    Serial.printf("  %s%s\n", entry.name(), entry.isDirectory() ? "/" : "");
    if (entry.isDirectory()) {
      char child[128];
      snprintf(child, sizeof(child), "%s/%s", path[1] ? path : "", entry.name());
      dumpDir(child[0] ? child : "/", depth + 1);
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
}

bool openAudioFile()
{
  if (!initSdCard()) {
    Serial.println("Locket: audio skipped, SD unavailable");
    return false;
  }

  if (gAudioFile) {
    gAudioFile.close();
  }
  gAudioDataRemaining = 0;

  // Scan kAudioDir for .wav files and pick one at random
  std::vector<String> wavFiles;
  {
    File dir = SD_MMC.open(kAudioDir);
    if (dir && dir.isDirectory()) {
      File entry = dir.openNextFile();
      while (entry) {
        if (!entry.isDirectory()) {
          String name = entry.name();
          String lower = name;
          lower.toLowerCase();
          if (lower.endsWith(".wav")) {
            // entry.name() returns the full path on SD_MMC
            wavFiles.push_back(name);
          }
        }
        entry.close();
        entry = dir.openNextFile();
      }
      dir.close();
    }
  }

  if (wavFiles.empty()) {
    Serial.println("Locket: no WAV found on SD");
    dumpDir("/");
    dumpDir("/locket");
    dumpDir("/locket/audio");
    return false;
  }

  // Avoid replaying the same file if there are multiple choices
  String chosen;
  if (wavFiles.size() == 1) {
    chosen = wavFiles[0];
  } else {
    do {
      chosen = wavFiles[esp_random() % wavFiles.size()];
    } while (gActiveAudioPath && chosen == gActiveAudioPath);
  }

  static String gSelectedAudioPath;
  gSelectedAudioPath = chosen;
  gActiveAudioPath = gSelectedAudioPath.c_str();

  if (gAudioFile) {
    gAudioFile.close();
  }
  gAudioFile = SD_MMC.open(gActiveAudioPath, FILE_READ);
  if (!gAudioFile) {
    Serial.printf("Locket: failed to open WAV %s\n", gActiveAudioPath);
    return false;
  }

  uint8_t riffHeader[12];
  if (gAudioFile.read(riffHeader, sizeof(riffHeader)) != static_cast<int>(sizeof(riffHeader))) {
    Serial.println("Locket: short WAV header");
    gAudioFile.close();
    return false;
  }
  if (memcmp(riffHeader, "RIFF", 4) != 0 || memcmp(riffHeader + 8, "WAVE", 4) != 0) {
    Serial.println("Locket: invalid WAV container");
    gAudioFile.close();
    return false;
  }

  bool foundFmt = false;
  bool foundData = false;
  uint16_t audioFormat = 0;
  uint16_t channels = 0;
  uint32_t sampleRate = 0;
  uint16_t bitsPerSample = 0;
  gAudioDataRemaining = 0;

  while (gAudioFile.available()) {
    uint8_t chunkHeader[8];
    if (gAudioFile.read(chunkHeader, sizeof(chunkHeader)) != static_cast<int>(sizeof(chunkHeader))) {
      break;
    }

    const uint32_t chunkSize = readLe32(chunkHeader + 4);
    if (memcmp(chunkHeader, "fmt ", 4) == 0) {
      uint8_t fmt[16];
      if (chunkSize < sizeof(fmt) || gAudioFile.read(fmt, sizeof(fmt)) != static_cast<int>(sizeof(fmt))) {
        Serial.println("Locket: invalid WAV fmt");
        gAudioFile.close();
        return false;
      }
      audioFormat = readLe16(fmt + 0);
      channels = readLe16(fmt + 2);
      sampleRate = readLe32(fmt + 4);
      bitsPerSample = readLe16(fmt + 14);
      if (chunkSize > sizeof(fmt)) {
        gAudioFile.seek(gAudioFile.position() + (chunkSize - sizeof(fmt)));
      }
      foundFmt = true;
    } else if (memcmp(chunkHeader, "data", 4) == 0) {
      gAudioDataRemaining = chunkSize;
      foundData = true;
      break;
    } else {
      gAudioFile.seek(gAudioFile.position() + chunkSize);
    }

    if (chunkSize & 1U) {
      gAudioFile.seek(gAudioFile.position() + 1);
    }
  }

  if (!foundFmt || !foundData) {
    Serial.println("Locket: missing WAV chunks");
    gAudioFile.close();
    return false;
  }
  if (audioFormat != 1 || bitsPerSample != 16 || channels != 2 || sampleRate != static_cast<uint32_t>(kAudioSampleRate)) {
    Serial.printf("Locket: unsupported WAV format a=%u ch=%u sr=%lu bits=%u\n",
                  static_cast<unsigned>(audioFormat),
                  static_cast<unsigned>(channels),
                  static_cast<unsigned long>(sampleRate),
                  static_cast<unsigned>(bitsPerSample));
    gAudioFile.close();
    return false;
  }

  gAudioSourceChannels = channels;
  gAudioSourceSampleRate = sampleRate;
  Serial.printf("Locket: playing %s\n", gActiveAudioPath);
  return true;
}

void closeAudioPlayback()
{
  if (gAudioFile) {
    gAudioFile.close();
  }
  gAudioDataRemaining = 0;
  gActiveAudioPath = nullptr;
  gAudioSourceChannels = 0;
  gAudioSourceSampleRate = 0;
}

void startAudioPlayback()
{
  if (!initAudioHardware()) {
    return;
  }
  if (kAudioDebugMode) {
    gAudioDebugPcmOffset = 0;
    return;
  }
  openAudioFile();
}

void updateAudioPlayback()
{
  if (gSerialTransferActive) {
    return;
  }

  if (kAudioDebugMode) {
    if (!gAudioReady) {
      startAudioPlayback();
      if (!gAudioReady) {
        return;
      }
    }
    serviceAudioDebug();
    return;
  }

  if (gSkipTrack) {
    gSkipTrack = false;
    closeAudioPlayback();
    openAudioFile();
    return;
  }

  if (!gAudioFile) {
    startAudioPlayback();
    return;
  }

  if (gAudioDataRemaining == 0) {
    openAudioFile();
    return;
  }

  const size_t toRead = min<size_t>(sizeof(gAudioReadBuffer), gAudioDataRemaining);
  const int bytesRead = gAudioFile.read(gAudioReadBuffer, toRead);
  if (bytesRead <= 0) {
    openAudioFile();
    return;
  }

  if ((bytesRead & 0x3) != 0 || gAudioSourceChannels != 2 || gAudioSourceSampleRate != static_cast<uint32_t>(kAudioSampleRate)) {
    openAudioFile();
    return;
  }

  const size_t outputBytes = static_cast<size_t>(bytesRead);
  const size_t written = gAudioI2s.write(reinterpret_cast<const uint8_t *>(gAudioReadBuffer), outputBytes);
  if (written != outputBytes) {
    Serial.println("Locket: speaker write failed");
  }
  gAudioDataRemaining -= static_cast<uint32_t>(bytesRead);
}

void audioTaskMain(void *arg)
{
  (void)arg;
  for (;;) {
    updateAudioPlayback();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

bool probeSdPresent()
{
  if (gSdMounted) {
    const uint8_t cardType = SD_MMC.cardType();
    if (cardType != CARD_NONE) {
      return true;
    }

    closeAudioPlayback();
    SD_MMC.end();
    gSdMounted = false;
    Serial.println("Locket: SD removed");
    return false;
  }

  return initSdCard();
}

void serviceSdPresence()
{
  const uint32_t now = millis();
  if (now - gLastSdPollAtMs < kSdPollIntervalMs) {
    return;
  }
  gLastSdPollAtMs = now;

  const bool presentNow = probeSdPresent();
  if (!gSdPresenceInitialized) {
    gSdPresent = presentNow;
    gSdPresenceInitialized = true;
    return;
  }

  if (presentNow == gSdPresent) {
    return;
  }

  gSdPresent = presentNow;
  closeAudioPlayback();
  if (presentNow) {
    playSdInsertedChime();
    startAudioPlayback();
  } else {
    playSdRemovedChime();
  }
}

void finishSerialTransfer(bool success, const char *message)
{
  if (gSerialTransferFile) {
    gSerialTransferFile.flush();
    gSerialTransferFile.close();
  }
  if (success) {
    Serial.printf("OK %lu\n", static_cast<unsigned long>(gSerialTransferReceived));
  } else {
    Serial.printf("ERR %s\n", message ? message : "transfer");
  }
  gSerialTransferActive = false;
  gSerialTransferExpected = 0;
  gSerialTransferReceived = 0;
}

void handleSerialCommand(const char *line)
{
  if (strcmp(line, "PING") == 0) {
    Serial.println("PONG");
    return;
  }

  if (strcmp(line, "STAT") == 0) {
    const bool sdReady = initSdCard();
    Serial.printf("STAT SD=%s AUDIO=%s\n", sdReady ? "IN" : "NO", gAudioReady ? "READY" : "OFF");
    return;
  }

  if (strncmp(line, "PUT ", 4) == 0) {
    if (!initSdCard()) {
      Serial.println("ERR no-sd");
      return;
    }

    char path[128];
    unsigned long expected = 0;
    if (sscanf(line + 4, "%127s %lu", path, &expected) != 2 || path[0] != '/' || expected == 0) {
      Serial.println("ERR bad-put");
      return;
    }

    if (!ensureParentDirs(path)) {
      Serial.println("ERR mkdir");
      return;
    }

    closeAudioPlayback();
    if (gSerialTransferFile) {
      gSerialTransferFile.close();
    }
    SD_MMC.remove(path);
    gSerialTransferFile = SD_MMC.open(path, FILE_WRITE);
    if (!gSerialTransferFile) {
      Serial.println("ERR open");
      return;
    }

    gSerialTransferExpected = static_cast<uint32_t>(expected);
    gSerialTransferReceived = 0;
    gSerialTransferActive = true;
    Serial.printf("READY %lu\n", static_cast<unsigned long>(gSerialTransferExpected));
    return;
  }

  Serial.println("ERR unknown");
}

void serviceSerialTransfer()
{
  while (Serial.available() > 0) {
    if (gSerialTransferActive) {
      const size_t chunk = min<size_t>(static_cast<size_t>(Serial.available()),
                                       min<size_t>(sizeof(gSerialTransferBuffer),
                                                   gSerialTransferExpected - gSerialTransferReceived));
      if (chunk == 0) {
        return;
      }

      const size_t bytesRead = Serial.readBytes(gSerialTransferBuffer, chunk);
      if (bytesRead == 0) {
        return;
      }

      const size_t written = gSerialTransferFile.write(gSerialTransferBuffer, bytesRead);
      if (written != bytesRead) {
        finishSerialTransfer(false, "write");
        return;
      }

      gSerialTransferReceived += static_cast<uint32_t>(written);
      if (gSerialTransferReceived >= gSerialTransferExpected) {
        finishSerialTransfer(true, nullptr);
        startAudioPlayback();
      }
      continue;
    }

    const char ch = static_cast<char>(Serial.read());
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      gSerialLine[gSerialLineLen] = '\0';
      if (gSerialLineLen > 0) {
        handleSerialCommand(gSerialLine);
      }
      gSerialLineLen = 0;
      continue;
    }

    if (gSerialLineLen + 1 < sizeof(gSerialLine)) {
      gSerialLine[gSerialLineLen++] = ch;
    } else {
      gSerialLineLen = 0;
      Serial.println("ERR line");
    }
  }
}

const char *cardTypeText(uint8_t cardType)
{
  switch (cardType) {
    case CARD_MMC:
      return "MMC";
    case CARD_SD:
      return "SDSC";
    case CARD_SDHC:
      return "SDHC";
    default:
      return "UNKNOWN";
  }
}

bool initSdCard()
{
  if (gSdMounted) {
    return true;
  }

  if (gExpanderReady) {
    gExpander.digitalWrite(7, HIGH);
    delay(20);
  }

  for (int attempt = 1; attempt <= 3; ++attempt) {
    SD_MMC.end();
    delay(20);
    SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);

    if (!SD_MMC.begin(kSdMountPath, true)) {
      Serial.printf("Locket: SD mount attempt %d failed\n", attempt);
      delay(80);
      continue;
    }

    const uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
      Serial.printf("Locket: SD cardType attempt %d returned none\n", attempt);
      SD_MMC.end();
      delay(80);
      continue;
    }

    gSdMounted = true;
    Serial.printf("Locket: SD mounted: %s\n", cardTypeText(cardType));
    return true;
  }

  Serial.println("Locket: SD unavailable, using embedded clouds");
  return false;
}

bool loadSpriteFromSd(const char *path, CloudSprite &sprite)
{
  File file = SD_MMC.open(path, FILE_READ);
  if (!file) {
    return false;
  }

  uint8_t header[8];
  if (file.read(header, sizeof(header)) != static_cast<int>(sizeof(header))) {
    file.close();
    return false;
  }

  if (header[0] != 'C' || header[1] != 'L' || header[2] != 'D' || header[3] != '1') {
    file.close();
    return false;
  }

  const uint16_t width = static_cast<uint16_t>(header[4] | (header[5] << 8));
  const uint16_t height = static_cast<uint16_t>(header[6] | (header[7] << 8));
  const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height);
  if (size == 0) {
    file.close();
    return false;
  }

#if defined(ESP32)
  uint8_t *pixels = psramFound()
                        ? static_cast<uint8_t *>(ps_malloc(size))
                        : static_cast<uint8_t *>(malloc(size));
#else
  uint8_t *pixels = static_cast<uint8_t *>(malloc(size));
#endif
  if (!pixels) {
    file.close();
    return false;
  }

  if (file.read(pixels, size) != static_cast<int>(size)) {
    free(pixels);
    file.close();
    return false;
  }

  file.close();

  if (sprite.sdPixels) {
    free(sprite.sdPixels);
  }
  sprite.sdWidth = width;
  sprite.sdHeight = height;
  sprite.sdPixels = pixels;
  return true;
}

void initCloudSprites()
{
  for (size_t index = 0; index < kEmbeddedCloudSpriteCount; ++index) {
    CloudSprite &sprite = gCloudSprites[index];
    sprite.fallbackWidth = kEmbeddedCloudSprites[index].width;
    sprite.fallbackHeight = kEmbeddedCloudSprites[index].height;
    sprite.fallbackPixels = kEmbeddedCloudSprites[index].pixels;
    sprite.sdWidth = 0;
    sprite.sdHeight = 0;
    sprite.sdPixels = nullptr;
  }

  gSdSpriteCount = 0;
  if (!initSdCard()) {
    return;
  }

  char path[64];
  for (size_t index = 0; index < kEmbeddedCloudSpriteCount; ++index) {
    snprintf(path, sizeof(path), "%s/%s", kCloudSpriteDir, kCloudSpriteFiles[index]);
    if (loadSpriteFromSd(path, gCloudSprites[index])) {
      ++gSdSpriteCount;
      Serial.printf("Locket: loaded cloud sprite from %s\n", path);
    }
  }

  if (gSdSpriteCount == 0) {
    Serial.println("Locket: no SD cloud sprites found, using embedded clouds");
  }
}

uint8_t chooseNextSpriteIndex(uint8_t previous)
{
  constexpr size_t usableSpriteCount = sizeof(kUsableSpriteIndices) / sizeof(kUsableSpriteIndices[0]);
  if (usableSpriteCount <= 1) {
    return kUsableSpriteIndices[0];
  }

  uint8_t candidate = kUsableSpriteIndices[gSpriteCursor % usableSpriteCount];
  gSpriteCursor = (gSpriteCursor + 1) % usableSpriteCount;
  if (candidate == previous) {
    candidate = kUsableSpriteIndices[gSpriteCursor % usableSpriteCount];
    gSpriteCursor = (gSpriteCursor + 1) % usableSpriteCount;
  }
  return candidate;
}

int16_t cloudDrawWidth(const Cloud &cloud)
{
  return static_cast<int16_t>((static_cast<uint32_t>(gCloudSprites[cloud.spriteIndex].width()) * cloud.scalePct) / 100U);
}

int16_t cloudDrawHeight(const Cloud &cloud)
{
  return static_cast<int16_t>((static_cast<uint32_t>(gCloudSprites[cloud.spriteIndex].height()) * cloud.scalePct) / 100U);
}

void resetCloud(size_t index, bool initialPlacement)
{
  Cloud &cloud = gClouds[index];
  const uint8_t previousSprite = cloud.spriteIndex;
  cloud.spriteIndex = chooseNextSpriteIndex(previousSprite);
  cloud.lane = static_cast<uint8_t>(index);
  cloud.scalePct = static_cast<uint8_t>(random(118, 168));
  cloud.alphaPct = static_cast<uint8_t>(random(84, 98));
  cloud.flipX = random(0, 2) == 0;

  const int16_t height = max<int16_t>(12, cloudDrawHeight(cloud));
  const int16_t width = max<int16_t>(12, cloudDrawWidth(cloud));
  const int16_t canopyTop = LCD_HEIGHT - 136;
  const int16_t yMin = max<int16_t>(canopyTop, LCD_HEIGHT - height - 44);
  const int16_t yMax = max<int16_t>(yMin + 1, LCD_HEIGHT - height + 24);
  cloud.y = static_cast<int16_t>(random(yMin, yMax));

  const float depthBias = 1.18f - (static_cast<float>(cloud.scalePct) / 150.0f);
  cloud.speed = (random(7, 14) / 10.0f) * depthBias;

  const int16_t laneSpacing = LCD_WIDTH / static_cast<int16_t>(kCloudCount - 1);
  const int16_t laneCenter = static_cast<int16_t>(cloud.lane * laneSpacing);
  const int16_t overlapBias = static_cast<int16_t>(width / 3);
  cloud.x = initialPlacement
                ? static_cast<float>(laneCenter - overlapBias + random(-18, 28))
                : static_cast<float>(LCD_WIDTH + random(10, 54));
}

void seedScene()
{
  randomSeed(0x10A0BA + static_cast<long>(esp_random()));
  gSpriteCursor = static_cast<uint8_t>(random(0, sizeof(kUsableSpriteIndices) / sizeof(kUsableSpriteIndices[0])));

  for (size_t index = 0; index < kBackgroundStarCount; ++index) {
    resetBackgroundStar(index, true);
  }

  for (size_t index = 0; index < kCloudCount; ++index) {
    gClouds[index].spriteIndex = kUsableSpriteIndices[index % (sizeof(kUsableSpriteIndices) / sizeof(kUsableSpriteIndices[0]))];
    resetCloud(index, true);
  }
}

void resetBackgroundStar(size_t index, bool initialPlacement)
{
  BackgroundStar &star = gBackgroundStars[index];
  constexpr int columns = 12;
  constexpr int rows = 7;
  const int slot = static_cast<int>(index);
  const int col = slot % columns;
  const int row = slot / columns;
  const float cellWidth = static_cast<float>(LCD_WIDTH) / static_cast<float>(columns);
  const float cellHeight = static_cast<float>(LCD_HEIGHT - 24) / static_cast<float>(rows);
  const float minX = static_cast<float>(col) * cellWidth;
  const float maxX = min<float>(LCD_WIDTH - 4.0f, minX + cellWidth - 4.0f);
  const float minY = 8.0f + static_cast<float>(row) * cellHeight;
  const float maxY = min<float>(LCD_HEIGHT - 12.0f, minY + cellHeight - 4.0f);
  star.x = initialPlacement
               ? minX + static_cast<float>(random(0, max<int>(1, static_cast<int>(maxX - minX))))
               : static_cast<float>(LCD_WIDTH + random(6, 42));
  star.y = static_cast<int16_t>(minY + static_cast<float>(random(0, max<int>(1, static_cast<int>(maxY - minY)))));
  const int roll = random(0, 100);
  star.radius = static_cast<uint8_t>(roll < 10 ? 2 : 1);
  star.color = random(0, 7) == 0 ? kStarBlue : kStarWhite;
}

uint16_t skyColorForY(int16_t y)
{
  const uint32_t yClamped = static_cast<uint32_t>(max<int16_t>(0, min<int16_t>(LCD_HEIGHT - 1, y)));
  const uint32_t t255 = (yClamped * 255U) / (LCD_HEIGHT - 1);

  if (t255 < 176U) {
    const uint8_t amount = static_cast<uint8_t>((t255 * 255U) / 176U);
    return blend565(kSkyTopNight, kSkyUpperMid, amount);
  }

  if (t255 < 232U) {
    const uint8_t amount = static_cast<uint8_t>(((t255 - 176U) * 255U) / 56U);
    return blend565(kSkyUpperMid, kSkyLowerMid, amount);
  }

  const uint8_t amount = static_cast<uint8_t>(((t255 - 232U) * 255U) / 23U);
  return blend565(kSkyLowerMid, kSkyHorizon, amount);
}

uint16_t skyColorForPosition(int16_t x, int16_t y)
{
  static const int8_t kBayer4x4[4][4] = {
      {-6, 2, -4, 4},
      {6, -2, 4, -4},
      {-3, 5, -5, 3},
      {5, -3, 3, -5},
  };

  const int16_t ySafe = max<int16_t>(0, min<int16_t>(LCD_HEIGHT - 1, y));
  const uint32_t baseMix = (static_cast<uint32_t>(ySafe) * 255U) / (LCD_HEIGHT - 1);
  const uint8_t ditheredMix = clampU8(static_cast<int>(baseMix) + kBayer4x4[ySafe & 3][x & 3]);
  const int16_t ditheredY = static_cast<int16_t>((static_cast<uint32_t>(ditheredMix) * (LCD_HEIGHT - 1)) / 255U);
  return skyColorForY(ditheredY);
}

void drawSkyBackground()
{
  uint16_t *framebuffer = gUseCanvas ? canvas->framebuffer() : nullptr;
  if (framebuffer) {
    for (int16_t y = 0; y < LCD_HEIGHT; ++y) {
      const size_t row = static_cast<size_t>(y) * LCD_WIDTH;
      for (int16_t x = 0; x < LCD_WIDTH; ++x) {
        framebuffer[row + static_cast<size_t>(x)] = skyColorForPosition(x, y);
      }
    }
    return;
  }

  for (int16_t y = 0; y < LCD_HEIGHT; ++y) {
    for (int16_t x = 0; x < LCD_WIDTH; ++x) {
      gfx->drawPixel(x, y, skyColorForPosition(x, y));
    }
  }
}

void drawStars()
{
  for (size_t index = 0; index < kBackgroundStarCount; ++index) {
    const BackgroundStar &star = gBackgroundStars[index];
    const int16_t starX = static_cast<int16_t>(lroundf(star.x));
    if (starX < -2 || starX >= LCD_WIDTH + 2 || star.y < 0 || star.y >= LCD_HEIGHT) {
      continue;
    }

    const uint8_t skyDepth = static_cast<uint8_t>(255U - ((static_cast<uint32_t>(star.y) * 255U) / LCD_HEIGHT));
    const uint8_t intensity = static_cast<uint8_t>(84U + ((static_cast<uint16_t>(skyDepth) * 96U) / 255U));
    const uint16_t starColor = blend565(skyColorForPosition(starX, star.y), star.color, intensity);
    if (star.radius <= 1) {
      scene->drawPixel(starX, star.y, starColor);
    } else {
      scene->fillCircle(starX, star.y, star.radius, starColor);
    }
  }
}

uint16_t cloudTone(uint8_t light4)
{
  const uint8_t midMix = static_cast<uint8_t>(70U + static_cast<uint16_t>(light4) * 10U);
  const uint16_t base = blend565(kCloudShadow, kCloudBody, midMix);
  return blend565(base, kCloudHighlight, static_cast<uint8_t>(light4 * 17U));
}

uint64_t animationTimeUs()
{
  const uint64_t nowUs = esp_timer_get_time();
  if (gAnimationPaused) {
    return gAnimationPausedAtUs - gAnimationStartUs - gAnimationPausedAccumUs;
  }
  return nowUs - gAnimationStartUs - gAnimationPausedAccumUs;
}

float phaseAngleForPeriodUs(uint32_t periodUs)
{
  const uint32_t phaseUs = static_cast<uint32_t>(animationTimeUs() % periodUs);
  return (static_cast<float>(phaseUs) / static_cast<float>(periodUs)) * 2.0f * PI;
}

void toggleAnimationPause()
{
  const uint64_t nowUs = esp_timer_get_time();
  if (gAnimationPaused) {
    if (kAudioDebugMode && gAudioDebugPcmOffset >= kEmbeddedTrackPcmLen) {
      gAudioDebugPcmOffset = 0;
    }
    gAnimationPausedAccumUs += nowUs - gAnimationPausedAtUs;
    gAnimationPaused = false;
  } else {
    gAnimationPausedAtUs = nowUs;
    gAnimationPaused = true;
  }

  // Signal the audio task to skip to a new random track
  gSkipTrack = true;
}

bool initTouch()
{
  pinMode(TP_INT, INPUT_PULLUP);
  if (!gTouchController) {
    return false;
  }

  gTouchReady = gTouchController->begin();
  if (!gTouchReady) {
    Serial.println("Locket: touch controller not found");
    return false;
  }

  gTouchController->IIC_Write_Device_State(
      gTouchController->Arduino_IIC_Touch::Device::TOUCH_POWER_MODE,
      gTouchController->Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_MONITOR);
  gTouchController->IIC_Interrupt_Flag = false;
  gTouchPressed = false;
  gTapTracking = false;
  return true;
}

void serviceTouch()
{
  if (!gTouchReady) {
    return;
  }

  const bool shouldRead = gTouchPressed || digitalRead(TP_INT) == LOW;
  if (!shouldRead) {
    return;
  }

  const bool wasPressed = gTouchPressed;
  const int fingers = static_cast<int>(gTouchController->IIC_Read_Device_Value(
      gTouchController->Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER));
  const bool pressedNow = fingers > 0;

  if (pressedNow) {
    gTouchX = static_cast<int16_t>(gTouchController->IIC_Read_Device_Value(
        gTouchController->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X));
    gTouchY = static_cast<int16_t>(gTouchController->IIC_Read_Device_Value(
        gTouchController->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y));
  }

  if (pressedNow && !wasPressed) {
    gTapTracking = true;
    gTapStartedAtMs = millis();
    gTapStartX = gTouchX;
    gTapStartY = gTouchY;
  } else if (!pressedNow && wasPressed) {
    const int dx = abs(gTouchX - gTapStartX);
    const int dy = abs(gTouchY - gTapStartY);
    const uint32_t heldMs = millis() - gTapStartedAtMs;
    if (gTapTracking &&
        heldMs <= kTapMaxDurationMs &&
        dx <= kTapMaxTravelPx &&
        dy <= kTapMaxTravelPx) {
      toggleAnimationPause();
    }
    gTapTracking = false;
  } else if (pressedNow && gTapTracking) {
    const int dx = abs(gTouchX - gTapStartX);
    const int dy = abs(gTouchY - gTapStartY);
    if (dx > kTapMaxTravelPx || dy > kTapMaxTravelPx) {
      gTapTracking = false;
    }
  }

  gTouchPressed = pressedNow;
  gTouchController->IIC_Interrupt_Flag = false;
}

void drawMoon()
{
  constexpr int16_t orbX = LCD_WIDTH / 2;
  constexpr int16_t orbY = LCD_HEIGHT / 2;
  constexpr uint32_t orbitPeriodUs = 9000000UL;
  constexpr int16_t moonRadius = 182;
  constexpr int16_t cutRadius = moonRadius - 13;
  constexpr int16_t glowRadius = moonRadius + 16;
  constexpr int16_t glowBand = 18;

  const float orbitAngle = -phaseAngleForPeriodUs(orbitPeriodUs);
  const int16_t moonX = orbX;
  const int16_t moonY = orbY;

  const float cutOffset = static_cast<float>(moonRadius) * 0.49f;
  const int16_t cutX = static_cast<int16_t>(roundf(moonX + cosf(orbitAngle) * cutOffset));
  const int16_t cutY = static_cast<int16_t>(roundf(moonY + sinf(orbitAngle) * cutOffset));

  uint16_t *framebuffer = gUseCanvas ? canvas->framebuffer() : nullptr;
  const int32_t moonRadius2 = moonRadius * moonRadius;
  const int32_t cutRadius2 = cutRadius * cutRadius;
  const int32_t glowRadius2 = glowRadius * glowRadius;
  const int32_t innerGlowMin2 = (moonRadius - glowBand) * (moonRadius - glowBand);
  const int32_t innerCutBand2 = (cutRadius - 8) * (cutRadius - 8);

  for (int16_t y = moonY - glowRadius; y <= moonY + glowRadius; ++y) {
    if (y < 0 || y >= LCD_HEIGHT) {
      continue;
    }

    for (int16_t x = moonX - glowRadius; x <= moonX + glowRadius; ++x) {
      if (x < 0 || x >= LCD_WIDTH) {
        continue;
      }

      const int32_t outerDx = x - moonX;
      const int32_t outerDy = y - moonY;
      const int32_t outerDist2 = (outerDx * outerDx) + (outerDy * outerDy);
      if (outerDist2 > glowRadius2) {
        continue;
      }

      const int32_t cutDx = x - cutX;
      const int32_t cutDy = y - cutY;
      const int32_t cutDist2 = (cutDx * cutDx) + (cutDy * cutDy);
      const bool insideCrescent = outerDist2 <= moonRadius2 && cutDist2 >= cutRadius2;

      uint16_t color = 0;
      uint8_t alpha = 0;

      if (insideCrescent) {
        color = kMoonWhite;
        alpha = 255;
      } else if (outerDist2 > moonRadius2 && outerDist2 <= glowRadius2 && cutDist2 >= cutRadius2) {
        color = kMoonGlow;
        alpha = 0; //static_cast<uint8_t>(42 + ((glowRadius2 - outerDist2) * 30LL) / max<int32_t>(1, glowRadius2 - moonRadius2));
      } else if (outerDist2 <= moonRadius2 && outerDist2 >= innerGlowMin2 && cutDist2 >= cutRadius2) {
        color = kMoonGlow;
        alpha = 0; //static_cast<uint8_t>(28 + ((moonRadius2 - outerDist2) * 20LL) / max<int32_t>(1, moonRadius2 - innerGlowMin2));
      } else if (outerDist2 <= moonRadius2 && cutDist2 < cutRadius2 && cutDist2 >= innerCutBand2) {
        color = kMoonGlow;
        alpha = 0; //static_cast<uint8_t>(18 + ((cutDist2 - innerCutBand2) * 24LL) / max<int32_t>(1, cutRadius2 - innerCutBand2));
      } else {
        continue;
      }

      if (framebuffer) {
        const size_t pixelIndex = (static_cast<size_t>(y) * LCD_WIDTH) + static_cast<size_t>(x);
        framebuffer[pixelIndex] = blend565(framebuffer[pixelIndex], color, alpha);
      } else {
        const uint16_t base = skyColorForPosition(x, y);
        gfx->drawPixel(x, y, blend565(base, color, alpha));
      }
    }
  }
}

void drawOrb()
{
  constexpr int16_t orbX = LCD_WIDTH / 2;
  constexpr int16_t orbY = LCD_HEIGHT / 2;
  constexpr int16_t orbRadius = 42;
  constexpr uint32_t spinPeriodUs = 2100000UL;
  constexpr uint16_t kOrbGlowOuter = 0xFD20;
  constexpr uint16_t kOrbGlowInner = 0xFBE0;
  const float spinAngle = -phaseAngleForPeriodUs(spinPeriodUs);

  for (int16_t ring = orbRadius + 12; ring >= orbRadius + 7; --ring) {
    const uint8_t amount = static_cast<uint8_t>(34 + ((orbRadius + 12 - ring) * 9));
    scene->drawCircle(orbX, orbY, ring, blend565(kOrbGlowOuter, kOrbGold, amount));
  }

  scene->drawCircle(orbX, orbY, orbRadius + 5, blend565(kOrbGoldHot, kOrbGold, 190));
  scene->drawCircle(orbX, orbY, orbRadius + 4, kOrbGoldHot);
  scene->drawCircle(orbX, orbY, orbRadius + 3, blend565(kOrbGoldHot, kOrbHighlight, 132));

  for (int16_t ring = orbRadius + 4; ring >= orbRadius + 1; --ring) {
    const uint8_t amount = static_cast<uint8_t>(24 + ((orbRadius + 4 - ring) * 11));
    scene->drawCircle(orbX, orbY, ring, blend565(kOrbGlowInner, kOrbMid, amount));
  }

  for (int16_t radius = orbRadius; radius >= 1; --radius) {
    const uint8_t edgeWeight = static_cast<uint8_t>((static_cast<uint32_t>(radius) * 255U) / orbRadius);
    uint16_t color = blend565(kOrbCore, kOrbMid, edgeWeight);
    color = blend565(kOrbEdge, color, static_cast<uint8_t>(120 + (edgeWeight / 2)));
    scene->fillCircle(orbX, orbY, radius, color);
  }

  const int16_t highlightX = static_cast<int16_t>(roundf(orbX + cosf(spinAngle - 2.15f) * 13.0f));
  const int16_t highlightY = static_cast<int16_t>(roundf(orbY + sinf(spinAngle - 2.15f) * 13.0f));
  const int16_t sparkleX = static_cast<int16_t>(roundf(orbX + cosf(spinAngle - 2.35f) * 20.0f));
  const int16_t sparkleY = static_cast<int16_t>(roundf(orbY + sinf(spinAngle - 2.35f) * 20.0f));

  scene->fillCircle(highlightX, highlightY, 18, blend565(kOrbMid, kOrbHighlight, 178));
  scene->fillCircle(sparkleX, sparkleY, 7, kOrbHighlight);
  scene->drawCircle(orbX, orbY, orbRadius + 1, blend565(kOrbHighlight, kOrbMid, 120));
  scene->drawCircle(orbX, orbY, orbRadius + 2, blend565(kOrbHighlight, kOrbGoldHot, 104));
}

void drawCloud(const Cloud &cloud)
{
  const CloudSprite &sprite = gCloudSprites[cloud.spriteIndex];
  const uint8_t *pixels = sprite.pixels();
  if (!pixels) {
    return;
  }

  const uint16_t sourceWidth = sprite.width();
  const uint16_t sourceHeight = sprite.height();
  if (sourceWidth == 0 || sourceHeight == 0) {
    return;
  }

  const int16_t drawWidth = max<int16_t>(1, cloudDrawWidth(cloud));
  const int16_t drawHeight = max<int16_t>(1, cloudDrawHeight(cloud));
  const int16_t originX = static_cast<int16_t>(cloud.x);
  const int16_t originY = cloud.y;
  uint16_t *framebuffer = gUseCanvas ? canvas->framebuffer() : nullptr;

  for (int16_t dy = 0; dy < drawHeight; ++dy) {
    const int16_t destY = originY + dy;
    if (destY < 0 || destY >= LCD_HEIGHT) {
      continue;
    }

    const uint16_t srcY = static_cast<uint16_t>((static_cast<uint32_t>(dy) * sourceHeight) / drawHeight);
    const size_t srcRow = static_cast<size_t>(srcY) * sourceWidth;
    const size_t destRow = static_cast<size_t>(destY) * LCD_WIDTH;

    for (int16_t dx = 0; dx < drawWidth; ++dx) {
      const int16_t destX = originX + dx;
      if (destX < 0 || destX >= LCD_WIDTH) {
        continue;
      }

      uint16_t srcX = static_cast<uint16_t>((static_cast<uint32_t>(dx) * sourceWidth) / drawWidth);
      if (cloud.flipX) {
        srcX = static_cast<uint16_t>((sourceWidth - 1U) - srcX);
      }

      const uint8_t packed = pixels[srcRow + srcX];
      const uint8_t alpha4 = packed >> 4;
      if (!alpha4) {
        continue;
      }

      uint8_t opacity = static_cast<uint8_t>((static_cast<uint16_t>(alpha4) * 17U * cloud.alphaPct) / 100U);
      opacity = min<uint8_t>(opacity, 235);
      const uint16_t color = cloudTone(packed & 0x0F);

      if (framebuffer) {
        const size_t pixelIndex = destRow + static_cast<size_t>(destX);
        framebuffer[pixelIndex] = blend565(framebuffer[pixelIndex], color, opacity);
      } else {
        gfx->drawPixel(destX, destY, blend565(skyColorForPosition(destX, destY), color, opacity));
      }
    }
  }
}

void renderFrame()
{
  drawSkyBackground();
  drawStars();
  for (size_t index = 0; index < kCloudCount; ++index) {
    drawCloud(gClouds[index]);
  }
  drawMoon();
  drawOrb();
}

void updateStars(float elapsedSeconds)
{
  for (size_t index = 0; index < kBackgroundStarCount; ++index) {
    BackgroundStar &star = gBackgroundStars[index];
    star.x -= kSceneDriftPixelsPerSecond * elapsedSeconds;
    if (star.x < -static_cast<float>(star.radius + 2)) {
      resetBackgroundStar(index, false);
    }
  }
}

void updateClouds(float elapsedSeconds)
{
  for (size_t index = 0; index < kCloudCount; ++index) {
    Cloud &cloud = gClouds[index];
    cloud.x -= cloud.speed * elapsedSeconds * kSceneDriftPixelsPerSecond * 3.0f;
    if (cloud.x < -(cloudDrawWidth(cloud) + 24.0f)) {
      resetCloud(index, false);
    }
  }
}
} // namespace

void setup()
{
  Serial.begin(115200);
  delay(250);
  gAnimationStartUs = esp_timer_get_time();
  gAnimationPausedAtUs = gAnimationStartUs;
  gAnimationPaused = false;

  Wire.begin(IIC_SDA, IIC_SCL);
  Wire.setClock(400000);
  gExpanderReady = initExpanderAndRails();
  initPower();
  initTouch();

  if (!gfx->begin()) {
    Serial.println("Locket: display init failed");
    while (true) {
      delay(1000);
    }
  }

  gfx->setBrightness(255);
  gfx->displayOn();
  gfx->fillScreen(kSkyTopNight);

  canvas = new PsramCanvas(LCD_WIDTH, LCD_HEIGHT, gfx);
  if (canvas && canvas->begin(GFX_SKIP_OUTPUT_BEGIN)) {
    scene = canvas;
    gUseCanvas = true;
  } else {
    scene = gfx;
    gUseCanvas = false;
    Serial.println("Locket: canvas init failed, falling back to direct render");
  }

  initCloudSprites();
  seedScene();
  playBootAudioDiagnostic();
  startAudioPlayback();
  if (!gAudioTaskHandle) {
    xTaskCreatePinnedToCore(audioTaskMain,
                            "locket-audio",
                            4096,
                            nullptr,
                            2,
                            &gAudioTaskHandle,
                            0);
  }
  renderFrame();
  if (gUseCanvas) {
    canvas->flush();
  }
  gLastFrameAtMs = millis();

  Serial.printf("Locket booted. Expander: %s, PMU: %s, canvas: %s, SD sprites: %u/%u\n",
                gExpanderReady ? "OK" : "missing",
                gPowerReady ? "OK" : "missing",
                gUseCanvas ? "ON" : "OFF",
                gSdSpriteCount,
                static_cast<unsigned>(kEmbeddedCloudSpriteCount));
}

void loop()
{
  const uint32_t now = millis();
  serviceSerialTransfer();
  if (gSerialTransferActive) {
    delay(1);
    return;
  }
  serviceTouch();
  serviceSdPresence();
  if (now - gLastFrameAtMs < kFrameIntervalMs) {
    delay(1);
    return;
  }

  const float elapsedSeconds = static_cast<float>(now - gLastFrameAtMs) / 1000.0f;
  gLastFrameAtMs = now;

  if (!gAnimationPaused) {
    updateStars(elapsedSeconds);
    updateClouds(elapsedSeconds);
  }
  renderFrame();
  if (gUseCanvas) {
    canvas->flush();
  }
}
