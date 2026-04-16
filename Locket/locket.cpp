#include <Adafruit_XCA9554.h>
#include <Arduino.h>
#include <Arduino_DriveBus_Library.h>
#include <Arduino_GFX_Library.h>
#include <ArduinoOTA.h>
#include <ESP_I2S.h>
#include <FS.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <WiFiServer.h>
#include <Wire.h>
#include <math.h>
#include <vector>
#include <stdarg.h>

#include "clouds.h"
#include "es8311.h"
#include "es8311_reg.h"
#include "pin_config.h"
#include "star_locket_melody.h"
#include <XPowersLib.h>

namespace
{
constexpr uint8_t kExpanderAddress = 0x20;
constexpr size_t kBackgroundStarCount = 84;
constexpr size_t kCloudCount = 4;
constexpr uint32_t kFrameIntervalMs = 16;
constexpr float kSceneDriftPixelsPerSecond = 22.0f;

constexpr uint16_t kSkyTopNight = 0x0000;  // pure black
constexpr uint16_t kSkyUpperMid = 0x0000;  // pure black
constexpr uint16_t kSkyLowerMid = 0x0000;  // pure black
constexpr uint16_t kSkyHorizon  = 0x0000;  // pure black

constexpr uint16_t kSkyTopDay = 0x2D7F;   // bright blue
constexpr uint16_t kSkyUpperDay = 0x5EBF;
constexpr uint16_t kSkyLowerDay = 0x8F5F;
constexpr uint16_t kSkyHorizonDay = 0xBFFF;

constexpr uint32_t kBatteryPollMs = 10000;
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
constexpr int kAudioVolumeDefault = 75;
constexpr es8311_mic_gain_t kAudioMicGain = static_cast<es8311_mic_gain_t>(3);
constexpr size_t kAudioReadBufferBytes = 2048;
constexpr bool kAudioDebugMode = false;
constexpr size_t kAudioDebugChunkBytes = 8192;
constexpr uint32_t kTapMaxDurationMs = 350;
constexpr int kTapMaxTravelPx = 18;
constexpr size_t kSerialLineBufferBytes = 192;
constexpr size_t kSerialTransferChunkBytes = 512;

constexpr const char *kSdMountPath = "/sdcard";
constexpr const char *kAudioDir = "/locket/audio";
constexpr uint8_t kUsableSpriteIndices[] = {0, 1};
constexpr const uint8_t *kEmbeddedTrackPcm = _star_locket_melody;
constexpr size_t kEmbeddedTrackPcmLen = _star_locket_melody_len;

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

WiFiServer gTelnetServer(23);
WiFiClient gTelnetClient;

void logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char *fmt, ...)
{
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print(buf);
  if (gTelnetClient && gTelnetClient.connected()) {
    gTelnetClient.print(buf);
  }
}

Adafruit_XCA9554 gExpander;
XPowersPMU gPower;
std::shared_ptr<Arduino_IIC_DriveBus> gTouchBus = std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);
std::unique_ptr<Arduino_IIC> gTouchController(new Arduino_FT3x68(gTouchBus, FT3168_DEVICE_ADDRESS, DRIVEBUS_DEFAULT_VALUE, TP_INT, nullptr));
bool gExpanderReady = false;
bool gPowerReady = false;
bool gSdMounted = false;
bool gSdInitFailed = false;
uint32_t gLastSdCheckMs = 0;
constexpr uint32_t kSdCheckIntervalMs = 5000;
uint8_t gSpriteCursor = 0;
uint32_t gLastFrameAtMs = 0;
bool gUseCanvas = false;
bool gAudioReady = false;
bool gTouchReady = false;
bool gTouchPressed = false;
bool gTapTracking = false;
bool gDragActive = false;
int gVolume = kAudioVolumeDefault;
uint32_t gLastVolumeSetMs = 0;
bool gAnimationPaused = true;
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
std::vector<String> gPlaylist;
std::vector<String> gShuffleQueue;
File gAudioFile;
uint32_t gAudioDataRemaining = 0;
volatile bool gSkipTrack = false;
bool gBottomButtonWasPressed = false;
bool gBootButtonWasPressed = false;
constexpr int kBootButtonPin = 0;
uint8_t gBatteryPercent = 100;
uint32_t gLastBatteryPollMs = 0;
uint8_t gSkyPercent = 100;
int8_t gSkyDirection = -1;
uint32_t gLastSkyAnimMs = 0;
constexpr bool kSkyDemoMode = false;
uint8_t gAudioReadBuffer[kAudioReadBufferBytes] = {};
uint16_t gAudioSourceChannels = 0;
uint32_t gAudioSourceSampleRate = 0;
uint32_t gAudioHardwareRate = 0;
bool gUsingEmbeddedTrack = false;
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

bool initSdCard(bool quiet = false);
void startAudioPlayback();
void closeAudioPlayback();
bool initTouch();
void serviceTouch();
void serviceAudioDebug();
bool beginAudioI2s(int sampleRate);
bool reconfigureAudioRate(uint32_t sampleRate);
void audioTaskMain(void *arg);
void resetBackgroundStar(size_t index, bool initialPlacement);

bool initPower()
{
  gPowerReady = gPower.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (!gPowerReady) {
    logf("Locket: PMU not found\n");
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

void cleanDotUnderscoreFiles(const char *dirPath)
{
  File dir = SD_MMC.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    return;
  }
  std::vector<String> toDelete;
  std::vector<String> subdirs;
  while (File entry = dir.openNextFile()) {
    if (entry.isDirectory()) {
      subdirs.push_back(entry.path());
    } else {
      const char *name = entry.name();
      if (name[0] == '.' && name[1] == '_') {
        toDelete.push_back(entry.path());
      }
    }
  }
  for (const auto &path : toDelete) {
    SD_MMC.remove(path.c_str());
    logf("Locket: removed %s\n", path.c_str());
  }
  for (const auto &sub : subdirs) {
    cleanDotUnderscoreFiles(sub.c_str());
  }
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
  gExpander.pinMode(BOTTOM_BUTTON_PIN, INPUT);
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
  logf("Locket: audio amp enable GPIO%d HIGH\n", AUDIO_POWER_AMP);

  if (!beginAudioI2s(kAudioSampleRate)) {
    logf("Locket: I2S begin failed\n");
    return false;
  }

  if (!gAudioCodec) {
    gAudioCodec = es8311_create(0, ES8311_ADDRRES_0);
  }
  if (!gAudioCodec) {
    logf("Locket: audio codec alloc failed\n");
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
  if (err == ESP_OK) err = es8311_voice_volume_set(gAudioCodec, gVolume, nullptr);
  if (err == ESP_OK) err = es8311_microphone_gain_set(gAudioCodec, kAudioMicGain);
  if (err != ESP_OK) {
    logf("Locket: codec init failed: %s\n", esp_err_to_name(err));
    gAudioI2s.end();
    return false;
  }

  gAudioReady = true;
  return true;
}

bool beginAudioI2s(int sampleRate)
{
  gAudioI2s.end();
  gAudioI2s.setPins(AUDIO_I2S_BCLK, AUDIO_I2S_WS, AUDIO_I2S_DOUT, AUDIO_I2S_DIN, AUDIO_I2S_MCLK);
  bool ok = gAudioI2s.begin(I2S_MODE_STD,
                            sampleRate,
                            I2S_DATA_BIT_WIDTH_16BIT,
                            I2S_SLOT_MODE_STEREO,
                            I2S_STD_SLOT_BOTH);
  if (ok) {
    gAudioHardwareRate = sampleRate;
  }
  return ok;
}

bool reconfigureAudioRate(uint32_t sampleRate)
{
  if (gAudioHardwareRate == sampleRate) {
    return true;
  }
  if (!beginAudioI2s(sampleRate)) {
    logf("Locket: I2S reconfigure to %lu failed\n", static_cast<unsigned long>(sampleRate));
    return false;
  }
  const es8311_clock_config_t clk = {
      .mclk_inverted = false,
      .sclk_inverted = false,
      .mclk_from_mclk_pin = true,
      .mclk_frequency = static_cast<int>(sampleRate) * 256,
      .sample_frequency = static_cast<int>(sampleRate),
  };
  esp_err_t err = es8311_sample_frequency_config(gAudioCodec, clk.mclk_frequency, clk.sample_frequency);
  if (err != ESP_OK) {
    logf("Locket: codec rate config failed: %s\n", esp_err_to_name(err));
    return false;
  }
  return true;
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
  logf("Locket: boot audio diagnostic start\n");
  initAudioHardware();
  logf("Locket: boot audio diagnostic end\n");
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

bool isWavPlayable(const char *path)
{
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) return false;

  uint8_t hdr[12];
  if (f.read(hdr, 12) != 12 || memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
    f.close();
    return false;
  }

  while (f.available()) {
    uint8_t ch[8];
    if (f.read(ch, 8) != 8) break;
    uint32_t sz = readLe32(ch + 4);
    if (memcmp(ch, "fmt ", 4) == 0) {
      uint8_t fmt[40];
      size_t toRead = min(static_cast<size_t>(sz), sizeof(fmt));
      if (toRead < 16 || f.read(fmt, toRead) != static_cast<int>(toRead)) { f.close(); return false; }
      uint16_t af = readLe16(fmt + 0);
      uint16_t cn = readLe16(fmt + 2);
      uint32_t sr = readLe32(fmt + 4);
      uint16_t bp = readLe16(fmt + 14);
      if (af == 0xFFFE && toRead >= 40) {
        static const uint8_t kPcm[16] = {0x01,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71};
        if (memcmp(fmt + 24, kPcm, 16) == 0) af = 1;
      }
      f.close();
      return af == 1 && bp == 16 && cn == 2 && sr > 0 && sr <= 48000;
    }
    f.seek(f.position() + sz + (sz & 1U));
  }
  f.close();
  return false;
}

void dumpDir(const char *path, uint8_t depth = 0)
{
  if (depth > 2) {
    return;
  }

  File dir = SD_MMC.open(path);
  if (!dir) {
    logf("Locket: dir open failed %s\n", path);
    return;
  }
  if (!dir.isDirectory()) {
    logf("Locket: not a dir %s\n", path);
    dir.close();
    return;
  }

  logf("Locket: dir %s\n", path);
  File entry = dir.openNextFile();
  while (entry) {
    logf("  %s%s\n", entry.name(), entry.isDirectory() ? "/" : "");
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
  if (!initSdCard(true)) {
    return false;
  }

  if (gAudioFile) {
    gAudioFile.close();
  }
  gAudioDataRemaining = 0;

  // Build playlist on first call or when empty
  if (gPlaylist.empty()) {
    File dir = SD_MMC.open(kAudioDir);
    if (dir && dir.isDirectory()) {
      File entry = dir.openNextFile();
      while (entry) {
        if (!entry.isDirectory()) {
          String name = entry.name();
          String lower = name;
          lower.toLowerCase();
          if (lower.endsWith(".wav")) {
            String fullPath = String(kAudioDir) + "/" + name;
            if (isWavPlayable(fullPath.c_str())) {
              gPlaylist.push_back(fullPath);
            } else {
              logf("Locket: skipping %s (unsupported format)\n", fullPath.c_str());
            }
          }
        }
        entry.close();
        entry = dir.openNextFile();
      }
      dir.close();
    }
    if (gPlaylist.empty()) {
      logf("Locket: no playable WAV found on SD\n");
      dumpDir("/");
      dumpDir("/locket");
      dumpDir("/locket/audio");
      return false;
    }
    logf("Locket: %u playable tracks\n", static_cast<unsigned>(gPlaylist.size()));
  }

  // Shuffle queue: play every track before repeating
  if (gShuffleQueue.empty()) {
    gShuffleQueue = gPlaylist;
    for (size_t i = gShuffleQueue.size() - 1; i > 0; --i) {
      size_t j = esp_random() % (i + 1);
      std::swap(gShuffleQueue[i], gShuffleQueue[j]);
    }
    // Avoid repeating the last song at the boundary
    if (gShuffleQueue.size() > 1 && gActiveAudioPath && gShuffleQueue.back() == gActiveAudioPath) {
      std::swap(gShuffleQueue.back(), gShuffleQueue[0]);
    }
  }
  String chosen = gShuffleQueue.back();
  gShuffleQueue.pop_back();

  static String gSelectedAudioPath;
  gSelectedAudioPath = chosen;
  gActiveAudioPath = gSelectedAudioPath.c_str();

  if (gAudioFile) {
    gAudioFile.close();
  }
  gAudioFile = SD_MMC.open(gActiveAudioPath, FILE_READ);
  if (!gAudioFile) {
    logf("Locket: failed to open WAV %s\n", gActiveAudioPath);
    return false;
  }

  uint8_t riffHeader[12];
  if (gAudioFile.read(riffHeader, sizeof(riffHeader)) != static_cast<int>(sizeof(riffHeader))) {
    logf("Locket: short WAV header\n");
    gAudioFile.close();
    return false;
  }
  if (memcmp(riffHeader, "RIFF", 4) != 0 || memcmp(riffHeader + 8, "WAVE", 4) != 0) {
    logf("Locket: invalid WAV container\n");
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
      uint8_t fmt[40];
      const size_t toRead = min(static_cast<size_t>(chunkSize), sizeof(fmt));
      if (toRead < 16 || gAudioFile.read(fmt, toRead) != static_cast<int>(toRead)) {
        logf("Locket: invalid WAV fmt\n");
        gAudioFile.close();
        return false;
      }
      audioFormat = readLe16(fmt + 0);
      channels = readLe16(fmt + 2);
      sampleRate = readLe32(fmt + 4);
      bitsPerSample = readLe16(fmt + 14);
      if (audioFormat == 0xFFFE && toRead >= 40) {
        // WAVE_FORMAT_EXTENSIBLE: real format is in SubFormat GUID at offset 24.
        // PCM GUID: 00000001-0000-0010-8000-00aa00389b71
        static const uint8_t kPcmGuid[16] = {
          0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
          0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
        };
        if (memcmp(fmt + 24, kPcmGuid, 16) == 0) {
          audioFormat = 1;
        }
      }
      if (chunkSize > toRead) {
        gAudioFile.seek(gAudioFile.position() + (chunkSize - toRead));
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
    logf("Locket: missing WAV chunks\n");
    gAudioFile.close();
    return false;
  }
  if (audioFormat != 1 || bitsPerSample != 16 || channels != 2 || sampleRate == 0 || sampleRate > 48000) {
    logf("Locket: unsupported WAV %s (a=%u ch=%u sr=%lu bits=%u)\n",
                  gActiveAudioPath,
                  static_cast<unsigned>(audioFormat),
                  static_cast<unsigned>(channels),
                  static_cast<unsigned long>(sampleRate),
                  static_cast<unsigned>(bitsPerSample));
    gAudioFile.close();
    return false;
  }

  if (!reconfigureAudioRate(sampleRate)) {
    gAudioFile.close();
    return false;
  }

  gAudioSourceChannels = channels;
  gAudioSourceSampleRate = sampleRate;
  {
    const uint32_t bytesPerSec = sampleRate * channels * (bitsPerSample / 8);
    const uint32_t durationSec = bytesPerSec ? gAudioDataRemaining / bytesPerSec : 0;
    logf("Locket: playing %s (%u-bit %luhz %uch %lu:%02lu)\n",
                  gActiveAudioPath,
                  static_cast<unsigned>(bitsPerSample),
                  static_cast<unsigned long>(sampleRate),
                  static_cast<unsigned>(channels),
                  static_cast<unsigned long>(durationSec / 60),
                  static_cast<unsigned long>(durationSec % 60));
  }
  return true;
}

void closeAudioPlayback()
{
  if (gAudioFile) {
    if (gActiveAudioPath) {
      logf("Locket: finished %s\n", gActiveAudioPath);
    }
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
    gUsingEmbeddedTrack = false;
    closeAudioPlayback();
    openAudioFile();
    if (!gAudioFile) {
      gUsingEmbeddedTrack = true;
      reconfigureAudioRate(kAudioSampleRate);
      gAudioDebugPcmOffset = 0;
    }
    return;
  }

  if (gAnimationPaused) {
    return;
  }

  if (gUsingEmbeddedTrack) {
    const size_t remaining = kEmbeddedTrackPcmLen - gAudioDebugPcmOffset;
    const size_t chunkSize = min<size_t>(kAudioDebugChunkBytes, remaining);
    if (chunkSize == 0) {
      gAnimationPausedAtUs = esp_timer_get_time();
      gAnimationPaused = true;
      return;
    }
    gAudioI2s.write(kEmbeddedTrackPcm + gAudioDebugPcmOffset, chunkSize);
    gAudioDebugPcmOffset += chunkSize;
    if (gAudioDebugPcmOffset >= kEmbeddedTrackPcmLen) {
      gAnimationPausedAtUs = esp_timer_get_time();
      gAnimationPaused = true;
    }
    return;
  }

  if (!gAudioFile) {
    startAudioPlayback();
    if (!gAudioFile) {
      gUsingEmbeddedTrack = true;
      reconfigureAudioRate(kAudioSampleRate);
      gAudioDebugPcmOffset = 0;
    }
    return;
  }

  if (gAudioDataRemaining == 0) {
    closeAudioPlayback();
    openAudioFile();
    if (!gAudioFile) {
      gUsingEmbeddedTrack = true;
      reconfigureAudioRate(kAudioSampleRate);
      gAudioDebugPcmOffset = 0;
    }
    return;
  }

  const size_t toRead = min<size_t>(sizeof(gAudioReadBuffer), gAudioDataRemaining);
  const int bytesRead = gAudioFile.read(gAudioReadBuffer, toRead);
  if (bytesRead <= 0) {
    closeAudioPlayback();
    openAudioFile();
    if (!gAudioFile) {
      gUsingEmbeddedTrack = true;
      reconfigureAudioRate(kAudioSampleRate);
      gAudioDebugPcmOffset = 0;
    }
    return;
  }

  if ((bytesRead & 0x3) != 0 || gAudioSourceChannels != 2) {
    closeAudioPlayback();
    openAudioFile();
    if (!gAudioFile) {
      gUsingEmbeddedTrack = true;
      reconfigureAudioRate(kAudioSampleRate);
      gAudioDebugPcmOffset = 0;
    }
    return;
  }

  const size_t outputBytes = static_cast<size_t>(bytesRead);
  const size_t written = gAudioI2s.write(reinterpret_cast<const uint8_t *>(gAudioReadBuffer), outputBytes);
  if (written != outputBytes) {
    logf("Locket: speaker write failed\n");
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


void finishSerialTransfer(bool success, const char *message)
{
  if (gSerialTransferFile) {
    gSerialTransferFile.flush();
    gSerialTransferFile.close();
  }
  if (success) {
    logf("OK %lu\n", static_cast<unsigned long>(gSerialTransferReceived));
  } else {
    logf("ERR %s\n", message ? message : "transfer");
  }
  gSerialTransferActive = false;
  gSerialTransferExpected = 0;
  gSerialTransferReceived = 0;
}

void handleSerialCommand(const char *line)
{
  if (strcmp(line, "PING") == 0) {
    logf("PONG\n");
    return;
  }

  if (strcmp(line, "STAT") == 0) {
    const bool sdReady = initSdCard();
    logf("STAT SD=%s AUDIO=%s\n", sdReady ? "IN" : "NO", gAudioReady ? "READY" : "OFF");
    return;
  }

  if (strncmp(line, "LS ", 3) == 0) {
    if (!initSdCard()) {
      logf("ERR no-sd\n");
      return;
    }
    const char *path = line + 3;
    File dir = SD_MMC.open(path);
    if (!dir) {
      logf("ERR not-found\n");
      return;
    }
    if (!dir.isDirectory()) {
      logf("F %lu %s\n", static_cast<unsigned long>(dir.size()), dir.name());
      dir.close();
      logf("END\n");
      return;
    }
    File entry = dir.openNextFile();
    while (entry) {
      if (entry.isDirectory()) {
        logf("D %s\n", entry.name());
      } else {
        logf("F %lu %s\n", static_cast<unsigned long>(entry.size()), entry.name());
      }
      entry.close();
      entry = dir.openNextFile();
    }
    dir.close();
    logf("END\n");
    return;
  }

  if (strncmp(line, "RM ", 3) == 0) {
    if (!initSdCard()) {
      logf("ERR no-sd\n");
      return;
    }
    const char *path = line + 3;
    if (SD_MMC.remove(path)) {
      logf("OK\n");
    } else {
      logf("ERR delete-failed\n");
    }
    return;
  }

  if (strncmp(line, "MV ", 3) == 0) {
    if (!initSdCard()) {
      logf("ERR no-sd\n");
      return;
    }
    char src[128], dst[128];
    if (sscanf(line + 3, "%127s %127s", src, dst) != 2) {
      logf("ERR bad-mv\n");
      return;
    }
    if (SD_MMC.rename(src, dst)) {
      logf("OK\n");
    } else {
      logf("ERR rename-failed\n");
    }
    return;
  }

  if (strncmp(line, "PUT ", 4) == 0) {
    if (!initSdCard()) {
      logf("ERR no-sd\n");
      return;
    }

    char path[128];
    unsigned long expected = 0;
    if (sscanf(line + 4, "%127s %lu", path, &expected) != 2 || path[0] != '/' || expected == 0) {
      logf("ERR bad-put\n");
      return;
    }

    if (!ensureParentDirs(path)) {
      logf("ERR mkdir\n");
      return;
    }

    closeAudioPlayback();
    if (gSerialTransferFile) {
      gSerialTransferFile.close();
    }
    SD_MMC.remove(path);
    gSerialTransferFile = SD_MMC.open(path, FILE_WRITE);
    if (!gSerialTransferFile) {
      logf("ERR open\n");
      return;
    }

    gSerialTransferExpected = static_cast<uint32_t>(expected);
    gSerialTransferReceived = 0;
    gSerialTransferActive = true;
    logf("READY %lu\n", static_cast<unsigned long>(gSerialTransferExpected));
    return;
  }

  logf("ERR unknown\n");
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
      logf("ERR line\n");
    }
  }

  // Also accept commands from telnet
  while (gTelnetClient && gTelnetClient.available()) {
    const char ch = static_cast<char>(gTelnetClient.read());
    if (ch == '\r') continue;
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
      logf("ERR line\n");
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

bool initSdCard(bool quiet)
{
  if (gSdMounted) {
    return true;
  }

  if (gSdInitFailed && quiet) {
    return false;
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
      if (!quiet) {
        logf("Locket: SD mount attempt %d failed\n", attempt);
      }
      delay(80);
      continue;
    }

    const uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
      if (!quiet) {
        logf("Locket: SD cardType attempt %d returned none\n", attempt);
      }
      SD_MMC.end();
      delay(80);
      continue;
    }

    gSdMounted = true;
    gSdInitFailed = false;
    logf("Locket: SD mounted: %s\n", cardTypeText(cardType));
    cleanDotUnderscoreFiles("/");
    return true;
  }

  gSdInitFailed = true;
  if (!quiet) {
    logf("Locket: SD unavailable, using embedded clouds\n");
  }
  return false;
}

void recheckSdCard()
{
  const uint32_t now = millis();
  if (now - gLastSdCheckMs < kSdCheckIntervalMs) {
    return;
  }
  gLastSdCheckMs = now;

  if (gSdMounted) {
    // Probe whether the card is still accessible.
    const uint8_t cardType = SD_MMC.cardType();
    if (cardType != CARD_NONE) {
      return;
    }
    gSdMounted = false;
    gPlaylist.clear();
    gShuffleQueue.clear();
    SD_MMC.end();
    logf("Locket: SD card removed\n");
    playSdRemovedChime();
  } else {
    // Single quick probe — no retries, no delays. If it fails we try again in 5s.
    SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
    if (SD_MMC.begin(kSdMountPath, true) && SD_MMC.cardType() != CARD_NONE) {
      gSdMounted = true;
      gSdInitFailed = false;
      logf("Locket: SD card inserted: %s\n", cardTypeText(SD_MMC.cardType()));
      cleanDotUnderscoreFiles("/");
      playSdInsertedChime();
      startAudioPlayback();
    } else {
      SD_MMC.end();
    }
  }
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

  // Night sky gradient
  uint16_t nightColor;
  if (t255 < 176U) {
    const uint8_t amount = static_cast<uint8_t>((t255 * 255U) / 176U);
    nightColor = blend565(kSkyTopNight, kSkyUpperMid, amount);
  } else if (t255 < 232U) {
    const uint8_t amount = static_cast<uint8_t>(((t255 - 176U) * 255U) / 56U);
    nightColor = blend565(kSkyUpperMid, kSkyLowerMid, amount);
  } else {
    const uint8_t amount = static_cast<uint8_t>(((t255 - 232U) * 255U) / 23U);
    nightColor = blend565(kSkyLowerMid, kSkyHorizon, amount);
  }

  if (gSkyPercent == 0) return nightColor;

  // Day sky gradient
  uint16_t dayColor;
  if (t255 < 176U) {
    const uint8_t amount = static_cast<uint8_t>((t255 * 255U) / 176U);
    dayColor = blend565(kSkyTopDay, kSkyUpperDay, amount);
  } else if (t255 < 232U) {
    const uint8_t amount = static_cast<uint8_t>(((t255 - 176U) * 255U) / 56U);
    dayColor = blend565(kSkyUpperDay, kSkyLowerDay, amount);
  } else {
    const uint8_t amount = static_cast<uint8_t>(((t255 - 232U) * 255U) / 23U);
    dayColor = blend565(kSkyLowerDay, kSkyHorizonDay, amount);
  }

  if (gSkyPercent >= 100) return dayColor;

  // Sunset gradient: top goes dark first, bottom stays bright longer.
  // At a given battery%, the top gets a lower dayAmount than the bottom.
  const uint32_t battFrac = static_cast<uint32_t>(gSkyPercent) * 255U / 100U;
  // posBoost: 0 at top, up to 80 at bottom — scaled by battery so it fades to 0 smoothly
  const uint32_t posBoost = (t255 * 80U * gSkyPercent) / (255U * 100U);
  const uint32_t raw = battFrac + posBoost;
  const uint8_t dayAmount = static_cast<uint8_t>(raw > 255U ? 255U : raw);
  return blend565(nightColor, dayColor, dayAmount);
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
  if (gSkyPercent >= 100) return;

  // Scale visible star count: 0% = all stars, 99% = almost none
  const size_t visibleCount = (kBackgroundStarCount * (100U - gSkyPercent)) / 100U;
  // Fade star intensity: dimmer at higher sky percent
  const uint8_t batteryFade = static_cast<uint8_t>((static_cast<uint32_t>(100U - gSkyPercent) * 255U) / 100U);

  for (size_t index = 0; index < visibleCount; ++index) {
    const BackgroundStar &star = gBackgroundStars[index];
    const int16_t starX = static_cast<int16_t>(lroundf(star.x));
    if (starX < -2 || starX >= LCD_WIDTH + 2 || star.y < 0 || star.y >= LCD_HEIGHT) {
      continue;
    }

    const uint8_t skyDepth = static_cast<uint8_t>(255U - ((static_cast<uint32_t>(star.y) * 255U) / LCD_HEIGHT));
    const uint8_t baseIntensity = static_cast<uint8_t>(84U + ((static_cast<uint16_t>(skyDepth) * 96U) / 255U));
    const uint8_t intensity = static_cast<uint8_t>((static_cast<uint16_t>(baseIntensity) * batteryFade) / 255U);
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
  const uint16_t dayColor = blend565(base, kCloudHighlight, static_cast<uint8_t>(light4 * 17U));
  if (gSkyPercent >= 30) return dayColor;
  // Darken clouds only near nighttime (below 30%)
  constexpr uint16_t kCloudNight = 0x2104; // dark grey
  const uint8_t dayAmount = static_cast<uint8_t>(static_cast<uint32_t>(gSkyPercent) * 255U / 30U);
  return blend565(kCloudNight, dayColor, dayAmount);
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
    if (gUsingEmbeddedTrack && gAudioDebugPcmOffset >= kEmbeddedTrackPcmLen) {
      gAudioDebugPcmOffset = 0;
    }
    gAnimationPausedAccumUs += nowUs - gAnimationPausedAtUs;
    gAnimationPaused = false;
    gLastSkyAnimMs = millis();
    if (gActiveAudioPath) {
      logf("Locket: resumed %s\n", gActiveAudioPath);
    }
  } else {
    gAnimationPausedAtUs = nowUs;
    gAnimationPaused = true;
    if (gActiveAudioPath) {
      logf("Locket: paused %s\n", gActiveAudioPath);
    }
  }

}

bool initTouch()
{
  pinMode(TP_INT, INPUT_PULLUP);
  if (!gTouchController) {
    return false;
  }

  gTouchReady = gTouchController->begin();
  if (!gTouchReady) {
    logf("Locket: touch controller not found\n");
    return false;
  }

  gTouchController->IIC_Write_Device_State(
    gTouchController->Arduino_IIC_Touch::Device::TOUCH_POWER_MODE,
    gTouchController->Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_ACTIVE
  );
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
  const int fingers = static_cast<int>(gTouchController->IIC_Read_Device_Value(gTouchController->Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER));
  const bool pressedNow = fingers > 0;

  if (pressedNow) {
    gTouchX = static_cast<int16_t>(gTouchController->IIC_Read_Device_Value( gTouchController->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X));
    gTouchY = static_cast<int16_t>(gTouchController->IIC_Read_Device_Value( gTouchController->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y));
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
    if (gTapTracking && heldMs <= kTapMaxDurationMs && dx <= kTapMaxTravelPx && dy <= kTapMaxTravelPx) {
      toggleAnimationPause();
    }
    gTapTracking = false;
  } else if (pressedNow && gTapTracking) {
    const int dx = abs(gTouchX - gTapStartX);
    const int dy = abs(gTouchY - gTapStartY);
    if (dx > kTapMaxTravelPx || dy > kTapMaxTravelPx) {
      gTapTracking = false;
      if (dy > dx) {
        gDragActive = true;
      }
    }
  }

  // Volume drag: finger position maps directly to volume (top = 100%, bottom = 0%)
  if (pressedNow && gDragActive) {
    int rawVol = constrain(100 - (gTouchY * 100 / LCD_HEIGHT), 0, 100);
    int newVol = (rawVol / 5) * 5;
    if (newVol != gVolume) {
      gVolume = newVol;
      const uint32_t now_ms = millis();
      if (gAudioCodec && now_ms - gLastVolumeSetMs >= 50) {
        gLastVolumeSetMs = now_ms;
        es8311_voice_volume_set(gAudioCodec, gVolume, nullptr);
      }
      logf("Locket: volume %d%% (y=%d)\n", gVolume, gTouchY);
    }
  }

  // Apply final volume on release in case the rate limit skipped it
  if (!pressedNow && gDragActive && gAudioCodec) {
    es8311_voice_volume_set(gAudioCodec, gVolume, nullptr);
  }

  if (!pressedNow) {
    gDragActive = false;
  }

  gTouchPressed = pressedNow;
  gTouchController->IIC_Interrupt_Flag = false;
}

void drawMoon()
{
  // --- Size & placement ---
  constexpr int16_t moonX = LCD_WIDTH / 2;           // x coordinate of center of the moon
  constexpr int16_t moonY = LCD_HEIGHT / 2;          // y coordinate of center of the moon
  constexpr int16_t moonRadius = 177;                // outer radius of the moon
  constexpr int16_t cutRadius = moonRadius - 20;     // radius of the circle that subtracts from the moon to form the crescent
  constexpr float cutOffsetFactor = 0.49f;           // how far the cut center sits from the moon center (fraction of moonRadius)
  constexpr uint32_t orbitPeriodUs = 9000000UL;      // full rotation period of the crescent phase (9 s)

  const float orbitAngle = -phaseAngleForPeriodUs(orbitPeriodUs);                           // current rotation angle of the cut circle
  const float cutOffset = static_cast<float>(moonRadius) * cutOffsetFactor;                 // pixel distance from moon center to cut center
  const int16_t cutX = static_cast<int16_t>(roundf(moonX + cosf(orbitAngle) * cutOffset));  // x position of cut center
  const int16_t cutY = static_cast<int16_t>(roundf(moonY + sinf(orbitAngle) * cutOffset));  // y position of cut center

  uint16_t *framebuffer = gUseCanvas ? canvas->framebuffer() : nullptr;
  const int32_t moonRadiusSq = moonRadius * moonRadius;   // squared radii for distance checks (avoids sqrt)
  const int32_t cutRadiusSq = cutRadius * cutRadius;

  for (int16_t y = moonY - moonRadius; y <= moonY + moonRadius; ++y) {
    if (y < 0 || y >= LCD_HEIGHT) continue;

    for (int16_t x = moonX - moonRadius; x <= moonX + moonRadius; ++x) {
      if (x < 0 || x >= LCD_WIDTH) continue;

      const int32_t outerDx = x - moonX;                                      // x offset from moon center
      const int32_t outerDy = y - moonY;                                      // y offset from moon center
      const int32_t outerDistSq = (outerDx * outerDx) + (outerDy * outerDy);  // squared distance to moon center
      if (outerDistSq > moonRadiusSq) continue;                               // outside the moon: skip

      const int32_t cutDx = x - cutX;                               // x offset from cut center
      const int32_t cutDy = y - cutY;                               // y offset from cut center
      const int32_t cutDistSq = (cutDx * cutDx) + (cutDy * cutDy);  // squared distance to cut center
      if (cutDistSq < cutRadiusSq) continue;                        // inside the cut circle: skip

      // pixel is inside the moon but outside the cut circle: draw crescent
      if (framebuffer) {
        const size_t pixelIndex = (static_cast<size_t>(y) * LCD_WIDTH) + static_cast<size_t>(x);
        framebuffer[pixelIndex] = blend565(framebuffer[pixelIndex], kMoonWhite, 255);
      } else {
        const uint16_t base = skyColorForPosition(x, y);
        gfx->drawPixel(x, y, blend565(base, kMoonWhite, 255));
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

  WiFi.begin("The Y-Files", "quartz21wrench10crown");
  uint32_t wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000) {
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED) {
    logf("Locket: WiFi connected: %s\n", WiFi.localIP().toString().c_str());
    ArduinoOTA.setHostname("locket");
    ArduinoOTA.onStart([]() {
      closeAudioPlayback();
      gAnimationPaused = true;
      gfx->fillScreen(0x0000);
      gfx->setTextColor(0xFFFF);
      gfx->setTextSize(2);
      gfx->setCursor(LCD_WIDTH / 2 - 60, LCD_HEIGHT / 2 - 30);
      gfx->print("Updating...");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      const int pct = static_cast<int>(progress * 100 / total);
      const int barW = LCD_WIDTH - 80;
      const int barH = 16;
      const int barX = 40;
      const int barY = LCD_HEIGHT / 2;
      const int fillW = pct * barW / 100;
      gfx->fillRect(barX, barY, barW, barH, 0x0000);
      gfx->drawRect(barX, barY, barW, barH, 0xFFFF);
      gfx->fillRect(barX + 1, barY + 1, fillW - 2, barH - 2, 0xFFFF);
    });
    ArduinoOTA.onEnd([]() {
      gfx->fillScreen(0x0000);
      gfx->setCursor(LCD_WIDTH / 2 - 48, LCD_HEIGHT / 2 - 10);
      gfx->setTextColor(0xFFFF);
      gfx->setTextSize(2);
      gfx->print("Rebooting");
    });
    ArduinoOTA.onError([](ota_error_t error) {
      gfx->fillScreen(0x0000);
      gfx->setCursor(LCD_WIDTH / 2 - 60, LCD_HEIGHT / 2 - 10);
      gfx->setTextColor(0xF800);
      gfx->setTextSize(2);
      gfx->print("OTA Failed");
    });
    ArduinoOTA.begin();
    gTelnetServer.begin();
    gTelnetServer.setNoDelay(true);
    logf("Locket: OTA ready, telnet on port 23\n");
  } else {
    logf("Locket: WiFi not available, OTA disabled\n");
  }
  gAnimationStartUs = esp_timer_get_time();
  gAnimationPausedAtUs = gAnimationStartUs;
  gAnimationPaused = false;

  pinMode(kBootButtonPin, INPUT_PULLUP);

  Wire.begin(IIC_SDA, IIC_SCL);
  Wire.setClock(400000);
  gExpanderReady = initExpanderAndRails();
  initPower();
  initTouch();

  if (!gfx->begin()) {
    logf("Locket: display init failed\n");
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
    logf("Locket: canvas init failed, falling back to direct render\n");
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

  logf("Locket booted. Expander: %s, PMU: %s, canvas: %s\n",
                gExpanderReady ? "OK" : "missing",
                gPowerReady ? "OK" : "missing",
                gUseCanvas ? "ON" : "OFF");
}

void loop()
{
  const uint32_t now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
    if (gTelnetServer.hasClient()) {
      if (gTelnetClient && gTelnetClient.connected()) {
        gTelnetClient.stop();
      }
      gTelnetClient = gTelnetServer.accept();
      logf("Locket: telnet client connected\n");
    }
  }
  serviceSerialTransfer();
  if (gSerialTransferActive) {
    delay(1);
    return;
  }
  recheckSdCard();
  if (gPowerReady && now - gLastBatteryPollMs >= kBatteryPollMs) {
    gLastBatteryPollMs = now;
    int pct = gPower.getBatteryPercent();
    uint8_t newPct = static_cast<uint8_t>(constrain(pct, 0, 100));
    if (newPct != gBatteryPercent) {
      logf("Locket: battery %u%%\n", static_cast<unsigned>(newPct));
      gBatteryPercent = newPct;
    }
  }
  if (kSkyDemoMode) {
    if (!gAnimationPaused && now - gLastSkyAnimMs >= 50) {
      gLastSkyAnimMs = now;
      if (gSkyPercent == 0) gSkyDirection = 1;
      else if (gSkyPercent == 100) gSkyDirection = -1;
      gSkyPercent += gSkyDirection;
    }
  } else {
    gSkyPercent = gBatteryPercent;
  }
  serviceTouch();
  if (gExpanderReady) {
    bool pressed = gExpander.digitalRead(BOTTOM_BUTTON_PIN) == LOW;
    if (pressed && !gBottomButtonWasPressed) {
      toggleAnimationPause();
    }
    gBottomButtonWasPressed = pressed;
  }
  if (gPowerReady) {
    gPower.getIrqStatus();
    if (gPower.isPekeyShortPressIrq()) {
      toggleAnimationPause();
    }
    gPower.clearIrqStatus();
  }
  {
    bool pressed = digitalRead(kBootButtonPin) == LOW;
    if (pressed && !gBootButtonWasPressed) {
      gSkipTrack = true;
    }
    gBootButtonWasPressed = pressed;
  }
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
