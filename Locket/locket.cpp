#include <Adafruit_XCA9554.h>
#include <Arduino.h>
#include <Arduino_DriveBus_Library.h>
#include <Arduino_GFX_Library.h>
#include <ArduinoOTA.h>
#include <ESP_I2S.h>
#include <FS.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiProv.h>
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
#include "usb_msc.h"
#include <XPowersLib.h>

namespace
{
// ---------------------------------------------------------------------------
// Feature toggles — flip to false to disable a feature at compile time.
// ---------------------------------------------------------------------------
constexpr bool kFeatureCloudsMoving   = true;  // animate cloud drift across sky
constexpr bool kFeatureMoonOrbSpin    = true;  // rotate crescent phase + orb highlight
constexpr bool kFeatureMusic          = true;  // play background audio (SD + embedded)
constexpr bool kFeatureStarsDrifting  = true;  // slow background star parallax
constexpr bool kFeatureShootingStars  = true;  // charging-triggered meteor streaks
constexpr bool kFeatureScrollImages   = true;  // SD-loaded image overlay
// ---------------------------------------------------------------------------

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
constexpr uint16_t kMoonHot = 0xFFDE;            // bright hot yellow-white (active)
constexpr uint16_t kMoonGold = 0xFCC0;           // gold amber (paused)
constexpr uint16_t kMoonBorder = 0x0000;         // black outline (paused)
constexpr uint16_t kSkyPinkTop = 0xFB57;         // bright pink top
constexpr uint16_t kSkyPinkMid = 0xFC9F;         // light pink mid
constexpr uint16_t kSkyPinkLow = 0xFDBF;         // pale pink lower
constexpr uint16_t kSkyPinkHorizon = 0xFEFF;     // very pale pink horizon
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
constexpr const char *kImageDir = "/locket/image";
constexpr int kScrollImageWidth = 300;
constexpr int kScrollImageHeight = 300;
constexpr size_t kScrollImageBytes = kScrollImageWidth * kScrollImageHeight * 2;
constexpr uint32_t kImageScrollDurationMs = 12000;
constexpr bool kZoetropeEnabled = false;
constexpr uint32_t kImageGapMinMs = 1500;
constexpr uint32_t kImageGapMaxMs = 4000;
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

struct ShootingStar
{
  bool active = false;
  float x = 0.0f;
  float y = 0.0f;
  float vx = 0.0f;
  float vy = 0.0f;
  float lifeSec = 0.0f;
  float maxLifeSec = 1.0f;
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
int gDragActive = 0;  // 0 = none, 1 = orb horizontal tug, 2 = orb vertical tug
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

// Orb tug: drag starting on the orb in a cardinal direction triggers an action.
bool gOrbTugActive = false;
volatile bool gRestartTrackRequested = false;       // left tug → restart current song
int gOrbOffsetX = 0;                                // visible "pull" offset while tug is active
int gOrbOffsetY = 0;
uint32_t gVolTugLastMs = 0;                         // last touch-service tick while vertical tug active
int32_t  gVolTugAccumQ8 = 0;                        // Q8 fractional-volume accumulator (rate × time)
// std::vector<String> gPlayedHistory;  // (unused) previous-track history; kept for reference

BackgroundStar gBackgroundStars[kBackgroundStarCount];
CloudSprite gCloudSprites[kEmbeddedCloudSpriteCount];
Cloud gClouds[kCloudCount];
es8311_handle_t gAudioCodec = nullptr;
I2SClass gAudioI2s;
const char *gActiveAudioPath = nullptr;
std::vector<String> gPlaylist;
std::vector<String> gImageList;
uint16_t *gImageBuffer = nullptr;
uint8_t *gImageAlphaMask = nullptr;  // precomputed vignette
bool gImageActive = false;
uint32_t gImageStartedMs = 0;
uint32_t gImageNextSpawnMs = 0;
String gImageActivePath;
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
bool gPauseTransitionActive = false;
bool gLastPausedState = true;  // matches gAnimationPaused default
uint32_t gPauseTransitionStartMs = 0;
constexpr uint32_t kPauseTransitionDurationMs = 900;
// Moon uses a uniform crossfade — the whole crescent glows up/down together.
constexpr uint32_t kMoonTransitionDurationMs = 1000;


constexpr size_t kShootingStarCount = 16;
ShootingStar gShootingStars[kShootingStarCount];
bool gIsPluggedIn = false;         // VBUS present (USB connected) — gates shooting stars
bool gIsActivelyCharging = false;  // battery actually drawing charge — triggers lightning

// Lightning strike: a jagged polyline flashed briefly while the battery is charging.
// Points are the vertices of the main bolt; one optional branch forks off mid-bolt.
constexpr size_t kLightningMainPoints = 18;
constexpr size_t kLightningBranchPoints = 6;
struct Lightning {
  bool active = false;
  uint32_t startMs = 0;
  uint32_t durationMs = 0;
  int16_t mainX[kLightningMainPoints];
  int16_t mainY[kLightningMainPoints];
  uint8_t mainCount = 0;
  int16_t branchX[kLightningBranchPoints];
  int16_t branchY[kLightningBranchPoints];
  uint8_t branchCount = 0;
};
Lightning gLightning;
uint32_t gLightningNextMinMs = 0;  // soonest time we'll try to strike again
uint32_t gLastChargeCheckMs = 0;
uint32_t gLastShootingSpawnMs = 0;
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
void updateLightning(uint32_t nowMs);
void drawLightning();

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

void buildImageAlphaMask();          // forward decl
float phaseAngleForPeriodUs(uint32_t periodUs);  // forward decl

void scanImageList()
{
  gImageList.clear();
  File dir = SD_MMC.open(kImageDir);
  if (!dir || !dir.isDirectory()) return;
  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String name = entry.name();
      if (name.endsWith(".bin") && !name.startsWith("._")) {
        gImageList.push_back(String(kImageDir) + "/" + name);
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
  logf("Locket: %u scroll images found\n", static_cast<unsigned>(gImageList.size()));
  buildImageAlphaMask();
}

bool loadScrollImage(const char *path)
{
  if (!gImageBuffer) {
    gImageBuffer = static_cast<uint16_t *>(ps_malloc(kScrollImageBytes));
    if (!gImageBuffer) {
      logf("Locket: image buffer alloc failed\n");
      return false;
    }
  }
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    logf("Locket: image open failed %s\n", path);
    return false;
  }
  size_t total = 0;
  while (total < kScrollImageBytes) {
    int got = f.read(reinterpret_cast<uint8_t *>(gImageBuffer) + total, kScrollImageBytes - total);
    if (got <= 0) break;
    total += got;
  }
  f.close();
  if (total != kScrollImageBytes) {
    logf("Locket: image short read %u/%u\n", static_cast<unsigned>(total), static_cast<unsigned>(kScrollImageBytes));
    return false;
  }
  return true;
}

void buildImageAlphaMask()
{
  if (gImageAlphaMask) return;
  gImageAlphaMask = static_cast<uint8_t *>(ps_malloc(kScrollImageWidth * kScrollImageHeight));
  if (!gImageAlphaMask) {
    logf("Locket: alpha mask alloc failed\n");
    return;
  }
  const float halfW = kScrollImageWidth * 0.5f;
  const float halfH = kScrollImageHeight * 0.5f;
  constexpr float fadeStart = 0.55f;
  constexpr float fadeEnd = 1.0f;
  for (int iy = 0; iy < kScrollImageHeight; ++iy) {
    for (int ix = 0; ix < kScrollImageWidth; ++ix) {
      const float dx = (ix - halfW) / halfW;
      const float dy = (iy - halfH) / halfH;
      const float d = sqrtf(dx * dx + dy * dy);
      float alpha = 1.0f;
      if (d > fadeStart) {
        alpha = 1.0f - (d - fadeStart) / (fadeEnd - fadeStart);
        if (alpha < 0.0f) alpha = 0.0f;
      }
      gImageAlphaMask[iy * kScrollImageWidth + ix] = static_cast<uint8_t>(alpha * 255.0f);
    }
  }
}

void drawScrollImage()
{
  if (!kZoetropeEnabled) return;
  if (!gImageActive || !gImageBuffer || !gImageAlphaMask) return;
  const uint32_t elapsed = millis() - gImageStartedMs;
  if (elapsed >= kImageScrollDurationMs) {
    gImageActive = false;
    gImageNextSpawnMs = millis() + kImageGapMinMs + (esp_random() % (kImageGapMaxMs - kImageGapMinMs));
    return;
  }

  // Image is fixed centered; the spotlight sweeps left-to-right across it.
  const float progress = static_cast<float>(elapsed) / static_cast<float>(kImageScrollDurationMs);
  const int32_t imgX = (LCD_WIDTH - kScrollImageWidth) / 2;
  const int32_t imgY = (LCD_HEIGHT - kScrollImageHeight) / 2;

  // Spotlight: vertical column with soft Gaussian falloff. Sweeps off-left to off-right
  // across the IMAGE's local x range so the spot enters and exits the image cleanly.
  constexpr float kSpotlightHalfWidthPx = 60.0f;  // half-width of fully-bright zone falloff
  const float spotCenterX = -kSpotlightHalfWidthPx +
                            progress * (kScrollImageWidth + 2.0f * kSpotlightHalfWidthPx);

  uint16_t *fb = gUseCanvas ? canvas->framebuffer() : nullptr;
  if (!fb) return;

  // Clamp iteration to visible region (the whole image is on screen, but be safe)
  const int ixStart = max<int32_t>(0, -imgX);
  const int ixEnd = min<int32_t>(kScrollImageWidth, LCD_WIDTH - imgX);
  const int iyStart = max<int32_t>(0, -imgY);
  const int iyEnd = min<int32_t>(kScrollImageHeight, LCD_HEIGHT - imgY);

  for (int iy = iyStart; iy < iyEnd; ++iy) {
    const int sy = imgY + iy;
    const size_t rowBase = iy * kScrollImageWidth;
    const size_t dstRowBase = sy * LCD_WIDTH;
    for (int ix = ixStart; ix < ixEnd; ++ix) {
      const uint8_t maskAmt = gImageAlphaMask[rowBase + ix];
      if (maskAmt == 0) continue;

      // Spotlight intensity at this column: 1.0 at center, falling off to 0 at half-width
      const float dx = static_cast<float>(ix) - spotCenterX;
      const float dn = dx / kSpotlightHalfWidthPx;
      float spot = 1.0f - (dn * dn);  // quadratic falloff
      if (spot < 0.0f) continue;
      if (spot > 1.0f) spot = 1.0f;

      // Combine vignette + spotlight
      const uint8_t amt = static_cast<uint8_t>(maskAmt * spot);
      if (amt == 0) continue;

      const uint16_t raw = gImageBuffer[rowBase + ix];
      const uint16_t pixel = (raw >> 8) | (raw << 8);
      const size_t dstIdx = dstRowBase + (imgX + ix);
      fb[dstIdx] = (amt == 255) ? pixel : blend565(fb[dstIdx], pixel, amt);
    }
  }
}

void updateScrollImage()
{
  if (!kFeatureScrollImages || !kZoetropeEnabled) {
    gImageActive = false;
    return;
  }
  if (gAnimationPaused) {
    gImageActive = false;
    return;
  }
  if (!gImageActive) {
    const uint32_t now = millis();
    if (gImageList.empty()) return;
    if (now < gImageNextSpawnMs) return;
    // Pick a random image and start scrolling
    const size_t idx = esp_random() % gImageList.size();
    gImageActivePath = gImageList[idx];
    if (loadScrollImage(gImageActivePath.c_str())) {
      gImageActive = true;
      gImageStartedMs = now;
      logf("Locket: scrolling %s\n", gImageActivePath.c_str());
    } else {
      gImageNextSpawnMs = now + 2000;
    }
  }
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

  // (Previous-track history — unused; left tug restarts the current song instead.)
  // gPlayedHistory.push_back(chosen);
  // if (gPlayedHistory.size() > 16) gPlayedHistory.erase(gPlayedHistory.begin());

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
  if (!kFeatureMusic) {
    return;
  }
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
  if (!kFeatureMusic) {
    return;
  }
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

  if (gRestartTrackRequested) {
    gRestartTrackRequested = false;
    // Restart the current track: push its path back onto the top of the shuffle
    // queue so the next openAudioFile() re-picks it.
    if (gActiveAudioPath) {
      gShuffleQueue.push_back(String(gActiveAudioPath));
    }
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

  // (Previous-track via history — kept for reference)
  // if (gPrevTrackRequested) {
  //   gPrevTrackRequested = false;
  //   if (gPlayedHistory.size() >= 2) {
  //     const String current = gPlayedHistory.back();
  //     gPlayedHistory.pop_back();
  //     const String prev = gPlayedHistory.back();
  //     gPlayedHistory.pop_back();
  //     gShuffleQueue.push_back(current);
  //     gShuffleQueue.push_back(prev);
  //   }
  //   gUsingEmbeddedTrack = false;
  //   closeAudioPlayback();
  //   openAudioFile();
  //   if (!gAudioFile) {
  //     gUsingEmbeddedTrack = true;
  //     reconfigureAudioRate(kAudioSampleRate);
  //     gAudioDebugPcmOffset = 0;
  //   }
  //   return;
  // }

  if (gAnimationPaused) {
    return;
  }

  if (gUsingEmbeddedTrack) {
    const size_t remaining = kEmbeddedTrackPcmLen - gAudioDebugPcmOffset;
    const size_t chunkSize = min<size_t>(kAudioDebugChunkBytes, remaining);
    if (chunkSize > 0) {
      gAudioI2s.write(kEmbeddedTrackPcm + gAudioDebugPcmOffset, chunkSize);
      gAudioDebugPcmOffset += chunkSize;
    }
    if (gAudioDebugPcmOffset >= kEmbeddedTrackPcmLen) {
      // End of the built-in song — re-check the SD card. If it's available now, hand
      // playback off to a real track. If still missing, pause as before.
      logf("Locket: embedded song ended, re-checking SD card\n");
      if (initSdCard(true)) {
        gUsingEmbeddedTrack = false;
        closeAudioPlayback();
        openAudioFile();
        if (gAudioFile) {
          logf("Locket: SD card detected, switching to playlist\n");
          return;
        }
        // open failed → fall back to embedded mode
        gUsingEmbeddedTrack = true;
        reconfigureAudioRate(kAudioSampleRate);
        gAudioDebugPcmOffset = 0;
      }
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
    scanImageList();
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
    setMscMediaPresent(false);
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
      setMscMediaPresent(true);
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

// Returns 0..255 blend amount for new-state color at column x.
// Right-to-left wipe: right column reaches new state first.
// 0 = old state, 255 = new state.
uint8_t columnTransitionAmount(int16_t x)
{
  if (!gPauseTransitionActive) return 255;
  const uint32_t elapsed = millis() - gPauseTransitionStartMs;
  if (elapsed >= kPauseTransitionDurationMs) {
    return 255;
  }
  const float progress = static_cast<float>(elapsed) / static_cast<float>(kPauseTransitionDurationMs);
  const float normX = static_cast<float>(x) / static_cast<float>(LCD_WIDTH);
  constexpr float feather = 0.25f;
  // Threshold sweeps from 1+feather (right) to -feather (off left)
  const float threshold = (1.0f + feather) - progress * (1.0f + 2.0f * feather);
  const float raw = (normX - threshold) / feather + 1.0f;
  const float clamped = raw < 0.0f ? 0.0f : (raw > 1.0f ? 1.0f : raw);
  return static_cast<uint8_t>(clamped * 255.0f);
}

// Moon transition: uniform crossfade over kMoonTransitionDurationMs — every pixel
// of the crescent glows up (or down) together, no spatial sweep.
// 0 = old state, 255 = new state.
uint8_t moonTransitionAmount(int16_t x, int16_t y, int16_t moonX, int16_t moonY)
{
  if (!gPauseTransitionActive) return 255;
  const uint32_t elapsed = millis() - gPauseTransitionStartMs;
  if (elapsed >= kMoonTransitionDurationMs) return 255;
  return static_cast<uint8_t>((elapsed * 255U) / kMoonTransitionDurationMs);

#if 0  // previous angular sweep — kept for reference
  const float dx = static_cast<float>(x - moonX);
  const float dy = static_cast<float>(y - moonY);
  if (dx == 0.0f && dy == 0.0f) return 255;

  // Convert pixel's screen angle to the crescent's LOCAL angular frame by subtracting
  // the current orbit angle.  This way the sweep tracks the crescent as it spins —
  // no shimmering caused by screen-fixed angles passing in and out of the crescent shape.
  constexpr uint32_t orbitPeriodUs = 9000000UL;
  const float currentOrbitAngle = kFeatureMoonOrbSpin ? -phaseAngleForPeriodUs(orbitPeriodUs) : 0.0f;
  const float screenAngle = atan2f(dy, dx);
  float localAngle = screenAngle - currentOrbitAngle;
  while (localAngle < 0.0f) localAngle += 2.0f * static_cast<float>(M_PI);
  while (localAngle >= 2.0f * static_cast<float>(M_PI)) localAngle -= 2.0f * static_cast<float>(M_PI);

  // Tip angle (local frame) — geometric intersection of outer moon circle and cut circle.
  // Same constants as drawMoon().
  static const float kTipAngleLocal = []() -> float {
    constexpr float moonRadius = 177.0f;
    constexpr float cutRadius  = moonRadius - 20.0f;
    constexpr float cutOffset  = moonRadius * 0.49f;
    const float a = (moonRadius * moonRadius - cutRadius * cutRadius + cutOffset * cutOffset)
                    / (2.0f * cutOffset);
    const float h = sqrtf(moonRadius * moonRadius - a * a);
    return atan2f(h, a);  // ≈62°
  }();
  // Crescent spans from +tip, CCW through the belly (local π), to -tip (=2π-tip).
  // Arc distance from the nearest tip along that body: 0 at either tip, max at belly.
  const float crescentHalfSpan = static_cast<float>(M_PI) - kTipAngleLocal;  // ≈118°

  // β: signed angular offset from the belly (local π). Body covers |β| ≤ crescentHalfSpan.
  float beta = localAngle - static_cast<float>(M_PI);
  while (beta >  static_cast<float>(M_PI)) beta -= 2.0f * static_cast<float>(M_PI);
  while (beta < -static_cast<float>(M_PI)) beta += 2.0f * static_cast<float>(M_PI);
  float distFromTip = crescentHalfSpan - fabsf(beta);  // 0 at tips, ~118° at belly
  if (distFromTip < 0.0f) distFromTip = 0.0f;          // gap-side halo tracks the tips

  // Sweep grows so both tips flip first and the belly completes exactly at the transition end.
  const float omega = crescentHalfSpan / (static_cast<float>(kMoonTransitionDurationMs) / 1000.0f);
  const float sweptRad = (static_cast<float>(elapsed) / 1000.0f) * omega;

  constexpr float feather = 0.8f;
  if (distFromTip <= sweptRad) return 255;
  if (distFromTip - sweptRad < feather) {
    const float t = 1.0f - (distFromTip - sweptRad) / feather;
    return static_cast<uint8_t>(t * 255.0f);
  }
  return 0;
#endif
}

uint16_t pinkSkyColorForY(int16_t y)
{
  const uint32_t yClamped = static_cast<uint32_t>(max<int16_t>(0, min<int16_t>(LCD_HEIGHT - 1, y)));
  const uint32_t t255 = (yClamped * 255U) / (LCD_HEIGHT - 1);
  if (t255 < 128U) {
    const uint8_t amt = static_cast<uint8_t>((t255 * 255U) / 128U);
    return blend565(kSkyPinkTop, kSkyPinkMid, amt);
  }
  if (t255 < 200U) {
    const uint8_t amt = static_cast<uint8_t>(((t255 - 128U) * 255U) / 72U);
    return blend565(kSkyPinkMid, kSkyPinkLow, amt);
  }
  const uint8_t amt = static_cast<uint8_t>(((t255 - 200U) * 255U) / 55U);
  return blend565(kSkyPinkLow, kSkyPinkHorizon, amt);
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

static const int8_t kBayer4x4[4][4] = {
    {-6, 2, -4, 4},
    {6, -2, 4, -4},
    {-3, 5, -5, 3},
    {5, -3, 3, -5},
};

// Sky LUT: per-row, 4 dithered colors (indexed by x & 3). Rebuilt when gSkyPercent changes.
uint16_t gSkyLUT[LCD_HEIGHT][4];
uint8_t gSkyLUTPercent = 255;  // sentinel so first frame rebuilds

void rebuildSkyLUT()
{
  for (int16_t y = 0; y < LCD_HEIGHT; ++y) {
    const uint32_t baseMix = (static_cast<uint32_t>(y) * 255U) / (LCD_HEIGHT - 1);
    for (int xm = 0; xm < 4; ++xm) {
      const uint8_t ditheredMix = clampU8(static_cast<int>(baseMix) + kBayer4x4[y & 3][xm]);
      const int16_t ditheredY = static_cast<int16_t>((static_cast<uint32_t>(ditheredMix) * (LCD_HEIGHT - 1)) / 255U);
      gSkyLUT[y][xm] = skyColorForY(ditheredY);
    }
  }
  gSkyLUTPercent = gSkyPercent;
}

uint16_t skyColorForPosition(int16_t x, int16_t y)
{
  const int16_t ySafe = max<int16_t>(0, min<int16_t>(LCD_HEIGHT - 1, y));
  return gSkyLUT[ySafe][x & 3];
}

void drawSkyBackground()
{
  if (gSkyLUTPercent != gSkyPercent) rebuildSkyLUT();

  uint16_t *framebuffer = gUseCanvas ? canvas->framebuffer() : nullptr;
  if (framebuffer) {
    for (int16_t y = 0; y < LCD_HEIGHT; ++y) {
      uint16_t *dst = framebuffer + static_cast<size_t>(y) * LCD_WIDTH;
      const uint16_t c0 = gSkyLUT[y][0];
      const uint16_t c1 = gSkyLUT[y][1];
      const uint16_t c2 = gSkyLUT[y][2];
      const uint16_t c3 = gSkyLUT[y][3];
      // LCD_WIDTH is 368 (divisible by 4), unroll the 4-wide Bayer pattern.
      for (int16_t x = 0; x < LCD_WIDTH; x += 4) {
        dst[x]     = c0;
        dst[x + 1] = c1;
        dst[x + 2] = c2;
        dst[x + 3] = c3;
      }
    }
    return;
  }

  for (int16_t y = 0; y < LCD_HEIGHT; ++y) {
    for (int16_t x = 0; x < LCD_WIDTH; ++x) {
      gfx->drawPixel(x, y, gSkyLUT[y][x & 3]);
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

void drawShootingStars()
{
  for (size_t i = 0; i < kShootingStarCount; ++i) {
    const ShootingStar &s = gShootingStars[i];
    if (!s.active) continue;

    // Fade in quickly, fade out slowly, with a high floor so it's never faint
    const float t = s.lifeSec / s.maxLifeSec;
    const float alpha = (t < 0.15f) ? (t / 0.15f) : (1.0f - (t - 0.15f) / 0.85f);
    const uint8_t intensity = static_cast<uint8_t>(constrain(static_cast<int>(alpha * 255.0f), 0, 255));

    // Bold head + long tapered trail
    const int tailLen = 140;
    for (int step = 0; step < tailLen; ++step) {
      const float fade = 1.0f - (static_cast<float>(step) / tailLen);
      const uint8_t segIntensity = static_cast<uint8_t>(intensity * fade);
      const int16_t px = static_cast<int16_t>(s.x - s.vx * (step * 0.022f));
      const int16_t py = static_cast<int16_t>(s.y - s.vy * (step * 0.022f));
      if (px < 0 || px >= LCD_WIDTH || py < 0 || py >= LCD_HEIGHT) continue;
      const uint16_t bg = skyColorForPosition(px, py);
      const uint16_t color = blend565(bg, kStarWhite, segIntensity);
      int radius;
      if (step < 2) radius = 4;       // bright head
      else if (step < 12) radius = 3;
      else if (step < 40) radius = 2;
      else radius = 1;
      if (radius == 1) {
        scene->drawPixel(px, py, color);
      } else {
        scene->fillCircle(px, py, radius, color);
      }
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

  constexpr int orbCenterX = LCD_WIDTH / 2;
  constexpr int orbCenterY = LCD_HEIGHT / 2;
  constexpr int orbTugRadius = 80;          // touches within this radius are "on the orb"
  constexpr int orbTugActivatePx = 10;      // drag must move this far before direction locks
  constexpr int orbHorizThresholdPx = 50;   // horizontal tug must reach this to trigger nav
  constexpr int orbHorizCommitted = 1;      // gDragActive value encoding "horizontal tug"
  constexpr int orbVertCommitted = 2;       // encoding "vertical (volume) tug"

  if (pressedNow && !wasPressed) {
    gTapTracking = true;
    gTapStartedAtMs = millis();
    gTapStartX = gTouchX;
    gTapStartY = gTouchY;
    const int odx = gTouchX - orbCenterX;
    const int ody = gTouchY - orbCenterY;
    gOrbTugActive = (odx * odx + ody * ody) <= orbTugRadius * orbTugRadius;
    gDragActive = 0;
    gVolTugLastMs = 0;
    gVolTugAccumQ8 = 0;
  } else if (!pressedNow && wasPressed) {
    const int dxSigned = gTouchX - gTapStartX;
    const int dySigned = gTouchY - gTapStartY;
    const int dxAbs = abs(dxSigned);
    const int dyAbs = abs(dySigned);
    const uint32_t heldMs = millis() - gTapStartedAtMs;
    if (gTapTracking && heldMs <= kTapMaxDurationMs && dxAbs <= kTapMaxTravelPx && dyAbs <= kTapMaxTravelPx) {
      toggleAnimationPause();
    } else if (gOrbTugActive && gDragActive == orbHorizCommitted) {
      // Horizontal tug released — trigger nav if it crossed the threshold.
      if (dxSigned >= orbHorizThresholdPx) {
        if (gAnimationPaused) {
          toggleAnimationPause();
          logf("Locket: orb tug right (paused) → play\n");
        } else {
          gSkipTrack = true;
          logf("Locket: orb tug right → next track\n");
        }
      } else if (dxSigned <= -orbHorizThresholdPx) {
        gRestartTrackRequested = true;
        logf("Locket: orb tug left → restart track\n");
      }
    } else if (gOrbTugActive && gDragActive == orbVertCommitted) {
      // Vertical tug: volume has been ticking continuously — apply final value.
      if (gAudioCodec) {
        es8311_voice_volume_set(gAudioCodec, gVolume, nullptr);
      }
      logf("Locket: orb tug vertical released → volume=%d%%\n", gVolume);
    }
    gTapTracking = false;
    gOrbTugActive = false;
    gDragActive = 0;
    gOrbOffsetX = 0;
    gOrbOffsetY = 0;
  } else if (pressedNow && gTapTracking) {
    const int dxAbs = abs(gTouchX - gTapStartX);
    const int dyAbs = abs(gTouchY - gTapStartY);
    if (dxAbs > kTapMaxTravelPx || dyAbs > kTapMaxTravelPx) {
      gTapTracking = false;
    }
  }

  // Once movement passes the activation distance, lock the tug to a cardinal axis.
  if (pressedNow && gOrbTugActive && gDragActive == 0) {
    const int dxAbs = abs(gTouchX - gTapStartX);
    const int dyAbs = abs(gTouchY - gTapStartY);
    if (dxAbs >= orbTugActivatePx || dyAbs >= orbTugActivatePx) {
      gDragActive = (dxAbs > dyAbs) ? orbHorizCommitted : orbVertCommitted;
      gTapTracking = false;  // past threshold → definitely not a tap
    }
  }

  // Vertical tug: rate-based volume. Past a small dead-zone, the offset becomes a rate,
  // so holding pulled-up keeps raising volume, and a bigger pull changes it faster.
  if (pressedNow && gOrbTugActive && gDragActive == orbVertCommitted) {
    const uint32_t now_ms = millis();
    if (gVolTugLastMs == 0) gVolTugLastMs = now_ms;
    const uint32_t dtMs = now_ms - gVolTugLastMs;
    gVolTugLastMs = now_ms;

    constexpr int kDeadZonePx = 12;
    const int dy = gTouchY - gTapStartY;   // +down / −up
    int pastDz = 0;
    if (dy >  kDeadZonePx) pastDz = dy - kDeadZonePx;
    else if (dy < -kDeadZonePx) pastDz = dy + kDeadZonePx;

    // Rate law: ~80 px past dead-zone ≈ 10 %/s (slow — fine-grained control is the goal;
    // reach the extremes by pulling hard). Accumulate in Q8 so sub-1% ticks don't lose.
    gVolTugAccumQ8 += (-static_cast<int32_t>(pastDz) * static_cast<int32_t>(dtMs) * 256) / 8000;
    const int32_t wholeSteps = gVolTugAccumQ8 >> 8;
    if (wholeSteps != 0) {
      gVolTugAccumQ8 -= wholeSteps << 8;
      const int newVol = constrain(gVolume + static_cast<int>(wholeSteps), 0, 100);
      if (newVol != gVolume) {
        gVolume = newVol;
        if (gAudioCodec && now_ms - gLastVolumeSetMs >= 20) {
          gLastVolumeSetMs = now_ms;
          es8311_voice_volume_set(gAudioCodec, gVolume, nullptr);
        }
      }
    }
  } else {
    gVolTugLastMs = 0;
    gVolTugAccumQ8 = 0;
  }

  // Visible "pull" offset — track the finger from the instant it lands on the orb.
  // Pre-lock: free 2D rubber-band. After axis lock: constrain to the locked axis.
  if (pressedNow && gOrbTugActive) {
    const int rawX = (gTouchX - gTapStartX) / 2;
    const int rawY = (gTouchY - gTapStartY) / 2;
    if (gDragActive == orbHorizCommitted) {
      gOrbOffsetX = constrain(rawX, -30, 30);
      gOrbOffsetY = 0;
    } else if (gDragActive == orbVertCommitted) {
      gOrbOffsetX = 0;
      gOrbOffsetY = constrain(rawY, -30, 30);
    } else {
      gOrbOffsetX = constrain(rawX, -30, 30);
      gOrbOffsetY = constrain(rawY, -30, 30);
    }
  } else if (!pressedNow) {
    gOrbOffsetX = 0;
    gOrbOffsetY = 0;
  }

  gTouchPressed = pressedNow;
  gTouchController->IIC_Interrupt_Flag = false;
}

// Precomputed moon masks (rotation-invariant, built once at boot).
// Mask lives in PSRAM — indexed in the crescent's LOCAL frame (cut on +x axis).
// At draw time we rotate each world pixel back into the local frame and sample.
constexpr int kMoonMaskR = 210;                          // covers moonRadius(177) + glowReach(32) + margin
constexpr int kMoonMaskSize = 2 * kMoonMaskR + 1;        // 421
uint8_t *gMoonGlowMask = nullptr;                        // [N] glow intensity 0..210, 0 if body
uint8_t *gMoonBodyMask = nullptr;                        // [N] 0=empty,1=interior,2=outerBorder,3=innerBorder

void buildMoonMasks()
{
  if (gMoonGlowMask && gMoonBodyMask) return;

  const size_t N = static_cast<size_t>(kMoonMaskSize) * static_cast<size_t>(kMoonMaskSize);
  gMoonGlowMask = static_cast<uint8_t *>(ps_calloc(N, 1));
  gMoonBodyMask = static_cast<uint8_t *>(ps_calloc(N, 1));
  if (!gMoonGlowMask || !gMoonBodyMask) {
    logf("Locket: moon mask alloc failed (%zu bytes each)\n", N);
    return;
  }

  constexpr float moonR = 177.0f;
  constexpr float cutR  = 177.0f - 20.0f;
  constexpr float cutOff = 177.0f * 0.49f;
  constexpr float borderPx = 3.0f;
  const float moonRSq = moonR * moonR;
  const float cutRSq  = cutR  * cutR;
  const float moonInnerRSq = (moonR - borderPx) * (moonR - borderPx);
  const float cutOuterRSq  = (cutR  + borderPx) * (cutR  + borderPx);
  constexpr float glowReachF = 32.0f;

  // Corner points in local frame (cut center at (cutOff, 0))
  float cornerLx[2] = {0, 0}, cornerLy[2] = {0, 0};
  bool cornersValid = false;
  {
    const float d = cutOff;
    if (d > 1e-3f && d < moonR + cutR && d > fabsf(moonR - cutR)) {
      const float a = (moonR * moonR - cutR * cutR + d * d) / (2.0f * d);
      const float hSq = moonR * moonR - a * a;
      if (hSq >= 0.0f) {
        const float h = sqrtf(hSq);
        cornerLx[0] = a;  cornerLy[0] =  h;
        cornerLx[1] = a;  cornerLy[1] = -h;
        cornersValid = true;
      }
    }
  }

  for (int my = 0; my < kMoonMaskSize; ++my) {
    const float ly = static_cast<float>(my - kMoonMaskR);
    for (int mx = 0; mx < kMoonMaskSize; ++mx) {
      const float lx = static_cast<float>(mx - kMoonMaskR);
      const float oSq = lx * lx + ly * ly;
      const float ccdx = lx - cutOff;
      const float ccdy = ly;
      const float cSq = ccdx * ccdx + ccdy * ccdy;
      const size_t idx = static_cast<size_t>(my) * kMoonMaskSize + static_cast<size_t>(mx);

      // Body classification
      if (oSq <= moonRSq && cSq >= cutRSq) {
        uint8_t state = 1;
        if (oSq >= moonInnerRSq) state = 2;
        else if (cSq <= cutOuterRSq) state = 3;
        gMoonBodyMask[idx] = state;
        continue;
      }

      // Glow pass — min distance to crescent outline
      float bestDist = glowReachF + 1.0f;

      if (oSq > 0.0f) {
        const float od = sqrtf(oSq);
        const float s = moonR / od;
        const float px = lx * s;
        const float py = ly * s;
        const float pcx = px - cutOff;
        const float pcy = py;
        if (pcx * pcx + pcy * pcy >= cutRSq) {
          const float d = fabsf(od - moonR);
          if (d < bestDist) bestDist = d;
        }
      }
      if (cSq > 0.0f) {
        const float cd = sqrtf(cSq);
        const float s = cutR / cd;
        const float px = cutOff + ccdx * s;
        const float py = ccdy * s;
        if (px * px + py * py <= moonRSq) {
          const float d = fabsf(cd - cutR);
          if (d < bestDist) bestDist = d;
        }
      }
      if (cornersValid) {
        for (int c = 0; c < 2; ++c) {
          const float rdx = lx - cornerLx[c];
          const float rdy = ly - cornerLy[c];
          const float d = sqrtf(rdx * rdx + rdy * rdy);
          if (d < bestDist) bestDist = d;
        }
      }

      if (bestDist < glowReachF) {
        const float t = 1.0f - (bestDist / glowReachF);
        gMoonGlowMask[idx] = static_cast<uint8_t>(t * t * 210.0f);
      }
    }
  }

  logf("Locket: moon masks built (%dx%d)\n", kMoonMaskSize, kMoonMaskSize);
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

  const float orbitAngle = kFeatureMoonOrbSpin ? -phaseAngleForPeriodUs(orbitPeriodUs) : 0.0f;  // current rotation angle of the cut circle
  const float cutOffset = static_cast<float>(moonRadius) * cutOffsetFactor;                 // pixel distance from moon center to cut center
  const int16_t cutX = static_cast<int16_t>(roundf(moonX + cosf(orbitAngle) * cutOffset));  // x position of cut center
  const int16_t cutY = static_cast<int16_t>(roundf(moonY + sinf(orbitAngle) * cutOffset));  // y position of cut center

  uint16_t *framebuffer = gUseCanvas ? canvas->framebuffer() : nullptr;
  const int32_t moonRadiusSq = moonRadius * moonRadius;   // squared radii for distance checks (avoids sqrt)
  const int32_t cutRadiusSq = cutRadius * cutRadius;

  // Border thickness (pixels) for the paused gold crescent
  constexpr int32_t borderPx = 3;
  const int32_t moonInnerRadiusSq = (moonRadius - borderPx) * (moonRadius - borderPx);
  const int32_t cutOuterRadiusSq = (cutRadius + borderPx) * (cutRadius + borderPx);

  // ---- Glow pass: warm halo around the crescent when active ----
  constexpr int32_t glowReach = 32;                                     // pixels outward from moon edge
  constexpr int32_t glowOuter = moonRadius + glowReach;
  constexpr int32_t glowOuterSq = glowOuter * glowOuter;
  constexpr uint16_t kMoonGlow = 0xFFFF;                                // pure white

  // Compute the two corner points where moon outer circle and cut circle intersect.
  // These are the sharp tips of the crescent — both glow passes miss them because
  // the projection onto either circle's edge falls outside the "valid" region there.
  float cornerX[2] = {0, 0}, cornerY[2] = {0, 0};
  bool cornersValid = false;
  {
    const float dx = static_cast<float>(cutX - moonX);
    const float dy = static_cast<float>(cutY - moonY);
    const float d = sqrtf(dx * dx + dy * dy);
    if (d > 1e-3f && d < static_cast<float>(moonRadius + cutRadius) &&
        d > fabsf(static_cast<float>(moonRadius - cutRadius))) {
      const float a = (static_cast<float>(moonRadius) * static_cast<float>(moonRadius) -
                       static_cast<float>(cutRadius) * static_cast<float>(cutRadius) + d * d) /
                      (2.0f * d);
      const float hSq = static_cast<float>(moonRadius) * static_cast<float>(moonRadius) - a * a;
      if (hSq >= 0.0f) {
        const float h = sqrtf(hSq);
        const float px = static_cast<float>(moonX) + a * dx / d;
        const float py = static_cast<float>(moonY) + a * dy / d;
        const float rx = -dy / d * h;
        const float ry = dx / d * h;
        cornerX[0] = px + rx;
        cornerY[0] = py + ry;
        cornerX[1] = px - rx;
        cornerY[1] = py - ry;
        cornersValid = true;
      }
    }
  }

  // Fast path: rotation-sampled mask lookup (built once, no per-pixel sqrts).
  if (framebuffer && gMoonGlowMask && gMoonBodyMask) {
    // Q16 fixed-point rotation — avoids per-pixel float→int conversion.
    const int32_t cosQ = static_cast<int32_t>(cosf(orbitAngle) * 65536.0f);
    const int32_t sinQ = static_cast<int32_t>(sinf(orbitAngle) * 65536.0f);
    const int32_t bboxR = kMoonMaskR;
    const bool txActive = gPauseTransitionActive;
    const bool paused = gAnimationPaused;
    const int32_t bboxRSq = bboxR * bboxR;

    for (int16_t y = moonY - bboxR; y <= moonY + bboxR; ++y) {
      if (y < 0 || y >= LCD_HEIGHT) continue;
      const int32_t wdyI = y - moonY;
      const int32_t wdyISq = wdyI * wdyI;
      // Row-invariant components: sinQ*wdy and cosQ*wdy.
      const int32_t sinQ_wdy = sinQ * wdyI;
      const int32_t cosQ_wdy = cosQ * wdyI;
      for (int16_t x = moonX - bboxR; x <= moonX + bboxR; ++x) {
        if (x < 0 || x >= LCD_WIDTH) continue;
        const int32_t wdxI = x - moonX;
        // Disk early-out: mask content is empty beyond bboxR radius.
        if (wdxI * wdxI + wdyISq > bboxRSq) continue;
        // Rotate world→local by -orbitAngle via integer Q16 math.
        const int32_t lxQ = cosQ * wdxI + sinQ_wdy;
        const int32_t lyQ = -sinQ * wdxI + cosQ_wdy;
        const int mx = ((lxQ + 32768) >> 16) + kMoonMaskR;
        const int my = ((lyQ + 32768) >> 16) + kMoonMaskR;
        if (mx < 0 || mx >= kMoonMaskSize || my < 0 || my >= kMoonMaskSize) continue;
        const size_t maskIdx = static_cast<size_t>(my) * kMoonMaskSize + static_cast<size_t>(mx);
        const uint8_t bodyState = gMoonBodyMask[maskIdx];
        const size_t fbIdx = static_cast<size_t>(y) * LCD_WIDTH + static_cast<size_t>(x);
        if (bodyState) {
          const bool isBorder = (bodyState == 2 || bodyState == 3);
          const uint16_t pausedColor = isBorder ? kMoonBorder : kMoonGold;
          const uint16_t activeColor = kMoonHot;
          uint16_t moonColor;
          if (txActive) {
            const uint8_t txAmt = moonTransitionAmount(x, y, moonX, moonY);
            const uint16_t newColor = paused ? pausedColor : activeColor;
            const uint16_t oldColor = paused ? activeColor : pausedColor;
            moonColor = blend565(oldColor, newColor, txAmt);
          } else {
            moonColor = paused ? pausedColor : activeColor;
          }
          framebuffer[fbIdx] = moonColor;
        } else {
          const uint8_t glowAmt = gMoonGlowMask[maskIdx];
          if (!glowAmt) continue;
          uint8_t activeWeight;
          if (txActive) {
            const uint8_t txAmt = moonTransitionAmount(x, y, moonX, moonY);
            activeWeight = paused ? static_cast<uint8_t>(255 - txAmt) : txAmt;
          } else if (paused) {
            continue;  // no glow when paused and no transition running
          } else {
            activeWeight = 255;
          }
          const uint16_t finalAmt = (static_cast<uint16_t>(glowAmt) * activeWeight) / 255;
          if (finalAmt == 0) continue;
          framebuffer[fbIdx] = blend565(framebuffer[fbIdx], kMoonGlow, finalAmt);
        }
      }
    }
    return;  // Done — skip the legacy glow+body paths below.
  }

  // Unified glow: for each pixel in the bounding box around the crescent, compute
  // the true minimum distance to the crescent outline (two arcs joined at corners),
  // then apply a quadratic glow falloff. This eliminates seams between separate passes.
  if (framebuffer && cornersValid) {
    const float moonRf = static_cast<float>(moonRadius);
    const float cutRf = static_cast<float>(cutRadius);
    const int32_t bboxR = moonRadius + glowReach;
    const float reachF = static_cast<float>(glowReach);
    // Squared-distance bounds for early-out — a pixel can only glow if it's within
    // glowReach of at least one of: outer arc, inner arc, or a corner point.
    const int32_t outerNearSq = (moonRadius - glowReach) * (moonRadius - glowReach);
    const int32_t outerFarSq  = (moonRadius + glowReach) * (moonRadius + glowReach);
    const int32_t cutNearSq   = (cutRadius  - glowReach) * (cutRadius  - glowReach);
    const int32_t cutFarSq    = (cutRadius  + glowReach) * (cutRadius  + glowReach);
    const int32_t cornerReachSq = glowReach * glowReach;
    const bool pauseTxActive = gPauseTransitionActive;
    const bool paused = gAnimationPaused;
    // When no transition is running, moonTransitionAmount returns 255 → activeWeight
    // is constant across the loop; avoid the per-pixel function call.
    const uint8_t staticActiveWeight = paused ? 0 : 255;

    for (int16_t y = moonY - bboxR; y <= moonY + bboxR; ++y) {
      if (y < 0 || y >= LCD_HEIGHT) continue;
      for (int16_t x = moonX - bboxR; x <= moonX + bboxR; ++x) {
        if (x < 0 || x >= LCD_WIDTH) continue;

        const int32_t odx = x - moonX;
        const int32_t ody = y - moonY;
        const int32_t oSq = odx * odx + ody * ody;
        const int32_t cdx = x - cutX;
        const int32_t cdy = y - cutY;
        const int32_t cSq = cdx * cdx + cdy * cdy;

        // Skip pixels inside the crescent (moon body will be drawn on top)
        if (oSq <= moonRadiusSq && cSq >= cutRadiusSq) continue;

        // Early-out: is this pixel near ANY part of the outline? If not, no sqrt needed.
        const bool nearOuter = (oSq >= outerNearSq && oSq <= outerFarSq);
        const bool nearInner = (cSq >= cutNearSq   && cSq <= cutFarSq);
        const int32_t c0dx = x - static_cast<int32_t>(cornerX[0]);
        const int32_t c0dy = y - static_cast<int32_t>(cornerY[0]);
        const int32_t c0Sq = c0dx * c0dx + c0dy * c0dy;
        const int32_t c1dx = x - static_cast<int32_t>(cornerX[1]);
        const int32_t c1dy = y - static_cast<int32_t>(cornerY[1]);
        const int32_t c1Sq = c1dx * c1dx + c1dy * c1dy;
        const bool nearCorner = (c0Sq <= cornerReachSq || c1Sq <= cornerReachSq);
        if (!nearOuter && !nearInner && !nearCorner) continue;

        // If paused and no transition is running, glow contributes zero — skip.
        if (!pauseTxActive && paused) continue;

        float bestDist = reachF + 1.0f;

        // Outer arc: project onto moon outer circle. Valid if projection is outside cut.
        if (nearOuter && oSq > 0) {
          const float od = sqrtf(static_cast<float>(oSq));
          const float s = moonRf / od;
          const float px = moonX + odx * s;
          const float py = moonY + ody * s;
          const float pcx = px - cutX;
          const float pcy = py - cutY;
          if (pcx * pcx + pcy * pcy >= cutRf * cutRf) {
            const float d = fabsf(od - moonRf);
            if (d < bestDist) bestDist = d;
          }
        }
        // Inner arc: project onto cut circle. Valid if projection is inside moon.
        if (nearInner && cSq > 0) {
          const float cd = sqrtf(static_cast<float>(cSq));
          const float s = cutRf / cd;
          const float px = cutX + cdx * s;
          const float py = cutY + cdy * s;
          const float pmx = px - moonX;
          const float pmy = py - moonY;
          if (pmx * pmx + pmy * pmy <= moonRf * moonRf) {
            const float d = fabsf(cd - cutRf);
            if (d < bestDist) bestDist = d;
          }
        }
        // Corner points (guarantees coverage where arc projections fall outside their valid ranges).
        // Only take sqrt if the squared-distance bound was already satisfied (nearCorner).
        if (c0Sq <= cornerReachSq) {
          const float d = sqrtf(static_cast<float>(c0Sq));
          if (d < bestDist) bestDist = d;
        }
        if (c1Sq <= cornerReachSq) {
          const float d = sqrtf(static_cast<float>(c1Sq));
          if (d < bestDist) bestDist = d;
        }

        if (bestDist >= reachF) continue;
        const float t = 1.0f - (bestDist / reachF);
        const uint8_t glowAmt = static_cast<uint8_t>(t * t * 210.0f);
        if (glowAmt == 0) continue;

        uint8_t activeWeight;
        if (pauseTxActive) {
          const uint8_t txAmt = moonTransitionAmount(x, y, moonX, moonY);
          activeWeight = paused ? (255 - txAmt) : txAmt;
        } else {
          activeWeight = staticActiveWeight;
        }
        const uint16_t finalAmt = (static_cast<uint16_t>(glowAmt) * activeWeight) / 255;
        if (finalAmt == 0) continue;

        const size_t idx = static_cast<size_t>(y) * LCD_WIDTH + static_cast<size_t>(x);
        framebuffer[idx] = blend565(framebuffer[idx], kMoonGlow, finalAmt);
      }
    }
  }

  const bool bodyTxActive = gPauseTransitionActive;
  const bool bodyPaused = gAnimationPaused;
  for (int16_t y = moonY - moonRadius; y <= moonY + moonRadius; ++y) {
    if (y < 0 || y >= LCD_HEIGHT) continue;

    for (int16_t x = moonX - moonRadius; x <= moonX + moonRadius; ++x) {
      if (x < 0 || x >= LCD_WIDTH) continue;

      const int32_t outerDx = x - moonX;
      const int32_t outerDy = y - moonY;
      const int32_t outerDistSq = (outerDx * outerDx) + (outerDy * outerDy);
      if (outerDistSq > moonRadiusSq) continue;

      const int32_t cutDx = x - cutX;
      const int32_t cutDy = y - cutY;
      const int32_t cutDistSq = (cutDx * cutDx) + (cutDy * cutDy);
      if (cutDistSq < cutRadiusSq) continue;

      // Paused color: gold, with black border near outer/inner edge
      const bool onOuterBorder = outerDistSq >= moonInnerRadiusSq;
      const bool onInnerBorder = cutDistSq <= cutOuterRadiusSq;
      const uint16_t pausedColor = (onOuterBorder || onInnerBorder) ? kMoonBorder : kMoonGold;
      const uint16_t activeColor = kMoonHot;

      uint16_t moonColor;
      if (bodyTxActive) {
        const uint8_t txAmt = moonTransitionAmount(x, y, moonX, moonY);
        const uint16_t newColor = bodyPaused ? pausedColor : activeColor;
        const uint16_t oldColor = bodyPaused ? activeColor : pausedColor;
        moonColor = blend565(oldColor, newColor, txAmt);
      } else {
        moonColor = bodyPaused ? pausedColor : activeColor;
      }

      if (framebuffer) {
        const size_t pixelIndex = (static_cast<size_t>(y) * LCD_WIDTH) + static_cast<size_t>(x);
        framebuffer[pixelIndex] = moonColor;
      } else {
        gfx->drawPixel(x, y, moonColor);
      }
    }
  }
}

void drawOrb()
{
  const int16_t orbX = LCD_WIDTH / 2 + gOrbOffsetX;
  const int16_t orbY = LCD_HEIGHT / 2 + gOrbOffsetY;
  constexpr int16_t orbRadius = 42;
  constexpr uint32_t spinPeriodUs = 2100000UL;
  constexpr uint16_t kOrbGlowOuter = 0xFD20;
  constexpr uint16_t kOrbGlowInner = 0xFBE0;
  const float spinAngle = kFeatureMoonOrbSpin ? -phaseAngleForPeriodUs(spinPeriodUs) : 0.0f;

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

  // Black borders around the orb ring (paused state), with right-to-left wipe
  uint16_t *fb = gUseCanvas ? canvas->framebuffer() : nullptr;
  constexpr int32_t kOrbBorderPx = 3;
  constexpr int32_t outerR1 = orbRadius + 8;                // outer edge of outer border
  constexpr int32_t outerR0 = outerR1 - kOrbBorderPx;       // inner edge of outer border
  constexpr int32_t innerR1 = orbRadius;                    // outer edge of inner border
  constexpr int32_t innerR0 = innerR1 - kOrbBorderPx;       // inner edge of inner border
  constexpr int32_t outerR1Sq = outerR1 * outerR1;
  constexpr int32_t outerR0Sq = outerR0 * outerR0;
  constexpr int32_t innerR1Sq = innerR1 * innerR1;
  constexpr int32_t innerR0Sq = innerR0 * innerR0;

  for (int16_t y = orbY - outerR1; y <= orbY + outerR1; ++y) {
    if (y < 0 || y >= LCD_HEIGHT) continue;
    for (int16_t x = orbX - outerR1; x <= orbX + outerR1; ++x) {
      if (x < 0 || x >= LCD_WIDTH) continue;
      const int32_t dx = x - orbX;
      const int32_t dy = y - orbY;
      const int32_t dSq = dx * dx + dy * dy;
      const bool onOuterBorder = (dSq >= outerR0Sq && dSq <= outerR1Sq);
      const bool onInnerBorder = (dSq >= innerR0Sq && dSq <= innerR1Sq);
      if (!onOuterBorder && !onInnerBorder) continue;

      const uint8_t txAmt = columnTransitionAmount(x);
      uint16_t borderColor;
      if (fb) {
        const size_t idx = static_cast<size_t>(y) * LCD_WIDTH + static_cast<size_t>(x);
        const uint16_t existing = fb[idx];
        // Paused state shows black; active state shows the existing orb pixel
        const uint16_t newColor = gAnimationPaused ? kMoonBorder : existing;
        const uint16_t oldColor = gAnimationPaused ? existing : kMoonBorder;
        borderColor = blend565(oldColor, newColor, txAmt);
        fb[idx] = borderColor;
      } else {
        if (gAnimationPaused) {
          gfx->drawPixel(x, y, kMoonBorder);
        }
      }
    }
  }
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

  // Clamp dx range to the visible horizontal extent — removes per-pixel bounds checks.
  int16_t dxStart = max<int16_t>(0, -originX);
  int16_t dxEnd   = min<int16_t>(drawWidth, LCD_WIDTH - originX);
  if (dxStart >= dxEnd) return;

  // Q16 step accumulator replaces a per-pixel 32-bit divide.
  const uint32_t stepQ16 = (static_cast<uint32_t>(sourceWidth) << 16) / static_cast<uint32_t>(drawWidth);
  const uint16_t flipBase = cloud.flipX ? static_cast<uint16_t>(sourceWidth - 1U) : 0;

  const uint16_t alphaScale = static_cast<uint16_t>(17U * cloud.alphaPct);

  // Precompute 16-entry tone and opacity tables for this cloud (4-bit light + 4-bit alpha nibbles).
  uint16_t toneLUT[16];
  uint8_t opacityLUT[16];
  for (int i = 0; i < 16; ++i) {
    toneLUT[i] = cloudTone(static_cast<uint8_t>(i));
    uint16_t o = (static_cast<uint16_t>(i) * alphaScale) / 100U;
    opacityLUT[i] = static_cast<uint8_t>(o > 235U ? 235U : o);
  }

  for (int16_t dy = 0; dy < drawHeight; ++dy) {
    const int16_t destY = originY + dy;
    if (destY < 0 || destY >= LCD_HEIGHT) {
      continue;
    }

    const uint16_t srcY = static_cast<uint16_t>((static_cast<uint32_t>(dy) * sourceHeight) / drawHeight);
    const size_t srcRow = static_cast<size_t>(srcY) * sourceWidth;
    const size_t destRow = static_cast<size_t>(destY) * LCD_WIDTH;

    if (framebuffer) {
      uint16_t *dst = framebuffer + destRow;
      uint32_t srcXQ16 = stepQ16 * static_cast<uint32_t>(dxStart);
      for (int16_t dx = dxStart; dx < dxEnd; ++dx, srcXQ16 += stepQ16) {
        uint16_t sx = static_cast<uint16_t>(srcXQ16 >> 16);
        if (cloud.flipX) sx = flipBase - sx;
        const uint8_t packed = pixels[srcRow + sx];
        const uint8_t alpha4 = packed >> 4;
        if (!alpha4) continue;
        const uint8_t opacity = opacityLUT[alpha4];
        const uint16_t color = toneLUT[packed & 0x0F];
        const int16_t destX = originX + dx;
        dst[destX] = blend565(dst[destX], color, opacity);
      }
    } else {
      uint32_t srcXQ16 = stepQ16 * static_cast<uint32_t>(dxStart);
      for (int16_t dx = dxStart; dx < dxEnd; ++dx, srcXQ16 += stepQ16) {
        uint16_t sx = static_cast<uint16_t>(srcXQ16 >> 16);
        if (cloud.flipX) sx = flipBase - sx;
        const uint8_t packed = pixels[srcRow + sx];
        const uint8_t alpha4 = packed >> 4;
        if (!alpha4) continue;
        const uint8_t opacity = opacityLUT[alpha4];
        const uint16_t color = toneLUT[packed & 0x0F];
        const int16_t destX = originX + dx;
        gfx->drawPixel(destX, destY, blend565(skyColorForPosition(destX, destY), color, opacity));
      }
    }
  }
}

struct FrameTimings {
  uint32_t sky, stars, shootingStars, clouds, moon, orb, scroll;
};
FrameTimings gLastFrameTimings = {};

void renderFrame()
{
  const uint64_t t0 = esp_timer_get_time();
  drawSkyBackground();
  const uint64_t t1 = esp_timer_get_time();
  drawStars();
  const uint64_t t2 = esp_timer_get_time();
  drawShootingStars();
  const uint64_t t3 = esp_timer_get_time();
  for (size_t index = 0; index < kCloudCount; ++index) {
    drawCloud(gClouds[index]);
  }
  drawLightning();  // flashes across the sky + clouds while charging
  const uint64_t t4 = esp_timer_get_time();
  drawMoon();
  const uint64_t t5 = esp_timer_get_time();
  drawOrb();
  const uint64_t t6 = esp_timer_get_time();
  drawScrollImage();  // on top of everything
  const uint64_t t7 = esp_timer_get_time();
  gLastFrameTimings.sky = static_cast<uint32_t>(t1 - t0);
  gLastFrameTimings.stars = static_cast<uint32_t>(t2 - t1);
  gLastFrameTimings.shootingStars = static_cast<uint32_t>(t3 - t2);
  gLastFrameTimings.clouds = static_cast<uint32_t>(t4 - t3);
  gLastFrameTimings.moon = static_cast<uint32_t>(t5 - t4);
  gLastFrameTimings.orb = static_cast<uint32_t>(t6 - t5);
  gLastFrameTimings.scroll = static_cast<uint32_t>(t7 - t6);
}

void updateStars(float elapsedSeconds)
{
  if (!kFeatureStarsDrifting) return;
  for (size_t index = 0; index < kBackgroundStarCount; ++index) {
    BackgroundStar &star = gBackgroundStars[index];
    star.x -= kSceneDriftPixelsPerSecond * elapsedSeconds;
    if (star.x < -static_cast<float>(star.radius + 2)) {
      resetBackgroundStar(index, false);
    }
  }

}

void updateShootingStars(float elapsedSeconds)
{
  if (!kFeatureShootingStars) return;
  // Move and age active stars — runs even while paused so they keep streaming.
  constexpr float kCurveRateRad = 0.0f;
  for (size_t i = 0; i < kShootingStarCount; ++i) {
    ShootingStar &s = gShootingStars[i];
    if (!s.active) continue;
    const float cr = cosf(kCurveRateRad * elapsedSeconds);
    const float sr = sinf(kCurveRateRad * elapsedSeconds);
    const float nvx = s.vx * cr - s.vy * sr;
    const float nvy = s.vx * sr + s.vy * cr;
    s.vx = nvx;
    s.vy = nvy;
    s.x += s.vx * elapsedSeconds;
    s.y += s.vy * elapsedSeconds;
    s.lifeSec += elapsedSeconds;
    if (s.lifeSec >= s.maxLifeSec || s.x < -40 || s.x > LCD_WIDTH + 40 ||
        s.y < -40 || s.y > LCD_HEIGHT + 40) {
      s.active = false;
    }
  }

  // Spawn new shooting stars only while USB is connected. In-flight stars keep going
  // so they finish naturally if the cable is unplugged mid-streak.
  if (gIsPluggedIn && esp_random() % 1000 < 250) {
    for (size_t i = 0; i < kShootingStarCount; ++i) {
      if (!gShootingStars[i].active) {
        ShootingStar &s = gShootingStars[i];
        s.active = true;
        // Point-source radiant near the top-right corner of the screen (slightly off-screen
        // so the convergence point is outside the visible area). Each star's velocity
        // points directly away from this radiant, so spawns on the top edge get shallower
        // (more horizontal) trajectories and spawns on the right edge get steeper (more
        // vertical) ones — the "greater/lesser incidence" effect.
        constexpr float radiantX = static_cast<float>(LCD_WIDTH) + 80.0f;
        constexpr float radiantY = -80.0f;

        // Spawn along the top or right edge (weighted by edge length).
        const uint32_t edgeRoll = esp_random() % static_cast<uint32_t>(LCD_WIDTH + LCD_HEIGHT);
        if (edgeRoll < static_cast<uint32_t>(LCD_WIDTH)) {
          s.x = static_cast<float>(edgeRoll);
          s.y = -20.0f;
        } else {
          s.x = static_cast<float>(LCD_WIDTH) + 20.0f;
          s.y = static_cast<float>(edgeRoll - static_cast<uint32_t>(LCD_WIDTH));
        }

        // Velocity: from radiant → spawn point, scaled to a random speed.
        const float dx = s.x - radiantX;
        const float dy = s.y - radiantY;
        const float invNorm = 1.0f / sqrtf(dx * dx + dy * dy);
        const float speed = 160.0f + static_cast<float>(esp_random() % 120);
        s.vx = dx * invNorm * speed;
        s.vy = dy * invNorm * speed;
        s.lifeSec = 0.0f;
        s.maxLifeSec = 2.0f + static_cast<float>(esp_random() % 150) / 100.0f; // 2.0–3.5s
        break;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Lightning (triggered while the battery is actively charging).
// ---------------------------------------------------------------------------
void spawnLightning()
{
  gLightning.active = true;
  gLightning.startMs = millis();
  // Two-beat flash: quick bright stroke, then a softer echo.
  gLightning.durationMs = 420 + (esp_random() % 180);

  // Main bolt: start somewhere along the upper portion of the screen and zig-zag down.
  int16_t x = static_cast<int16_t>(40 + (esp_random() % (LCD_WIDTH - 80)));
  int16_t y = 0;
  gLightning.mainCount = 0;
  for (size_t i = 0; i < kLightningMainPoints; ++i) {
    if (y >= LCD_HEIGHT + 20) break;
    gLightning.mainX[i] = x;
    gLightning.mainY[i] = y;
    ++gLightning.mainCount;
    const int step = 20 + static_cast<int>(esp_random() % 24);           // 20..43 px down
    const int jitter = static_cast<int>(esp_random() % 42) - 21;         // ±20 px
    y = static_cast<int16_t>(y + step);
    x = static_cast<int16_t>(constrain(x + jitter, 4, LCD_WIDTH - 5));
  }

  // Branch: fork off the main bolt somewhere in the middle third.
  gLightning.branchCount = 0;
  if (gLightning.mainCount >= 6 && (esp_random() & 1) == 0) {
    const size_t forkIdx = 2 + (esp_random() % (gLightning.mainCount - 4));
    int16_t bx = gLightning.mainX[forkIdx];
    int16_t by = gLightning.mainY[forkIdx];
    const int dir = ((esp_random() & 1) == 0) ? -1 : 1;
    for (size_t i = 0; i < kLightningBranchPoints; ++i) {
      if (by >= LCD_HEIGHT + 10) break;
      gLightning.branchX[i] = bx;
      gLightning.branchY[i] = by;
      ++gLightning.branchCount;
      const int step = 16 + static_cast<int>(esp_random() % 18);
      const int jitter = (dir * (18 + static_cast<int>(esp_random() % 18)));
      by = static_cast<int16_t>(by + step);
      bx = static_cast<int16_t>(constrain(bx + jitter, 4, LCD_WIDTH - 5));
    }
  }
}

void updateLightning(uint32_t nowMs)
{
  if (gLightning.active && nowMs - gLightning.startMs >= gLightning.durationMs) {
    gLightning.active = false;
  }
  // While actively charging, strike every few seconds.
  if (!gLightning.active && gIsActivelyCharging && nowMs >= gLightningNextMinMs) {
    // ~4% chance per frame → ~1–2 strikes per sec at 30fps, but we gate further with
    // a minimum cooldown so it doesn't feel spammy.
    if ((esp_random() % 100) < 4) {
      spawnLightning();
      gLightningNextMinMs = nowMs + 1800 + (esp_random() % 2200);  // 1.8–4 s cooldown
    }
  }
}

// Draw a jagged polyline with a soft halo (3 parallel passes) + bright core stroke.
// `coreAmt` / `glowAmt` are scaled intensities (0..255); blending with 0x0000 dims.
// Bresenham line that alpha-blends `color` into the framebuffer with `amt` as the
// alpha (0 = invisible, 255 = opaque). Skips work when amt == 0.
static void blendLineFb(uint16_t *fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                        uint16_t color, uint8_t amt)
{
  if (!fb || amt == 0) return;
  int16_t dx = x1 > x0 ? x1 - x0 : x0 - x1;
  int16_t dy = y1 > y0 ? y1 - y0 : y0 - y1;
  int16_t sx = x0 < x1 ? 1 : -1;
  int16_t sy = y0 < y1 ? 1 : -1;
  int16_t err = (dx > dy ? dx : -dy) / 2;
  int16_t x = x0, y = y0;
  for (;;) {
    if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) {
      const size_t idx = static_cast<size_t>(y) * LCD_WIDTH + static_cast<size_t>(x);
      fb[idx] = (amt == 255) ? color : blend565(fb[idx], color, amt);
    }
    if (x == x1 && y == y1) break;
    const int16_t e2 = err;
    if (e2 > -dx) { err -= dy; x += sx; }
    if (e2 <  dy) { err += dx; y += sy; }
  }
}

static void drawLightningPolyline(const int16_t *xs, const int16_t *ys, uint8_t count,
                                  uint16_t coreColor, uint16_t glowColor,
                                  uint8_t coreAmt, uint8_t glowAmt)
{
  if (count < 2) return;
  uint16_t *fb = gUseCanvas ? canvas->framebuffer() : nullptr;
  if (!fb) {
    // Canvas-less fallback: write the bolt opaquely so it's at least visible.
    for (uint8_t i = 1; i < count; ++i) {
      scene->drawLine(xs[i - 1], ys[i - 1], xs[i], ys[i], coreColor);
    }
    return;
  }
  for (uint8_t i = 1; i < count; ++i) {
    const int16_t x0 = xs[i - 1];
    const int16_t y0 = ys[i - 1];
    const int16_t x1 = xs[i];
    const int16_t y1 = ys[i];
    if (glowAmt) {
      blendLineFb(fb, x0 - 1, y0,     x1 - 1, y1,     glowColor, glowAmt);
      blendLineFb(fb, x0 + 1, y0,     x1 + 1, y1,     glowColor, glowAmt);
      blendLineFb(fb, x0,     y0 - 1, x1,     y1 - 1, glowColor, glowAmt);
    }
    blendLineFb(fb, x0, y0, x1, y1, coreColor, coreAmt);
  }
}

void drawLightning()
{
  if (!gLightning.active) return;
  const uint32_t nowMs = millis();
  const uint32_t elapsed = nowMs - gLightning.startMs;
  if (elapsed >= gLightning.durationMs) return;

  // Two-beat intensity: big spike in the first ~40% then a softer second strike.
  const float t = static_cast<float>(elapsed) / static_cast<float>(gLightning.durationMs);
  float intensity;
  if (t < 0.12f) {
    intensity = t / 0.12f;                       // sharp rise
  } else if (t < 0.40f) {
    intensity = 1.0f - (t - 0.12f) / 0.56f;      // first fade
  } else if (t < 0.55f) {
    intensity = 0.55f + (0.55f - t) / 0.15f * 0.2f;  // small rebound
  } else {
    intensity = 0.75f * (1.0f - (t - 0.55f) / 0.45f);
  }
  if (intensity < 0.0f) intensity = 0.0f;
  if (intensity > 1.0f) intensity = 1.0f;

  const uint8_t coreAmt = static_cast<uint8_t>(intensity * 255.0f);
  const uint8_t glowAmt = static_cast<uint8_t>(intensity * 140.0f);
  constexpr uint16_t kLightningCore = 0xFFFF;      // pure white
  constexpr uint16_t kLightningGlow = 0xBEFF;      // pale blue-white

  // Full-screen flash: brighten the framebuffer uniformly.
  uint16_t *fb = gUseCanvas ? canvas->framebuffer() : nullptr;
  if (fb) {
    const uint8_t flashAmt = static_cast<uint8_t>(intensity * 70.0f);
    if (flashAmt) {
      const size_t N = static_cast<size_t>(LCD_WIDTH) * static_cast<size_t>(LCD_HEIGHT);
      for (size_t i = 0; i < N; ++i) {
        fb[i] = blend565(fb[i], kLightningGlow, flashAmt);
      }
    }
  }

  drawLightningPolyline(gLightning.mainX, gLightning.mainY, gLightning.mainCount,
                        kLightningCore, kLightningGlow, coreAmt, glowAmt);
  if (gLightning.branchCount >= 2) {
    drawLightningPolyline(gLightning.branchX, gLightning.branchY, gLightning.branchCount,
                          kLightningCore, kLightningGlow,
                          static_cast<uint8_t>(coreAmt * 0.75f),
                          static_cast<uint8_t>(glowAmt * 0.75f));
  }
}

void updateClouds(float elapsedSeconds)
{
  if (!kFeatureCloudsMoving) return;
  for (size_t index = 0; index < kCloudCount; ++index) {
    Cloud &cloud = gClouds[index];
    cloud.x -= cloud.speed * elapsedSeconds * kSceneDriftPixelsPerSecond * 3.0f;
    if (cloud.x < -(cloudDrawWidth(cloud) + 24.0f)) {
      resetCloud(index, false);
    }
  }
}

WiFiMulti &wifiMulti()
{
  static WiFiMulti instance;
  return instance;
}

// Brings up OTA + telnet on first successful Wi-Fi association, regardless of
// whether the connection came from a WiFiMulti SSID or from BLE provisioning.
// Idempotent — repeated WL_CONNECTED events (post-roam) won't restart services.
static bool gOnlineServicesReady = false;
static void bringUpOnlineServices()
{
  if (gOnlineServicesReady) return;
  gOnlineServicesReady = true;
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
}

// Logs provisioning lifecycle events. Wi-Fi associations also flow through
// here; bringUpOnlineServices() is called on STA_GOT_IP (DHCP success).
static void onWiFiEvent(arduino_event_t *event)
{
  switch (event->event_id) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      bringUpOnlineServices();
      break;
    case ARDUINO_EVENT_PROV_START:
      logf("Locket: BLE provisioning ready — pair with 'ESP BLE Provisioning' app\n");
      break;
    case ARDUINO_EVENT_PROV_CRED_RECV:
      logf("Locket: received credentials for SSID '%s'\n",
           (const char *)event->event_info.prov_cred_recv.ssid);
      break;
    case ARDUINO_EVENT_PROV_CRED_SUCCESS:
      logf("Locket: provisioning successful — credentials saved to NVS\n");
      break;
    case ARDUINO_EVENT_PROV_CRED_FAIL:
      logf("Locket: provisioning failed (bad password or SSID unreachable)\n");
      break;
    default:
      break;
  }
}

} // namespace

void setup()
{
  Serial.begin(115200);
  delay(250);

  // The event handler installs OTA + telnet asynchronously the first time we
  // get a DHCP lease, regardless of which path got us connected (WiFiMulti or
  // BLE provisioning). Register before kicking anything off.
  WiFi.onEvent(onWiFiEvent);

  // Hardcoded networks — additional SSIDs go here.
  wifiMulti().addAP("The Y-Files", "quartz21wrench10crown");
  wifiMulti().addAP("jotunheim", "2217concordcircle");
  uint32_t wifiStart = millis();
  while (wifiMulti().run() != WL_CONNECTED && millis() - wifiStart < 10000) {
    delay(100);
  }
  if (WiFi.status() != WL_CONNECTED) {
    // No known SSID nearby. Fall back to BLE provisioning: device advertises a
    // service named "PROV_locket"; on the phone, install the official
    // "ESP BLE Provisioning" app (Espressif, free on iOS/Android), tap the
    // device, enter the PIN, and pick the network. Provisioned credentials
    // are saved to NVS, so subsequent boots connect without phone interaction.
    static const uint8_t kProvUuid[16] = {
      0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
      0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02
    };
    WiFiProv.beginProvision(
      NETWORK_PROV_SCHEME_BLE,
      NETWORK_PROV_SCHEME_HANDLER_FREE_BLE,
      NETWORK_PROV_SECURITY_1,
      "locket1234",   // PoP / PIN entered in the app
      "PROV_locket",  // BLE service name shown in the app
      nullptr,
      const_cast<uint8_t *>(kProvUuid),
      false           // keep any previously-saved NVS credentials
    );
    logf("Locket: no known WiFi — BLE provisioning service running (PIN: locket1234)\n");
  }
  gAnimationStartUs = esp_timer_get_time();
  gAnimationPausedAtUs = gAnimationStartUs;
  gAnimationPaused = true;
  gLastPausedState = true;

  pinMode(kBootButtonPin, INPUT_PULLUP);

  Wire.begin(IIC_SDA, IIC_SCL);
  Wire.setClock(400000);
  gExpanderReady = initExpanderAndRails();
  initPower();
  initTouch();

  if (!gfx->begin(80000000)) {
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
  buildMoonMasks();
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

  // Make sure the card is mounted (lazy paths above may not have touched
  // it yet), then advertise it to the host as a USB disk. TinyUSB
  // descriptors are fixed at USB.begin(), so MSC can only come up if the
  // card is present at this moment — a later insert still updates media
  // presence via setMscMediaPresent() in recheckSdCard().
  initSdCard(true);
  if (initUsbMsc()) {
    logf("Locket: USB MSC active — SD visible to host\n");
  }
}

void loop()
{
  const uint32_t now = millis();
  // Drive WiFiMulti at 1 Hz so we re-associate (or pick a different SSID from
  // the list) if the current AP drops, instead of giving up forever like the
  // single-shot WiFi.begin() in setup() used to.
  static uint32_t lastWifiTickMs = 0;
  if (now - lastWifiTickMs >= 1000) {
    lastWifiTickMs = now;
    wifiMulti().run();
  }
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
  updateScrollImage();
  if (gPowerReady && now - gLastBatteryPollMs >= kBatteryPollMs) {
    gLastBatteryPollMs = now;
    int pct = gPower.getBatteryPercent();
    uint8_t newPct = static_cast<uint8_t>(constrain(pct, 0, 100));
    if (newPct != gBatteryPercent) {
      logf("Locket: battery %u%%\n", static_cast<unsigned>(newPct));
      gBatteryPercent = newPct;
    }
    const bool pluggedIn = gPower.isVbusIn();
    if (pluggedIn != gIsPluggedIn) {
      gIsPluggedIn = pluggedIn;
      logf("Locket: USB %s\n", pluggedIn ? "connected" : "disconnected");
    }
    const bool actuallyCharging = gPower.isCharging() && gPower.isBatteryConnect();
    if (actuallyCharging != gIsActivelyCharging) {
      gIsActivelyCharging = actuallyCharging;
      logf("Locket: charging %s\n", actuallyCharging ? "started" : "stopped");
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
  // Detect pause state transitions AFTER all input handling so the first rendered frame
  // already has the transition active (prevents a single flash-frame of the new state).
  if (gAnimationPaused != gLastPausedState) {
    gPauseTransitionActive = true;
    gPauseTransitionStartMs = now;
    gLastPausedState = gAnimationPaused;
  }
  if (gPauseTransitionActive && now - gPauseTransitionStartMs >= kMoonTransitionDurationMs) {
    gPauseTransitionActive = false;
  }
  if (now - gLastFrameAtMs < kFrameIntervalMs) {
    delay(1);
    return;
  }

  const float elapsedSeconds = static_cast<float>(now - gLastFrameAtMs) / 1000.0f;
  gLastFrameAtMs = now;

  static uint64_t sPerfWindowStartUs = 0;
  static uint32_t sPerfFrameCount = 0;
  static uint64_t sPerfRenderUsSum = 0, sPerfFlushUsSum = 0, sPerfFrameUsSum = 0;
  static uint32_t sPerfRenderUsMax = 0, sPerfFlushUsMax = 0, sPerfFrameUsMax = 0;
  static uint64_t sPerfSkySum = 0, sPerfStarsSum = 0, sPerfShootSum = 0;
  static uint64_t sPerfCloudsSum = 0, sPerfMoonSum = 0, sPerfOrbSum = 0, sPerfScrollSum = 0;
  static uint32_t sPerfActiveShooters = 0;
  const uint64_t perfFrameStartUs = esp_timer_get_time();
  if (sPerfWindowStartUs == 0) sPerfWindowStartUs = perfFrameStartUs;

  if (!gAnimationPaused) {
    updateStars(elapsedSeconds);
    updateClouds(elapsedSeconds);
  }
  updateShootingStars(elapsedSeconds);  // always — runs even while paused
  updateLightning(millis());            // spawns new bolts while charging, ages active ones
  const uint64_t perfRenderStartUs = esp_timer_get_time();
  renderFrame();
  const uint64_t perfRenderEndUs = esp_timer_get_time();
  if (gUseCanvas) {
    canvas->flush();
  }
  const uint64_t perfFlushEndUs = esp_timer_get_time();

  {
    const uint32_t renderUs = static_cast<uint32_t>(perfRenderEndUs - perfRenderStartUs);
    const uint32_t flushUs  = static_cast<uint32_t>(perfFlushEndUs  - perfRenderEndUs);
    const uint32_t frameUs  = static_cast<uint32_t>(perfFlushEndUs  - perfFrameStartUs);
    sPerfRenderUsSum += renderUs;  if (renderUs > sPerfRenderUsMax) sPerfRenderUsMax = renderUs;
    sPerfFlushUsSum  += flushUs;   if (flushUs  > sPerfFlushUsMax)  sPerfFlushUsMax  = flushUs;
    sPerfFrameUsSum  += frameUs;   if (frameUs  > sPerfFrameUsMax)  sPerfFrameUsMax  = frameUs;
    sPerfSkySum    += gLastFrameTimings.sky;
    sPerfStarsSum  += gLastFrameTimings.stars;
    sPerfShootSum  += gLastFrameTimings.shootingStars;
    sPerfCloudsSum += gLastFrameTimings.clouds;
    sPerfMoonSum   += gLastFrameTimings.moon;
    sPerfOrbSum    += gLastFrameTimings.orb;
    sPerfScrollSum += gLastFrameTimings.scroll;
    uint32_t activeShooters = 0;
    for (size_t i = 0; i < kShootingStarCount; ++i) if (gShootingStars[i].active) ++activeShooters;
    sPerfActiveShooters += activeShooters;
    ++sPerfFrameCount;
  }

  if (perfFlushEndUs - sPerfWindowStartUs >= 1000000ULL && sPerfFrameCount > 0) {
    const uint32_t n = sPerfFrameCount;
    logf("perf fps=%lu frame avg/max=%lu/%lu us  render avg/max=%lu/%lu  flush avg/max=%lu/%lu  "
         "sky=%lu stars=%lu shoot=%lu clouds=%lu moon=%lu orb=%lu scroll=%lu  shooters_avg=%lu\n",
         static_cast<unsigned long>(n),
         static_cast<unsigned long>(sPerfFrameUsSum / n),  static_cast<unsigned long>(sPerfFrameUsMax),
         static_cast<unsigned long>(sPerfRenderUsSum / n), static_cast<unsigned long>(sPerfRenderUsMax),
         static_cast<unsigned long>(sPerfFlushUsSum / n),  static_cast<unsigned long>(sPerfFlushUsMax),
         static_cast<unsigned long>(sPerfSkySum / n),
         static_cast<unsigned long>(sPerfStarsSum / n),
         static_cast<unsigned long>(sPerfShootSum / n),
         static_cast<unsigned long>(sPerfCloudsSum / n),
         static_cast<unsigned long>(sPerfMoonSum / n),
         static_cast<unsigned long>(sPerfOrbSum / n),
         static_cast<unsigned long>(sPerfScrollSum / n),
         static_cast<unsigned long>((sPerfActiveShooters * 10U) / n));
    sPerfWindowStartUs = perfFlushEndUs;
    sPerfFrameCount = 0;
    sPerfRenderUsSum = sPerfFlushUsSum = sPerfFrameUsSum = 0;
    sPerfRenderUsMax = sPerfFlushUsMax = sPerfFrameUsMax = 0;
    sPerfSkySum = sPerfStarsSum = sPerfShootSum = 0;
    sPerfCloudsSum = sPerfMoonSum = sPerfOrbSum = sPerfScrollSum = 0;
    sPerfActiveShooters = 0;
  }
}
