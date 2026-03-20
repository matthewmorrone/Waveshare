#include <Adafruit_XCA9554.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <Arduino_DriveBus_Library.h>
#include <Arduino_GFX_Library.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "pin_config.h"
#include <SD_MMC.h>
#include <SensorPCF85063.hpp>
#include <SensorQMI8658.hpp>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <XPowersLib.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_sleep.h>
#include <lvgl.h>
#include <math.h>
#include <memory>
#include <sys/time.h>
#include <time.h>

#include "ota_config.h"

namespace
{
constexpr uint16_t kBackgroundColor = 0x0000;
constexpr uint8_t kActiveBrightness = 255;
constexpr uint8_t kDimBrightness = 26;
constexpr uint32_t kButtonDebounceMs = 180;
constexpr uint32_t kStatusRefreshMs = 250;
constexpr uint32_t kMotionRefreshMs = 40;
constexpr uint32_t kWeatherAnimRefreshMs = 90;
constexpr uint32_t kWeatherRefreshIntervalMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kWeatherRetryIntervalMs = 60UL * 1000UL;
constexpr uint32_t kWeatherFetchTimeoutMs = 10000;
constexpr uint32_t kWifiConnectTimeoutMs = 12000;
constexpr uint32_t kWifiRetryIntervalMs = 30000;
constexpr uint32_t kRtcWriteIntervalSeconds = 60;
constexpr uint32_t kDimAfterMs = 30000;
constexpr uint32_t kSleepAfterMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kImuSampleIntervalMs = 20;
constexpr uint32_t kTapMaxDurationMs = 350;
constexpr uint32_t kShutdownHoldArmMs = 450;
constexpr uint32_t kPowerButtonShutdownHoldMs = 4000;
constexpr uint32_t kLvglBufferRows = 40;
constexpr time_t kMinValidEpoch = 1704067200;
constexpr const char *kSdMountPath = "/sdcard";
constexpr int kMotionSwipeMinDistancePx = 8;
constexpr int kMotionSwipeMinDominancePx = -6;
constexpr int kMotionViewAnimOffsetPx = 28;
constexpr uint32_t kMotionViewAnimMs = 140;
constexpr float kMotionDeltaThreshold = 0.18f;
constexpr float kMotionSensorSmoothingAlpha = 0.18f;
constexpr float kMotionReadoutSmoothingAlpha = 0.14f;
constexpr float kMotionIndicatorSmoothingAlpha = 0.12f;
constexpr float kMotionIndicatorDeadzoneVector = 0.035f;
constexpr float kMotionIndicatorFullScaleVector = 0.55f;
constexpr int kDotDiameter = 26;
constexpr int kTapMaxTravelPx = 12;
constexpr int kBatteryTrackWidth = 266;
constexpr int kBatteryTrackHeight = 12;
constexpr int kBatteryTrackY = 358;
constexpr int kWatchTimeY = 58;
constexpr int kWatchDateY = 272;
constexpr int kWatchTimezoneY = 306;
constexpr int kClockDigitWidth = 50;
constexpr int kClockColonWidth = 22;
constexpr int kClockGlyphHeight = 148;
constexpr int kClockZoom = 384;
constexpr size_t kScreenCount = 3;
constexpr size_t kWeatherParticleCount = 8;
constexpr size_t kCubeEdgeCount = 12;
constexpr size_t kCubeArrowSegmentCount = 3;
constexpr size_t kCubeArrowCount = 3;
constexpr size_t kCubeArrowLineCount = kCubeArrowCount * kCubeArrowSegmentCount;
constexpr const char *kSdStartupDirs[] = {
    "/assets",
    "/config",
    "/cache",
    "/recordings",
    "/update",
};
constexpr const char *kPreferencesNamespace = "waveform";
constexpr const char *kPrefScreenKey = "screen";
constexpr const char *kPrefMotionViewKey = "motionview";

struct WiFiCredential
{
  const char *ssid;
  const char *password;
};

enum class ConnectivityState : uint8_t
{
  Offline,
  Connecting,
  Online,
};

enum class ScreenId : uint8_t
{
  Watchface,
  Motion,
  Weather,
};

enum class MotionViewMode : uint8_t
{
  Dot,
  Cube,
  Raw,
  Count,
};

enum class WeatherSceneType : uint8_t
{
  Clear,
  PartlyCloudy,
  Cloudy,
  Rain,
  Snow,
  Storm,
  Fog,
};

struct MotionState
{
  bool valid = false;
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  float gx = 0.0f;
  float gy = 0.0f;
  float gz = 0.0f;
  float pitch = 0.0f;
  float roll = 0.0f;
  char orientation[20] = "Unavailable";
};

struct WeatherState
{
  bool hasData = false;
  bool stale = true;
  bool isDay = true;
  int weatherCode = -1;
  int temperatureF = 0;
  int feelsLikeF = 0;
  int highF = 0;
  int lowF = 0;
  int windMph = 0;
  int precipitationPercent = 0;
  int cloudCoverPercent = 0;
  String condition = "Waiting for weather";
  String updated = "Waiting for Wi-Fi";
  String sunrise = "--:--";
  String sunset = "--:--";
  String location = WEATHER_LOCATION_LABEL;
};

struct Vec3
{
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

constexpr WiFiCredential kWiFiCredentials[] = {
    {WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY},
    {WIFI_SSID_FALLBACK, WIFI_PASSWORD_FALLBACK},
};

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

XPowersPMU power;
Adafruit_XCA9554 expander;
SensorPCF85063 rtc;
SensorQMI8658 imu;
std::shared_ptr<Arduino_IIC_DriveBus> touchBus = std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);
Preferences preferences;
void handleTouchInterrupt();
void captureMotionReference();
std::unique_ptr<Arduino_IIC> touchController(
    new Arduino_FT3x68(touchBus, FT3168_DEVICE_ADDRESS, DRIVEBUS_DEFAULT_VALUE, TP_INT, handleTouchInterrupt));

lv_display_t *display = nullptr;
lv_indev_t *touchInput = nullptr;
static uint8_t *lvBuffer = nullptr;

lv_obj_t *screenRoots[kScreenCount] = {nullptr, nullptr, nullptr};
size_t currentScreenIndex = 0;

lv_obj_t *watchTimeLabel = nullptr;
lv_obj_t *watchTimeRow = nullptr;
lv_obj_t *watchTimeGlyphs[5] = {nullptr};
lv_obj_t *watchDateLabel = nullptr;
lv_obj_t *watchTimezoneLabel = nullptr;
lv_obj_t *watchBatteryTrack = nullptr;
lv_obj_t *watchBatteryFill = nullptr;
lv_obj_t *watchWifiIcon = nullptr;
lv_obj_t *watchBluetoothIcon = nullptr;
lv_obj_t *watchUsbIcon = nullptr;
lv_obj_t *watchStatusLabel = nullptr;

lv_obj_t *motionScreen = nullptr;
lv_obj_t *motionTapOverlay = nullptr;
lv_obj_t *motionDotView = nullptr;
lv_obj_t *motionDotBoundary = nullptr;
lv_obj_t *motionDotCrossH = nullptr;
lv_obj_t *motionDotCrossV = nullptr;
lv_obj_t *motionDot = nullptr;
lv_obj_t *motionCubeView = nullptr;
lv_obj_t *motionRawView = nullptr;
lv_obj_t *motionRawCard = nullptr;
lv_obj_t *motionRawOrientation = nullptr;
lv_obj_t *motionRawPitch = nullptr;
lv_obj_t *motionRawRoll = nullptr;
lv_obj_t *motionRawAccel = nullptr;
lv_obj_t *motionRawGyro = nullptr;
lv_obj_t *motionRawPitchCaption = nullptr;
lv_obj_t *motionRawRollCaption = nullptr;
lv_obj_t *motionRawAccelCaption = nullptr;
lv_obj_t *motionRawGyroCaption = nullptr;
lv_obj_t *motionCubeEdges[kCubeEdgeCount] = {nullptr};
lv_obj_t *motionCubeArrows[kCubeArrowLineCount] = {nullptr};
lv_point_precise_t motionCubeEdgePoints[kCubeEdgeCount][2] = {};
lv_point_precise_t motionCubeArrowPoints[kCubeArrowLineCount][2] = {};

lv_obj_t *weatherScreen = nullptr;
lv_obj_t *weatherHero = nullptr;
lv_obj_t *weatherTempLabel = nullptr;
lv_obj_t *weatherConditionLabel = nullptr;
lv_obj_t *weatherPrimaryLabel = nullptr;
lv_obj_t *weatherSecondaryLabel = nullptr;
lv_obj_t *weatherUpdatedValueLabel = nullptr;
lv_obj_t *weatherHintLabel = nullptr;
lv_obj_t *weatherSun = nullptr;
lv_obj_t *weatherMoon = nullptr;
lv_obj_t *weatherClouds[3] = {nullptr};
lv_obj_t *weatherParticles[kWeatherParticleCount] = {nullptr};
lv_obj_t *weatherBolt = nullptr;
lv_obj_t *weatherFogBars[3] = {nullptr};

lv_obj_t *otaOverlay = nullptr;
lv_obj_t *otaStatusLabel = nullptr;
lv_obj_t *otaFooterLabel = nullptr;
lv_obj_t *otaPercentLabel = nullptr;
lv_obj_t *otaBar = nullptr;
lv_obj_t *powerHoldOverlay = nullptr;
lv_obj_t *powerHoldStatusLabel = nullptr;
lv_obj_t *powerHoldFooterLabel = nullptr;
lv_obj_t *powerHoldSecondsLabel = nullptr;
lv_obj_t *powerHoldBar = nullptr;

bool expanderReady = false;
bool powerReady = false;
bool rtcReady = false;
bool touchReady = false;
bool imuReady = false;
bool displayDimmed = false;
bool inLightSleep = false;
bool touchPressed = false;
bool touchGestureConsumed = false;
bool sideButtonPressed = false;
bool sideButtonReleaseArmed = false;
bool powerButtonPressed = false;
bool powerButtonReleaseArmed = false;
bool wifiAttemptInProgress = false;
bool otaReady = false;
bool otaInProgress = false;
bool ntpConfigured = false;
bool weatherFetchInProgress = false;
bool weatherForceRefreshRequested = false;
bool motionReferenceReady = false;
bool motionReferenceCapturePending = false;
bool motionDisplayValid = false;
bool motionFilterReady = false;
bool haveLastAccelSample = false;
bool watchfaceBuilt = false;
bool motionBuilt = false;
bool weatherBuilt = false;
bool sdMounted = false;
bool touchTapTracking = false;
bool powerButtonHoldActive = false;
bool powerButtonShutdownTriggered = false;
uint8_t sdCardType = CARD_NONE;
uint64_t sdCardSizeMb = 0;

ConnectivityState connectivityState = ConnectivityState::Offline;
MotionViewMode motionViewMode = MotionViewMode::Dot;
MotionViewMode renderedMotionViewMode = MotionViewMode::Count;
MotionState motionState;
MotionState motionDisplayState;
WeatherState weatherState;
Vec3 motionReferenceDown = {0.0f, 0.0f, 1.0f};
Vec3 motionReferenceAxisA = {1.0f, 0.0f, 0.0f};
Vec3 motionReferenceAxisB = {0.0f, 1.0f, 0.0f};

float motionPitchZero = 0.0f;
float motionRollZero = 0.0f;
float motionDotPitch = 0.0f;
float motionDotRoll = 0.0f;
float lastAccelX = 0.0f;
float lastAccelY = 0.0f;
float lastAccelZ = 0.0f;

int16_t touchLastX = LCD_WIDTH / 2;
int16_t touchLastY = LCD_HEIGHT / 2;
int renderedMotionTransitionDirection = 0;
size_t configuredNetworkCount = 0;
size_t currentCredentialIndex = 0;
size_t nextCredentialIndex = 0;
uint32_t lastTopButtonEdgeAtMs = 0;
uint32_t lastPowerButtonEdgeAtMs = 0;
uint32_t sideButtonPressedAtMs = 0;
uint32_t powerButtonPressedAtMs = 0;
uint32_t powerButtonHoldStartedAtMs = 0;
uint32_t lastStatusRefreshAtMs = 0;
uint32_t lastMotionRefreshAtMs = 0;
uint32_t lastWeatherAnimAtMs = 0;
uint32_t lastImuSampleAtMs = 0;
uint32_t wifiConnectStartedMs = 0;
uint32_t nextWifiRetryAtMs = 0;
uint32_t nextWeatherRefreshAtMs = 0;
uint32_t lastActivityAtMs = 0;
time_t lastRtcWriteEpoch = 0;
uint32_t touchTapStartedAtMs = 0;

int16_t touchTapStartX = LCD_WIDTH / 2;
int16_t touchTapStartY = LCD_HEIGHT / 2;

lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b)
{
  return lv_color_make(r, g, b);
}

lv_color_t blendColor(uint8_t r, uint8_t g, uint8_t b)
{
  return lv_color_make(r, g, b);
}

void setDisplayBrightness(uint8_t brightness)
{
  gfx->setBrightness(brightness);
}

void noteActivity()
{
  lastActivityAtMs = millis();
  if (displayDimmed) {
    gfx->displayOn();
    setDisplayBrightness(kActiveBrightness);
    displayDimmed = false;
  }
}

void saveUiState()
{
  preferences.putUChar(kPrefScreenKey, static_cast<uint8_t>(currentScreenIndex));
  preferences.putUChar(kPrefMotionViewKey, static_cast<uint8_t>(motionViewMode));
}

void advanceMotionView()
{
  renderedMotionTransitionDirection = -1;
  motionViewMode = static_cast<MotionViewMode>((static_cast<uint8_t>(motionViewMode) + 1) %
                                               static_cast<uint8_t>(MotionViewMode::Count));
  saveUiState();
}

void reverseMotionView()
{
  renderedMotionTransitionDirection = 1;
  motionViewMode = static_cast<MotionViewMode>(
      (static_cast<uint8_t>(motionViewMode) + static_cast<uint8_t>(MotionViewMode::Count) - 1) %
      static_cast<uint8_t>(MotionViewMode::Count));
  saveUiState();
}

void centerMotionDot()
{
  if (!motionDot) {
    return;
  }

  if (motionDotBoundary) {
    lv_obj_align_to(motionDot, motionDotBoundary, LV_ALIGN_CENTER, 0, 0);
    return;
  }

  lv_obj_align(motionDot, LV_ALIGN_CENTER, 0, 0);
}

void handleScreenTap()
{
  ScreenId currentScreen = static_cast<ScreenId>(currentScreenIndex);
  if (currentScreen == ScreenId::Motion) {
    if (motionViewMode == MotionViewMode::Dot) {
      captureMotionReference();
      centerMotionDot();
    }
    noteActivity();
    return;
  }

  if (currentScreen == ScreenId::Weather) {
    weatherForceRefreshRequested = true;
    noteActivity();
  }
}

void handleMotionSwipe(int deltaY)
{
  if (deltaY <= -kMotionSwipeMinDistancePx) {
    advanceMotionView();
    noteActivity();
    return;
  }

  if (deltaY >= kMotionSwipeMinDistancePx) {
    reverseMotionView();
    noteActivity();
  }
}

void handleTouchInterrupt()
{
  if (touchController) {
    touchController->IIC_Interrupt_Flag = true;
  }
}

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

  if (!touchReady) {
    data->state = LV_INDEV_STATE_RELEASED;
    data->point.x = touchLastX;
    data->point.y = touchLastY;
    return;
  }

  bool shouldRead = touchPressed || touchController->IIC_Interrupt_Flag || digitalRead(TP_INT) == LOW;
  if (shouldRead) {
    bool wasPressed = touchPressed;
    int fingers = static_cast<int>(touchController->IIC_Read_Device_Value(
        touchController->Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER));
    bool pressedNow = fingers > 0;

    if (pressedNow) {
      touchLastX = static_cast<int16_t>(touchController->IIC_Read_Device_Value(
          touchController->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X));
      touchLastY = static_cast<int16_t>(touchController->IIC_Read_Device_Value(
          touchController->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y));
      noteActivity();
    }

    if (pressedNow && !wasPressed) {
      touchTapTracking = true;
      touchGestureConsumed = false;
      touchTapStartedAtMs = millis();
      touchTapStartX = touchLastX;
      touchTapStartY = touchLastY;
    } else if (pressedNow && !touchGestureConsumed &&
               static_cast<ScreenId>(currentScreenIndex) == ScreenId::Motion) {
      int deltaX = touchLastX - touchTapStartX;
      int deltaY = touchLastY - touchTapStartY;
      int absDx = abs(deltaX);
      int absDy = abs(deltaY);
      if (absDy >= kMotionSwipeMinDistancePx &&
          absDy >= (absDx + kMotionSwipeMinDominancePx)) {
        handleMotionSwipe(deltaY);
        touchGestureConsumed = true;
        touchTapTracking = false;
      }
    } else if (!pressedNow && wasPressed) {
      int deltaX = touchLastX - touchTapStartX;
      int deltaY = touchLastY - touchTapStartY;
      int absDx = abs(deltaX);
      int absDy = abs(deltaY);
      if (touchGestureConsumed) {
        touchGestureConsumed = false;
      } else if (touchTapTracking) {
        uint32_t tapDurationMs = millis() - touchTapStartedAtMs;
        if (tapDurationMs <= kTapMaxDurationMs && absDx <= kTapMaxTravelPx && absDy <= kTapMaxTravelPx) {
          handleScreenTap();
        }
      } else if (static_cast<ScreenId>(currentScreenIndex) == ScreenId::Motion &&
                 absDy >= kMotionSwipeMinDistancePx &&
                 absDy >= (absDx + kMotionSwipeMinDominancePx)) {
        handleMotionSwipe(deltaY);
      }
      touchTapTracking = false;
    } else if (pressedNow && touchTapTracking) {
      int dx = abs(touchLastX - touchTapStartX);
      int dy = abs(touchLastY - touchTapStartY);
      if (dx > kTapMaxTravelPx || dy > kTapMaxTravelPx) {
        touchTapTracking = false;
      }
    }

    touchPressed = pressedNow;
    touchController->IIC_Interrupt_Flag = false;
  }

  data->point.x = touchLastX;
  data->point.y = touchLastY;
  data->state = touchPressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

bool initTouch()
{
  if (!touchController) {
    return false;
  }

  touchReady = touchController->begin();
  if (!touchReady) {
    Serial.println("Touch controller not found");
    return false;
  }

  touchController->IIC_Write_Device_State(
      touchController->Arduino_IIC_Touch::Device::TOUCH_POWER_MODE,
      touchController->Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_MONITOR);
  touchController->IIC_Interrupt_Flag = false;
  touchPressed = false;
  return true;
}

const char *sdCardTypeLabel(uint8_t cardType)
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

void ensureSdDirectories()
{
  for (const char *dir : kSdStartupDirs) {
    String fullPath = String(kSdMountPath) + dir;
    if (!SD_MMC.exists(fullPath.c_str()) && !SD_MMC.mkdir(fullPath.c_str())) {
      Serial.printf("Failed to create SD directory: %s\n", fullPath.c_str());
    }
  }
}

void initSdCard()
{
  SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
  if (!SD_MMC.begin(kSdMountPath, true)) {
    Serial.println("SD card not mounted");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card detected");
    SD_MMC.end();
    return;
  }

  sdMounted = true;
  sdCardType = cardType;
  sdCardSizeMb = SD_MMC.cardSize() / (1024 * 1024);
  ensureSdDirectories();

  Serial.printf("SD mounted: %s, %llu MB\n", sdCardTypeLabel(sdCardType), sdCardSizeMb);
}

void initExpander()
{
  expanderReady = expander.begin(0x20);
  if (!expanderReady) {
    Serial.println("I2C expander not found");
    return;
  }

  expander.pinMode(PMU_IRQ_PIN, INPUT);
  expander.pinMode(TOP_BUTTON_PIN, INPUT);
  expander.pinMode(1, OUTPUT);
  expander.pinMode(2, OUTPUT);
  expander.digitalWrite(1, LOW);
  expander.digitalWrite(2, LOW);
  delay(20);
  expander.digitalWrite(1, HIGH);
  expander.digitalWrite(2, HIGH);
  sideButtonPressed = expander.digitalRead(TOP_BUTTON_PIN) == HIGH;
  sideButtonReleaseArmed = false;
  sideButtonPressedAtMs = millis();
}

void initPower()
{
  powerReady = power.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (!powerReady) {
    Serial.println("PMU not found");
    return;
  }

  power.enableBattDetection();
  power.enableVbusVoltageMeasure();
  power.enableBattVoltageMeasure();
  power.enableSystemVoltageMeasure();
  power.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  power.clearIrqStatus();
  power.setIrqLevelTime(XPOWERS_AXP2101_IRQ_TIME_1S);
  power.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
  power.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ |
                  XPOWERS_AXP2101_PKEY_LONG_IRQ |
                  XPOWERS_AXP2101_PKEY_NEGATIVE_IRQ |
                  XPOWERS_AXP2101_PKEY_POSITIVE_IRQ);
}

bool rtcTimeLooksValid(const struct tm &timeInfo)
{
  int year = timeInfo.tm_year + 1900;
  return year >= 2024 && year <= 2099 &&
         timeInfo.tm_mon >= 0 && timeInfo.tm_mon <= 11 &&
         timeInfo.tm_mday >= 1 && timeInfo.tm_mday <= 31 &&
         timeInfo.tm_hour >= 0 && timeInfo.tm_hour <= 23 &&
         timeInfo.tm_min >= 0 && timeInfo.tm_min <= 59 &&
         timeInfo.tm_sec >= 0 && timeInfo.tm_sec <= 59;
}

bool setSystemTimeFromTm(const struct tm &timeInfo)
{
  struct tm localTime = timeInfo;
  time_t epoch = mktime(&localTime);
  if (epoch < kMinValidEpoch) {
    return false;
  }

  timeval tv = {
      .tv_sec = epoch,
      .tv_usec = 0,
  };
  return settimeofday(&tv, nullptr) == 0;
}

void loadTimeFromRtc()
{
  if (!rtcReady) {
    return;
  }

  struct tm rtcTime = {};
  rtc.getDateTime(&rtcTime);
  if (!rtcTimeLooksValid(rtcTime)) {
    Serial.println("RTC time is not valid yet");
    return;
  }

  if (setSystemTimeFromTm(rtcTime)) {
    Serial.printf("Loaded time from RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                  rtcTime.tm_year + 1900,
                  rtcTime.tm_mon + 1,
                  rtcTime.tm_mday,
                  rtcTime.tm_hour,
                  rtcTime.tm_min,
                  rtcTime.tm_sec);
  }
}

void initRtc()
{
  rtcReady = rtc.begin(Wire, IIC_SDA, IIC_SCL);
  if (!rtcReady) {
    Serial.println("RTC not found");
    return;
  }

  loadTimeFromRtc();
}

size_t countConfiguredNetworks()
{
  size_t count = 0;
  for (const auto &credential : kWiFiCredentials) {
    if (strlen(credential.ssid) > 0) {
      ++count;
    }
  }
  return count;
}

bool hasConfiguredNetworks()
{
  return configuredNetworkCount > 0;
}

bool hasValidTime()
{
  return time(nullptr) >= kMinValidEpoch;
}

bool batteryIsAvailable()
{
  return powerReady && power.isBatteryConnect();
}

int batteryPercentValue()
{
  if (!batteryIsAvailable()) {
    return -1;
  }

  int percent = power.getBatteryPercent();
  return percent < 0 ? -1 : constrain(percent, 0, 100);
}

bool usbIsConnected()
{
  return powerReady && power.isVbusIn();
}

bool batteryIsCharging()
{
  if (!powerReady || !usbIsConnected()) {
    return false;
  }

  int percent = power.getBatteryPercent();
  if (percent >= 100) {
    return true;
  }

  if (power.isCharging()) {
    return true;
  }

  switch (power.getChargerStatus()) {
    case XPOWERS_AXP2101_CHG_PRE_STATE:
    case XPOWERS_AXP2101_CHG_CC_STATE:
    case XPOWERS_AXP2101_CHG_CV_STATE:
    case XPOWERS_AXP2101_CHG_TRI_STATE:
      return true;
    default:
      return false;
  }
}

String timeText()
{
  if (!hasValidTime()) {
    return "--:--";
  }

  char buffer[16];
  struct tm localTime = {};
  time_t now = time(nullptr);
  localtime_r(&now, &localTime);
  strftime(buffer, sizeof(buffer), "%H:%M", &localTime);
  return String(buffer);
}

String dateText()
{
  if (!hasValidTime()) {
    return "Waiting for RTC or Wi-Fi";
  }

  char buffer[20];
  struct tm localTime = {};
  time_t now = time(nullptr);
  localtime_r(&now, &localTime);
  strftime(buffer, sizeof(buffer), "%a %d %m", &localTime);
  return String(buffer);
}

String timezoneText()
{
  if (!hasValidTime()) {
    return "TIME UNAVAILABLE";
  }

  char buffer[16];
  struct tm localTime = {};
  time_t now = time(nullptr);
  localtime_r(&now, &localTime);
  strftime(buffer, sizeof(buffer), "%Z", &localTime);
  return String(buffer);
}

lv_color_t batteryIndicatorColor(int percent, bool charging)
{
  if (charging) {
    return lvColor(0, 120, 255);
  }

  if (percent < 0) {
    return lvColor(88, 88, 96);
  }

  int clamped = constrain(percent, 0, 100);
  uint8_t red = 0;
  uint8_t green = 0;

  if (clamped <= 50) {
    red = 255;
    green = static_cast<uint8_t>((clamped * 255) / 50);
  } else {
    red = static_cast<uint8_t>(((100 - clamped) * 255) / 50);
    green = 255;
  }

  return lvColor(red, green, 0);
}

void persistTimeToRtc()
{
  if (!rtcReady || !hasValidTime()) {
    return;
  }

  time_t now = time(nullptr);
  if (lastRtcWriteEpoch != 0 && (now - lastRtcWriteEpoch) < kRtcWriteIntervalSeconds) {
    return;
  }

  struct tm localTime = {};
  localtime_r(&now, &localTime);
  rtc.setDateTime(localTime);
  lastRtcWriteEpoch = now;
}

bool networkIsOnline()
{
  return connectivityState == ConnectivityState::Online && WiFi.status() == WL_CONNECTED;
}

String weatherTimeFragment(const char *isoText)
{
  if (isoText == nullptr || isoText[0] == '\0') {
    return "--:--";
  }

  const char *timeFragment = strrchr(isoText, 'T');
  if (timeFragment == nullptr) {
    return String(isoText);
  }

  ++timeFragment;
  char buffer[6] = {'-', '-', ':', '-', '-', '\0'};
  strncpy(buffer, timeFragment, 5);
  buffer[5] = '\0';
  return String(buffer);
}

WeatherSceneType weatherSceneTypeForCode(int weatherCode)
{
  switch (weatherCode) {
    case 0:
      return WeatherSceneType::Clear;
    case 1:
    case 2:
      return WeatherSceneType::PartlyCloudy;
    case 3:
      return WeatherSceneType::Cloudy;
    case 45:
    case 48:
      return WeatherSceneType::Fog;
    case 51:
    case 53:
    case 55:
    case 56:
    case 57:
    case 61:
    case 63:
    case 65:
    case 66:
    case 67:
    case 80:
    case 81:
    case 82:
      return WeatherSceneType::Rain;
    case 71:
    case 73:
    case 75:
    case 77:
    case 85:
    case 86:
      return WeatherSceneType::Snow;
    case 95:
    case 96:
    case 99:
      return WeatherSceneType::Storm;
    default:
      return WeatherSceneType::Cloudy;
  }
}

String weatherConditionFromCode(int weatherCode, bool isDay)
{
  switch (weatherCode) {
    case 0:
      return isDay ? "Clear sky" : "Clear night";
    case 1:
      return isDay ? "Mostly sunny" : "Mostly clear";
    case 2:
      return "Partly cloudy";
    case 3:
      return "Overcast";
    case 45:
    case 48:
      return "Fog";
    case 51:
    case 53:
    case 55:
      return "Drizzle";
    case 56:
    case 57:
      return "Freezing drizzle";
    case 61:
    case 63:
    case 65:
      return "Rain";
    case 66:
    case 67:
      return "Freezing rain";
    case 71:
    case 73:
    case 75:
      return "Snow";
    case 77:
      return "Snow grains";
    case 80:
    case 81:
    case 82:
      return "Rain showers";
    case 85:
    case 86:
      return "Snow showers";
    case 95:
      return "Thunderstorm";
    case 96:
    case 99:
      return "Thunder + hail";
    default:
      return "Weather";
  }
}

String weatherApiUrl()
{
  char url[512];
  snprintf(url,
           sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,apparent_temperature,is_day,weather_code,wind_speed_10m,cloud_cover"
           "&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset,precipitation_probability_max"
           "&temperature_unit=fahrenheit&wind_speed_unit=mph&timezone=auto&forecast_days=1",
           WEATHER_LATITUDE,
           WEATHER_LONGITUDE);
  return String(url);
}

String weatherUpdatedLabel()
{
  if (!hasValidTime()) {
    return "Updated just now";
  }

  char buffer[24];
  struct tm localTime = {};
  time_t now = time(nullptr);
  localtime_r(&now, &localTime);
  strftime(buffer, sizeof(buffer), "Updated %H:%M", &localTime);
  return String(buffer);
}

void setOfflineMode(const char *reason)
{
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
  }

  connectivityState = ConnectivityState::Offline;
  wifiAttemptInProgress = false;
  otaReady = false;
  ntpConfigured = false;

  if (weatherState.hasData) {
    weatherState.stale = true;
    weatherState.updated = "Offline - cached weather";
  }

  Serial.printf("Offline mode: %s\n", reason);
}

void beginWifiAttempt(size_t credentialIndex)
{
  const WiFiCredential &credential = kWiFiCredentials[credentialIndex];

  currentCredentialIndex = credentialIndex;
  wifiAttemptInProgress = true;
  connectivityState = ConnectivityState::Connecting;
  wifiConnectStartedMs = millis();

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname(OTA_HOSTNAME);

  Serial.printf("Connecting to Wi-Fi SSID \"%s\"...\n", credential.ssid);
  WiFi.begin(credential.ssid, credential.password);
}

void beginNextWifiCycle()
{
  if (!hasConfiguredNetworks()) {
    setOfflineMode("no Wi-Fi credentials configured");
    return;
  }

  nextCredentialIndex = 0;
  while (nextCredentialIndex < (sizeof(kWiFiCredentials) / sizeof(kWiFiCredentials[0]))) {
    if (strlen(kWiFiCredentials[nextCredentialIndex].ssid) > 0) {
      beginWifiAttempt(nextCredentialIndex++);
      return;
    }
    ++nextCredentialIndex;
  }

  setOfflineMode("no usable Wi-Fi credentials configured");
}

void requestNtpSync()
{
  if (ntpConfigured || !networkIsOnline()) {
    return;
  }

  configTzTime(TIMEZONE_POSIX, NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);
  ntpConfigured = true;
  Serial.println("Requested NTP sync");
}

void updatePowerHoldOverlay(const String &status, const String &footer, int remainingTenths)
{
  if (!powerHoldOverlay) {
    return;
  }

  lv_obj_clear_flag(powerHoldOverlay, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(powerHoldStatusLabel, status.c_str());
  lv_label_set_text(powerHoldFooterLabel, footer.c_str());
  char countdown[8];
  snprintf(countdown, sizeof(countdown), "%d.%d", remainingTenths / 10, remainingTenths % 10);
  lv_label_set_text(powerHoldSecondsLabel, countdown);
  lv_bar_set_value(powerHoldBar, remainingTenths, LV_ANIM_OFF);
  if (display) {
    lv_timer_handler();
  }
}

void hidePowerHoldOverlay()
{
  if (!powerHoldOverlay) {
    return;
  }

  lv_obj_add_flag(powerHoldOverlay, LV_OBJ_FLAG_HIDDEN);
}

void startPowerButtonHold()
{
  powerButtonHoldActive = true;
  powerButtonShutdownTriggered = false;
  powerButtonHoldStartedAtMs = millis();
  noteActivity();
  updatePowerHoldOverlay("Power Off", "release to cancel", static_cast<int>(kPowerButtonShutdownHoldMs / 100));
}

void cancelPowerButtonHold()
{
  if (!powerButtonHoldActive && !powerButtonShutdownTriggered) {
    return;
  }

  powerButtonHoldActive = false;
  powerButtonShutdownTriggered = false;
  powerButtonHoldStartedAtMs = 0;
  hidePowerHoldOverlay();
  noteActivity();
}

void triggerPowerButtonShutdown()
{
  if (powerButtonShutdownTriggered || !powerReady) {
    return;
  }

  powerButtonShutdownTriggered = true;
  updatePowerHoldOverlay("Power Off", "powering off", 0);
  delay(160);
  power.shutdown();
  delay(200);
  cancelPowerButtonHold();
}

void updatePowerButtonHold()
{
  if (!powerButtonHoldActive || powerButtonShutdownTriggered) {
    return;
  }

  uint32_t elapsedMs = millis() - powerButtonHoldStartedAtMs;
  if (elapsedMs >= kPowerButtonShutdownHoldMs) {
    triggerPowerButtonShutdown();
    return;
  }

  uint32_t remainingMs = kPowerButtonShutdownHoldMs - elapsedMs;
  int remainingTenths = static_cast<int>((remainingMs + 99) / 100);
  updatePowerHoldOverlay("Power Off", "release to cancel", remainingTenths);
}

void updateOtaOverlay(const String &status, const String &footer, int percent)
{
  if (!otaOverlay) {
    return;
  }

  lv_obj_clear_flag(otaOverlay, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(otaStatusLabel, status.c_str());
  lv_label_set_text(otaFooterLabel, footer.c_str());
  String percentText = String(percent) + "%";
  lv_label_set_text(otaPercentLabel, percentText.c_str());
  lv_bar_set_value(otaBar, percent, LV_ANIM_OFF);
  if (display) {
    lv_timer_handler();
  }
}

void hideOtaOverlay()
{
  if (!otaOverlay) {
    return;
  }

  lv_obj_add_flag(otaOverlay, LV_OBJ_FLAG_HIDDEN);
}

void startOta()
{
  if (otaReady || !networkIsOnline()) {
    return;
  }

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  if (strlen(OTA_PASSWORD) > 0) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }

  ArduinoOTA
      .onStart([]() {
        Serial.println("OTA start");
        otaInProgress = true;
        updateOtaOverlay("Receiving firmware", "do not power off", 0);
      })
      .onEnd([]() {
        Serial.println("\nOTA complete");
        updateOtaOverlay("Finishing update", "restarting", 100);
        delay(350);
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        int percent = static_cast<int>((progress * 100U) / total);
        updateOtaOverlay("Receiving firmware", "do not power off", percent);
      })
      .onError([](ota_error_t error) {
        Serial.printf("OTA error[%u]\n", error);
        otaInProgress = false;

        String status = "Update failed";
        switch (error) {
          case OTA_AUTH_ERROR:
            status = "Auth failed";
            break;
          case OTA_BEGIN_ERROR:
            status = "Begin failed";
            break;
          case OTA_CONNECT_ERROR:
            status = "Connect failed";
            break;
          case OTA_RECEIVE_ERROR:
            status = "Receive failed";
            break;
          case OTA_END_ERROR:
            status = "Finalize failed";
            break;
        }
        updateOtaOverlay(status, "returning to app", 0);
      });

  ArduinoOTA.begin();
  otaReady = true;
  Serial.printf("OTA ready at %s.local (%s)\n", OTA_HOSTNAME, WiFi.localIP().toString().c_str());
}

void updateWiFi()
{
  if (!hasConfiguredNetworks()) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (connectivityState != ConnectivityState::Online) {
      connectivityState = ConnectivityState::Online;
      wifiAttemptInProgress = false;
      nextWeatherRefreshAtMs = millis() + 1000;
      Serial.printf("Wi-Fi connected to \"%s\": %s\n",
                    WiFi.SSID().c_str(),
                    WiFi.localIP().toString().c_str());
    }
    requestNtpSync();
    startOta();
    return;
  }

  if (connectivityState == ConnectivityState::Online) {
    setOfflineMode("connection lost; UI continues from local state");
    nextWifiRetryAtMs = millis();
  }

  if (wifiAttemptInProgress) {
    if (millis() - wifiConnectStartedMs < kWifiConnectTimeoutMs) {
      return;
    }

    wifiAttemptInProgress = false;
    Serial.printf("Timed out on \"%s\"\n", kWiFiCredentials[currentCredentialIndex].ssid);

    while (nextCredentialIndex < (sizeof(kWiFiCredentials) / sizeof(kWiFiCredentials[0]))) {
      if (strlen(kWiFiCredentials[nextCredentialIndex].ssid) > 0) {
        beginWifiAttempt(nextCredentialIndex++);
        return;
      }
      ++nextCredentialIndex;
    }

    setOfflineMode("all Wi-Fi networks unavailable; retrying in background");
    nextWifiRetryAtMs = millis() + kWifiRetryIntervalMs;
    return;
  }

  if (static_cast<int32_t>(millis() - nextWifiRetryAtMs) < 0) {
    return;
  }

  beginNextWifiCycle();
}

bool fetchWeather()
{
  if (!networkIsOnline()) {
    return false;
  }

  weatherFetchInProgress = true;
  Serial.println("Fetching weather...");

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(kWeatherFetchTimeoutMs);
  http.setTimeout(kWeatherFetchTimeoutMs);

  bool success = false;
  if (http.begin(secureClient, weatherApiUrl())) {
    int statusCode = http.GET();
    if (statusCode == HTTP_CODE_OK) {
      DynamicJsonDocument doc(8192);
      DeserializationError error = deserializeJson(doc, http.getString());
      if (!error) {
        JsonObject current = doc["current"];
        JsonObject daily = doc["daily"];
        JsonArray highArray = daily["temperature_2m_max"].as<JsonArray>();
        JsonArray lowArray = daily["temperature_2m_min"].as<JsonArray>();
        JsonArray sunriseArray = daily["sunrise"].as<JsonArray>();
        JsonArray sunsetArray = daily["sunset"].as<JsonArray>();
        JsonArray precipitationArray = daily["precipitation_probability_max"].as<JsonArray>();

        float temperature = current["temperature_2m"].as<float>();
        float feelsLike = current["apparent_temperature"].as<float>();
        float wind = current["wind_speed_10m"].as<float>();
        float high = !highArray.isNull() && highArray.size() > 0 ? highArray[0].as<float>() : temperature;
        float low = !lowArray.isNull() && lowArray.size() > 0 ? lowArray[0].as<float>() : temperature;

        weatherState.hasData = true;
        weatherState.stale = false;
        weatherState.isDay = (current["is_day"] | 1) == 1;
        weatherState.weatherCode = current["weather_code"] | -1;
        weatherState.temperatureF = static_cast<int>(roundf(temperature));
        weatherState.feelsLikeF = static_cast<int>(roundf(feelsLike));
        weatherState.highF = static_cast<int>(roundf(high));
        weatherState.lowF = static_cast<int>(roundf(low));
        weatherState.windMph = static_cast<int>(roundf(wind));
        weatherState.precipitationPercent =
            !precipitationArray.isNull() && precipitationArray.size() > 0 ? precipitationArray[0].as<int>() : 0;
        weatherState.cloudCoverPercent = current["cloud_cover"] | 0;
        weatherState.condition = weatherConditionFromCode(weatherState.weatherCode, weatherState.isDay);
        weatherState.updated = weatherUpdatedLabel();
        weatherState.sunrise =
            !sunriseArray.isNull() && sunriseArray.size() > 0 ? weatherTimeFragment(sunriseArray[0].as<const char *>()) : "--:--";
        weatherState.sunset =
            !sunsetArray.isNull() && sunsetArray.size() > 0 ? weatherTimeFragment(sunsetArray[0].as<const char *>()) : "--:--";

        nextWeatherRefreshAtMs = millis() + kWeatherRefreshIntervalMs;
        success = true;
      } else {
        Serial.printf("Weather JSON parse failed: %s\n", error.c_str());
      }
    } else {
      Serial.printf("Weather request failed: HTTP %d\n", statusCode);
    }
    http.end();
  } else {
    Serial.println("Weather request setup failed");
  }

  if (!success) {
    if (weatherState.hasData) {
      weatherState.stale = true;
      weatherState.updated = "Offline - cached weather";
    } else {
      weatherState.hasData = false;
      weatherState.stale = true;
      weatherState.condition = "Weather unavailable";
      weatherState.updated = networkIsOnline() ? "Retrying shortly" : "Waiting for Wi-Fi";
      weatherState.sunrise = "--:--";
      weatherState.sunset = "--:--";
    }
    nextWeatherRefreshAtMs = millis() + kWeatherRetryIntervalMs;
  }

  weatherFetchInProgress = false;
  return success;
}

void updateWeather()
{
  if (!networkIsOnline()) {
    if (weatherState.hasData && !weatherState.stale) {
      weatherState.stale = true;
      weatherState.updated = "Offline - cached weather";
    }
    return;
  }

  if (weatherFetchInProgress || otaInProgress || inLightSleep) {
    return;
  }

  if (weatherForceRefreshRequested) {
    weatherForceRefreshRequested = false;
    fetchWeather();
    return;
  }

  if (static_cast<int32_t>(millis() - nextWeatherRefreshAtMs) >= 0) {
    fetchWeather();
  }
}

Vec3 vec3(float x, float y, float z)
{
  return {x, y, z};
}

Vec3 addVec3(const Vec3 &a, const Vec3 &b)
{
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 subtractVec3(const Vec3 &a, const Vec3 &b)
{
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 scaleVec3(const Vec3 &value, float scalar)
{
  return {value.x * scalar, value.y * scalar, value.z * scalar};
}

float lengthVec3(const Vec3 &value)
{
  return sqrtf(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vec3 normalizeVec3(const Vec3 &value)
{
  float length = lengthVec3(value);
  if (length < 0.0001f) {
    return {0.0f, 0.0f, 0.0f};
  }

  return {value.x / length, value.y / length, value.z / length};
}

Vec3 crossVec3(const Vec3 &a, const Vec3 &b)
{
  return {
      a.y * b.z - a.z * b.y,
      a.z * b.x - a.x * b.z,
      a.x * b.y - a.y * b.x,
  };
}

float dotVec3(const Vec3 &a, const Vec3 &b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 projectOntoPlane(const Vec3 &vector, const Vec3 &normal)
{
  return subtractVec3(vector, scaleVec3(normal, dotVec3(vector, normal)));
}

Vec3 stablePerpendicular(const Vec3 &base, const Vec3 &fallback)
{
  Vec3 perpendicular = normalizeVec3(projectOntoPlane(fallback, base));
  if (lengthVec3(perpendicular) >= 0.1f) {
    return perpendicular;
  }

  Vec3 alternate = fabsf(base.x) < 0.85f ? vec3(1.0f, 0.0f, 0.0f) : vec3(0.0f, 1.0f, 0.0f);
  perpendicular = normalizeVec3(projectOntoPlane(alternate, base));
  if (lengthVec3(perpendicular) >= 0.1f) {
    return perpendicular;
  }

  return {1.0f, 0.0f, 0.0f};
}

Vec3 normalizedDownVector(const MotionState &state)
{
  return normalizeVec3(vec3(state.ax, state.ay, state.az));
}

const char *dominantPhysicalDirection(float ax, float ay, float az)
{
  float absX = fabsf(ax);
  float absY = fabsf(ay);
  float absZ = fabsf(az);

  if (absZ >= absX && absZ >= absY) {
    return az >= 0.0f ? "FACE DOWN" : "FACE UP";
  }

  if (absX >= absY) {
    return ax >= 0.0f ? "USB SIDE" : "LEFT EDGE";
  }

  return ay >= 0.0f ? "TOP EDGE" : "BOTTOM EDGE";
}

void remapImuAxes(float rawAx, float rawAy, float rawAz,
                  float rawGx, float rawGy, float rawGz,
                  float &ax, float &ay, float &az,
                  float &gx, float &gy, float &gz)
{
  // Board orientation: sensor X maps to watch "top", sensor Y maps to watch "right".
  ax = -rawAy;
  ay = -rawAx;
  az = -rawAz;
  gx = -rawGy;
  gy = -rawGx;
  gz = -rawGz;
}

void setOrientationLabel(float ax, float ay, float az)
{
  float absX = fabsf(ax);
  float absY = fabsf(ay);
  float absZ = fabsf(az);

  const char *label = "Moving";
  if (absZ >= absX && absZ >= absY) {
    label = az >= 0.0f ? "Face Up" : "Face Down";
  } else if (absX >= absY) {
    label = ax >= 0.0f ? "Left Up" : "Right Up";
  } else {
    label = ay >= 0.0f ? "Bottom Up" : "Top Up";
  }

  snprintf(motionState.orientation, sizeof(motionState.orientation), "%s", label);
}

bool initImu()
{
  imuReady = imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (!imuReady) {
    Serial.println("QMI8658 not found");
    motionState.valid = false;
    return false;
  }

  if (!imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G,
                               SensorQMI8658::ACC_ODR_125Hz,
                               SensorQMI8658::LPF_MODE_0)) {
    Serial.println("QMI8658 accelerometer configuration failed");
    imuReady = false;
    motionState.valid = false;
    return false;
  }

  imu.configGyroscope(SensorQMI8658::GYR_RANGE_512DPS,
                      SensorQMI8658::GYR_ODR_112_1Hz,
                      SensorQMI8658::LPF_MODE_3);

  if (!imu.enableAccelerometer()) {
    Serial.println("QMI8658 accelerometer enable failed");
    imuReady = false;
    motionState.valid = false;
    return false;
  }

  imu.enableGyroscope();
  pinMode(IMU_IRQ, INPUT_PULLUP);

  motionState.valid = false;
  motionDisplayValid = false;
  motionFilterReady = false;
  haveLastAccelSample = false;
  lastImuSampleAtMs = 0;
  return true;
}

bool configureImuWakeOnMotion()
{
  if (!imuReady) {
    return false;
  }

  if (!imu.configWakeOnMotion(100,
                              SensorQMI8658::ACC_ODR_LOWPOWER_128Hz,
                              SensorQMI8658::INTERRUPT_PIN_2)) {
    Serial.println("QMI8658 wake-on-motion configuration failed");
    return false;
  }

  pinMode(IMU_IRQ, INPUT_PULLUP);
  return true;
}

float blendMotionValue(float current, float target, float alpha)
{
  return current + ((target - current) * alpha);
}

float normalizeMotionIndicatorOffset(float offset)
{
  float magnitude = fabsf(offset);
  if (magnitude <= kMotionIndicatorDeadzoneVector) {
    return 0.0f;
  }

  float adjustedMagnitude = (magnitude - kMotionIndicatorDeadzoneVector) /
                            (kMotionIndicatorFullScaleVector - kMotionIndicatorDeadzoneVector);
  adjustedMagnitude = constrain(adjustedMagnitude, 0.0f, 1.0f);
  return copysignf(adjustedMagnitude, offset);
}

void updateImuState()
{
  if (!imuReady || inLightSleep) {
    return;
  }

  uint32_t now = millis();
  if (now - lastImuSampleAtMs < kImuSampleIntervalMs) {
    return;
  }
  lastImuSampleAtMs = now;

  if (!imu.getDataReady()) {
    return;
  }

  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  if (!imu.getAccelerometer(ax, ay, az)) {
    return;
  }
  float rawAx = ax;
  float rawAy = ay;
  float rawAz = az;

  float gx = 0.0f;
  float gy = 0.0f;
  float gz = 0.0f;
  imu.getGyroscope(gx, gy, gz);
  float rawGx = gx;
  float rawGy = gy;
  float rawGz = gz;

  remapImuAxes(rawAx, rawAy, rawAz,
               rawGx, rawGy, rawGz,
               ax, ay, az,
               gx, gy, gz);

  if (motionState.valid) {
    ax = blendMotionValue(motionState.ax, ax, kMotionSensorSmoothingAlpha);
    ay = blendMotionValue(motionState.ay, ay, kMotionSensorSmoothingAlpha);
    az = blendMotionValue(motionState.az, az, kMotionSensorSmoothingAlpha);
    gx = blendMotionValue(motionState.gx, gx, kMotionSensorSmoothingAlpha);
    gy = blendMotionValue(motionState.gy, gy, kMotionSensorSmoothingAlpha);
    gz = blendMotionValue(motionState.gz, gz, kMotionSensorSmoothingAlpha);
  }

  motionState.valid = true;
  motionState.ax = ax;
  motionState.ay = ay;
  motionState.az = az;
  motionState.gx = gx;
  motionState.gy = gy;
  motionState.gz = gz;
  motionState.pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / PI;
  motionState.roll = atan2f(ay, az) * 180.0f / PI;
  setOrientationLabel(ax, ay, az);

  if (!motionDisplayValid) {
    motionDisplayState = motionState;
    motionDisplayValid = true;
  } else {
    motionDisplayState.ax = blendMotionValue(motionDisplayState.ax, motionState.ax, kMotionReadoutSmoothingAlpha);
    motionDisplayState.ay = blendMotionValue(motionDisplayState.ay, motionState.ay, kMotionReadoutSmoothingAlpha);
    motionDisplayState.az = blendMotionValue(motionDisplayState.az, motionState.az, kMotionReadoutSmoothingAlpha);
    motionDisplayState.gx = blendMotionValue(motionDisplayState.gx, motionState.gx, kMotionReadoutSmoothingAlpha);
    motionDisplayState.gy = blendMotionValue(motionDisplayState.gy, motionState.gy, kMotionReadoutSmoothingAlpha);
    motionDisplayState.gz = blendMotionValue(motionDisplayState.gz, motionState.gz, kMotionReadoutSmoothingAlpha);
    motionDisplayState.pitch = blendMotionValue(motionDisplayState.pitch, motionState.pitch, kMotionReadoutSmoothingAlpha);
    motionDisplayState.roll = blendMotionValue(motionDisplayState.roll, motionState.roll, kMotionReadoutSmoothingAlpha);
    motionDisplayState.valid = true;
    snprintf(motionDisplayState.orientation, sizeof(motionDisplayState.orientation), "%s", motionState.orientation);
  }

  if (!haveLastAccelSample) {
    lastAccelX = rawAx;
    lastAccelY = rawAy;
    lastAccelZ = rawAz;
    haveLastAccelSample = true;
    return;
  }

  float delta = fabsf(rawAx - lastAccelX) + fabsf(rawAy - lastAccelY) + fabsf(rawAz - lastAccelZ);
  lastAccelX = rawAx;
  lastAccelY = rawAy;
  lastAccelZ = rawAz;
  if (delta >= kMotionDeltaThreshold) {
    noteActivity();
  }
}

void captureMotionReference()
{
  motionDotPitch = 0.0f;
  motionDotRoll = 0.0f;
  motionFilterReady = false;
  motionReferenceReady = false;
  motionReferenceCapturePending = true;
}

void motionCubeBasis(const MotionState &state, Vec3 &down, Vec3 &axisA, Vec3 &axisB)
{
  down = normalizedDownVector(state);
  if (!motionReferenceReady) {
    captureMotionReference();
  }

  if (!motionReferenceReady) {
    axisA = vec3(1.0f, 0.0f, 0.0f);
    axisB = vec3(0.0f, 1.0f, 0.0f);
    return;
  }

  axisA = stablePerpendicular(down, motionReferenceAxisA);
  axisB = normalizeVec3(crossVec3(down, axisA));
  if (lengthVec3(axisB) < 0.1f) {
    axisB = stablePerpendicular(down, motionReferenceAxisB);
    axisA = normalizeVec3(crossVec3(axisB, down));
  }
}

void enableWakeOnPinChange(uint8_t pin)
{
  pinMode(pin, INPUT_PULLUP);
  int currentLevel = digitalRead(pin);
  gpio_wakeup_enable(static_cast<gpio_num_t>(pin),
                     currentLevel == HIGH ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
}

void restoreFromLightSleep()
{
  inLightSleep = false;

  gpio_wakeup_disable(static_cast<gpio_num_t>(TP_INT));
  gpio_wakeup_disable(static_cast<gpio_num_t>(IMU_IRQ));

  gfx->displayOn();
  delay(120);
  setDisplayBrightness(kActiveBrightness);
  displayDimmed = false;

  initTouch();
  initImu();
  noteActivity();

  if (hasConfiguredNetworks()) {
    nextWifiRetryAtMs = millis();
  }

  lv_obj_invalidate(lv_screen_active());
}

void enterLightSleep()
{
  if (inLightSleep || otaInProgress || usbIsConnected()) {
    return;
  }

  Serial.println("Entering light sleep");
  inLightSleep = true;

  setOfflineMode("entering light sleep");
  WiFi.mode(WIFI_OFF);

  enableWakeOnPinChange(TP_INT);
  if (configureImuWakeOnMotion()) {
    enableWakeOnPinChange(IMU_IRQ);
  }

  gfx->displayOff();
  setDisplayBrightness(0);
  delay(40);

  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();

  restoreFromLightSleep();
}

void updateIdleState()
{
  if (otaInProgress || inLightSleep) {
    return;
  }

  if (usbIsConnected()) {
    if (displayDimmed) {
      gfx->displayOn();
      setDisplayBrightness(kActiveBrightness);
      displayDimmed = false;
    }
    lastActivityAtMs = millis();
    return;
  }

  uint32_t idleMs = millis() - lastActivityAtMs;
  if (!displayDimmed && idleMs >= kDimAfterMs) {
    setDisplayBrightness(kDimBrightness);
    displayDimmed = true;
  }

  if (idleMs >= kSleepAfterMs) {
    enterLightSleep();
  }
}

void applyRootStyle(lv_obj_t *obj)
{
  lv_obj_set_style_bg_color(obj, lvColor(0, 0, 0), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_outline_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *createCloud(lv_obj_t *parent, lv_coord_t width, lv_coord_t height)
{
  lv_obj_t *cloud = lv_obj_create(parent);
  lv_obj_set_size(cloud, width, height);
  lv_obj_set_style_radius(cloud, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(cloud, lvColor(244, 247, 255), 0);
  lv_obj_set_style_bg_opa(cloud, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(cloud, 0, 0);
  return cloud;
}

void styleLine(lv_obj_t *line, lv_color_t color, int width)
{
  lv_obj_set_style_line_color(line, color, 0);
  lv_obj_set_style_line_width(line, width, 0);
  lv_obj_set_style_line_rounded(line, true, 0);
}

void setObjYOffset(void *obj, int32_t value)
{
  lv_obj_set_y(static_cast<lv_obj_t *>(obj), static_cast<lv_coord_t>(value));
}

lv_obj_t *motionViewObject(MotionViewMode mode)
{
  switch (mode) {
    case MotionViewMode::Dot:
      return motionDotView;
    case MotionViewMode::Cube:
      return motionCubeView;
    case MotionViewMode::Raw:
      return motionRawView;
    default:
      return nullptr;
  }
}

void showMotionViewInstant(MotionViewMode mode)
{
  if (!motionDotView || !motionCubeView || !motionRawView) {
    return;
  }

  lv_obj_add_flag(motionDotView, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(motionCubeView, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(motionRawView, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_y(motionDotView, 0);
  lv_obj_set_y(motionCubeView, 0);
  lv_obj_set_y(motionRawView, 0);

  lv_obj_t *target = motionViewObject(mode);
  if (target) {
    lv_obj_clear_flag(target, LV_OBJ_FLAG_HIDDEN);
  }
  renderedMotionViewMode = mode;
}

void animateMotionViewTo(MotionViewMode mode)
{
  lv_obj_t *target = motionViewObject(mode);
  if (!target) {
    return;
  }

  if (renderedMotionViewMode == MotionViewMode::Count || renderedMotionViewMode == mode) {
    showMotionViewInstant(mode);
    return;
  }

  lv_obj_t *previous = motionViewObject(renderedMotionViewMode);
  if (previous) {
    lv_obj_add_flag(previous, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(previous, 0);
  }

  lv_obj_clear_flag(target, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_y(target, renderedMotionTransitionDirection * kMotionViewAnimOffsetPx);

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, target);
  lv_anim_set_values(&anim, renderedMotionTransitionDirection * kMotionViewAnimOffsetPx, 0);
  lv_anim_set_time(&anim, kMotionViewAnimMs);
  lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&anim, setObjYOffset);
  lv_anim_start(&anim);

  renderedMotionViewMode = mode;
}

void enableTapBubbling(lv_obj_t *obj)
{
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
}

void showScreen(size_t index)
{
  if (index >= kScreenCount || !screenRoots[index]) {
    return;
  }

  currentScreenIndex = index;
  saveUiState();
  lv_screen_load(screenRoots[index]);

  if (static_cast<ScreenId>(index) == ScreenId::Motion) {
    renderedMotionViewMode = MotionViewMode::Count;
    renderedMotionTransitionDirection = 0;
    captureMotionReference();
    centerMotionDot();
  }
}

void showNextScreen()
{
  showScreen((currentScreenIndex + 1) % kScreenCount);
}

void showPreviousScreen()
{
  showScreen((currentScreenIndex + kScreenCount - 1) % kScreenCount);
}

void updateTopButton()
{
  if (!expanderReady) {
    return;
  }

  uint32_t now = millis();
  int rawTopButton = expander.digitalRead(TOP_BUTTON_PIN);
  bool pressed = rawTopButton == HIGH;
  if (pressed) {
    if (!sideButtonPressed) {
      sideButtonPressed = true;
      sideButtonPressedAtMs = now;
      noteActivity();
      if (now - lastTopButtonEdgeAtMs >= kButtonDebounceMs) {
        lastTopButtonEdgeAtMs = now;
        showNextScreen();
      }
    }
    return;
  }

  if (sideButtonPressed) {
    sideButtonPressed = false;
    sideButtonReleaseArmed = false;
  }
}

void updatePowerKey()
{
  if (!expanderReady || !powerReady) {
    return;
  }

  if (expander.digitalRead(PMU_IRQ_PIN) != HIGH) {
    return;
  }

  power.getIrqStatus();
  bool shortPress = power.isPekeyShortPressIrq();
  bool releasedEdge = power.isPekeyNegativeIrq();

  if (shortPress) {
    powerButtonPressed = false;
    bool shouldNavigate = !powerButtonShutdownTriggered &&
                          millis() - lastPowerButtonEdgeAtMs >= kButtonDebounceMs;
    cancelPowerButtonHold();
    if (shouldNavigate) {
      lastPowerButtonEdgeAtMs = millis();
      showPreviousScreen();
    }
    powerButtonReleaseArmed = false;
  }

  if (releasedEdge) {
    powerButtonPressed = false;
    cancelPowerButtonHold();
    powerButtonReleaseArmed = false;
  }

  power.clearIrqStatus();
}

void buildWatchfaceScreen()
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);
  lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *iconRow = lv_obj_create(screen);
  applyRootStyle(iconRow);
  lv_obj_set_size(iconRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_layout(iconRow, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(iconRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(iconRow, 14, 0);
  lv_obj_align(iconRow, LV_ALIGN_TOP_RIGHT, -20, 18);

  watchBluetoothIcon = lv_label_create(iconRow);
  lv_obj_set_style_text_font(watchBluetoothIcon, &lv_font_montserrat_20, 0);
  lv_label_set_text(watchBluetoothIcon, LV_SYMBOL_BLUETOOTH);

  watchWifiIcon = lv_label_create(iconRow);
  lv_obj_set_style_text_font(watchWifiIcon, &lv_font_montserrat_20, 0);
  lv_label_set_text(watchWifiIcon, LV_SYMBOL_WIFI);

  watchUsbIcon = lv_label_create(iconRow);
  lv_obj_set_style_text_font(watchUsbIcon, &lv_font_montserrat_20, 0);
  lv_label_set_text(watchUsbIcon, LV_SYMBOL_CHARGE);

  watchBatteryTrack = lv_obj_create(screen);
  lv_obj_set_size(watchBatteryTrack, kBatteryTrackWidth, kBatteryTrackHeight);
  lv_obj_align(watchBatteryTrack, LV_ALIGN_TOP_MID, 0, kBatteryTrackY);
  lv_obj_set_style_radius(watchBatteryTrack, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(watchBatteryTrack, lvColor(24, 30, 36), 0);
  lv_obj_set_style_border_width(watchBatteryTrack, 1, 0);
  lv_obj_set_style_border_color(watchBatteryTrack, lvColor(46, 54, 64), 0);
  lv_obj_set_style_pad_all(watchBatteryTrack, 0, 0);

  watchBatteryFill = lv_obj_create(watchBatteryTrack);
  lv_obj_set_size(watchBatteryFill, kBatteryTrackWidth, kBatteryTrackHeight);
  lv_obj_set_style_radius(watchBatteryFill, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(watchBatteryFill, 0, 0);
  lv_obj_set_style_pad_all(watchBatteryFill, 0, 0);
  lv_obj_align(watchBatteryFill, LV_ALIGN_LEFT_MID, 0, 0);

  watchTimeRow = lv_obj_create(screen);
  applyRootStyle(watchTimeRow);
  lv_obj_set_size(watchTimeRow, LCD_WIDTH - 4, kClockGlyphHeight);
  lv_obj_set_layout(watchTimeRow, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(watchTimeRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(watchTimeRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(watchTimeRow, 0, 0);
  lv_obj_align(watchTimeRow, LV_ALIGN_TOP_MID, 0, kWatchTimeY);

  for (size_t i = 0; i < 5; ++i) {
    const int glyphWidth = (i == 2) ? kClockColonWidth : kClockDigitWidth;
    watchTimeGlyphs[i] = lv_label_create(watchTimeRow);
    lv_obj_set_size(watchTimeGlyphs[i], glyphWidth, kClockGlyphHeight);
    lv_obj_set_style_text_font(watchTimeGlyphs[i], &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(watchTimeGlyphs[i], lvColor(255, 255, 255), 0);
    lv_obj_set_style_text_align(watchTimeGlyphs[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_transform_zoom(watchTimeGlyphs[i], kClockZoom, 0);
    lv_label_set_text(watchTimeGlyphs[i], (i == 2) ? ":" : "-");
  }

  watchDateLabel = lv_label_create(screen);
  lv_obj_set_width(watchDateLabel, LCD_WIDTH - 36);
  lv_obj_set_style_text_font(watchDateLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(watchDateLabel, lvColor(255, 255, 255), 0);
  lv_obj_set_style_text_align(watchDateLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(watchDateLabel, "Waiting for RTC or Wi-Fi");
  lv_obj_align(watchDateLabel, LV_ALIGN_TOP_MID, 0, kWatchDateY);

  watchTimezoneLabel = lv_label_create(screen);
  lv_obj_set_width(watchTimezoneLabel, LCD_WIDTH - 36);
  lv_obj_set_style_text_font(watchTimezoneLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(watchTimezoneLabel, lvColor(148, 156, 168), 0);
  lv_obj_set_style_text_align(watchTimezoneLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(watchTimezoneLabel, "TIME UNAVAILABLE");
  lv_obj_align(watchTimezoneLabel, LV_ALIGN_TOP_MID, 0, kWatchTimezoneY);

  watchStatusLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(watchStatusLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(watchStatusLabel, lvColor(112, 124, 140), 0);
  lv_obj_set_width(watchStatusLabel, LCD_WIDTH - 64);
  lv_obj_set_style_text_align(watchStatusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(watchStatusLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(watchStatusLabel, "Buttons cycle screens");
  lv_obj_align(watchStatusLabel, LV_ALIGN_BOTTOM_MID, 0, -28);

  screenRoots[static_cast<size_t>(ScreenId::Watchface)] = screen;
  watchfaceBuilt = true;
}

void buildMotionScreen()
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);
  motionScreen = screen;

  motionDotView = lv_obj_create(screen);
  applyRootStyle(motionDotView);
  lv_obj_set_size(motionDotView, LCD_WIDTH, LCD_HEIGHT);
  enableTapBubbling(motionDotView);

  motionDotBoundary = lv_obj_create(motionDotView);
  lv_obj_set_size(motionDotBoundary, LCD_WIDTH - 24, LCD_WIDTH - 24);
  lv_obj_align(motionDotBoundary, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(motionDotBoundary, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(motionDotBoundary, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(motionDotBoundary, 2, 0);
  lv_obj_set_style_border_color(motionDotBoundary, lvColor(52, 58, 66), 0);

  motionDotCrossH = lv_obj_create(motionDotView);
  lv_obj_set_size(motionDotCrossH, LCD_WIDTH - 48, 2);
  lv_obj_align(motionDotCrossH, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(motionDotCrossH, lvColor(38, 44, 52), 0);
  lv_obj_set_style_border_width(motionDotCrossH, 0, 0);

  motionDotCrossV = lv_obj_create(motionDotView);
  lv_obj_set_size(motionDotCrossV, 2, LCD_WIDTH - 48);
  lv_obj_align(motionDotCrossV, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(motionDotCrossV, lvColor(38, 44, 52), 0);
  lv_obj_set_style_border_width(motionDotCrossV, 0, 0);

  motionDot = lv_obj_create(motionDotView);
  lv_obj_set_size(motionDot, kDotDiameter, kDotDiameter);
  lv_obj_set_style_radius(motionDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(motionDot, lvColor(255, 255, 255), 0);
  lv_obj_set_style_border_width(motionDot, 3, 0);
  lv_obj_set_style_border_color(motionDot, lvColor(0, 0, 0), 0);
  lv_obj_align(motionDot, LV_ALIGN_CENTER, 0, 0);

  motionCubeView = lv_obj_create(screen);
  applyRootStyle(motionCubeView);
  lv_obj_set_size(motionCubeView, LCD_WIDTH, LCD_HEIGHT);
  enableTapBubbling(motionCubeView);

  for (size_t i = 0; i < kCubeEdgeCount; ++i) {
    motionCubeEdges[i] = lv_line_create(motionCubeView);
    lv_obj_set_size(motionCubeEdges[i], LCD_WIDTH, LCD_HEIGHT);
    styleLine(motionCubeEdges[i], lvColor(88, 96, 110), 2);
    lv_line_set_points(motionCubeEdges[i], motionCubeEdgePoints[i], 2);
  }

  lv_color_t arrowColors[kCubeArrowCount] = {
      lvColor(46, 142, 255),
      lvColor(255, 255, 255),
      lvColor(140, 154, 176),
  };
  for (size_t i = 0; i < kCubeArrowLineCount; ++i) {
    motionCubeArrows[i] = lv_line_create(motionCubeView);
    lv_obj_set_size(motionCubeArrows[i], LCD_WIDTH, LCD_HEIGHT);
    styleLine(motionCubeArrows[i], arrowColors[i / kCubeArrowSegmentCount], 3);
    lv_line_set_points(motionCubeArrows[i], motionCubeArrowPoints[i], 2);
  }

  motionRawView = lv_obj_create(screen);
  applyRootStyle(motionRawView);
  lv_obj_set_size(motionRawView, LCD_WIDTH, LCD_HEIGHT);
  enableTapBubbling(motionRawView);

  motionRawCard = lv_obj_create(motionRawView);
  lv_obj_set_size(motionRawCard, LCD_WIDTH - 36, LCD_HEIGHT - 68);
  lv_obj_align(motionRawCard, LV_ALIGN_CENTER, 0, 6);
  lv_obj_set_style_radius(motionRawCard, 26, 0);
  lv_obj_set_style_bg_color(motionRawCard, lvColor(10, 14, 20), 0);
  lv_obj_set_style_bg_opa(motionRawCard, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(motionRawCard, 1, 0);
  lv_obj_set_style_border_color(motionRawCard, lvColor(28, 36, 48), 0);
  lv_obj_set_style_outline_width(motionRawCard, 0, 0);
  lv_obj_set_style_pad_all(motionRawCard, 0, 0);
  lv_obj_clear_flag(motionRawCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(motionRawCard, LV_SCROLLBAR_MODE_OFF);

  motionRawOrientation = lv_label_create(motionRawCard);
  lv_obj_set_width(motionRawOrientation, LCD_WIDTH - 76);
  lv_obj_set_style_text_font(motionRawOrientation, &lv_font_montserrat_36, 0);
  lv_obj_set_style_text_color(motionRawOrientation, lvColor(250, 252, 255), 0);
  lv_obj_set_style_text_align(motionRawOrientation, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(motionRawOrientation, "Unavailable");
  lv_obj_align(motionRawOrientation, LV_ALIGN_TOP_MID, 0, 26);

  motionRawPitchCaption = lv_label_create(motionRawCard);
  lv_obj_set_style_text_font(motionRawPitchCaption, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(motionRawPitchCaption, lvColor(110, 126, 146), 0);
  lv_label_set_text(motionRawPitchCaption, "PITCH");
  lv_obj_align(motionRawPitchCaption, LV_ALIGN_TOP_LEFT, 28, 96);

  motionRawRollCaption = lv_label_create(motionRawCard);
  lv_obj_set_style_text_font(motionRawRollCaption, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(motionRawRollCaption, lvColor(110, 126, 146), 0);
  lv_label_set_text(motionRawRollCaption, "ROLL");
  lv_obj_align(motionRawRollCaption, LV_ALIGN_TOP_RIGHT, -28, 96);

  motionRawPitch = lv_label_create(motionRawCard);
  lv_obj_set_width(motionRawPitch, 112);
  lv_obj_set_style_text_font(motionRawPitch, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(motionRawPitch, lvColor(148, 214, 255), 0);
  lv_obj_set_style_text_align(motionRawPitch, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_text(motionRawPitch, "--.-");
  lv_obj_align_to(motionRawPitch, motionRawPitchCaption, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);

  motionRawRoll = lv_label_create(motionRawCard);
  lv_obj_set_width(motionRawRoll, 112);
  lv_obj_set_style_text_font(motionRawRoll, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(motionRawRoll, lvColor(244, 246, 250), 0);
  lv_obj_set_style_text_align(motionRawRoll, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text(motionRawRoll, "--.-");
  lv_obj_align_to(motionRawRoll, motionRawRollCaption, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 6);

  motionRawAccelCaption = lv_label_create(motionRawCard);
  lv_obj_set_style_text_font(motionRawAccelCaption, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(motionRawAccelCaption, lvColor(110, 126, 146), 0);
  lv_label_set_text(motionRawAccelCaption, "ACCELEROMETER");
  lv_obj_align(motionRawAccelCaption, LV_ALIGN_TOP_LEFT, 28, 176);

  motionRawAccel = lv_label_create(motionRawCard);
  lv_obj_set_width(motionRawAccel, 124);
  lv_obj_set_style_text_font(motionRawAccel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(motionRawAccel, lvColor(214, 220, 230), 0);
  lv_obj_set_style_text_line_space(motionRawAccel, 8, 0);
  lv_obj_set_style_text_align(motionRawAccel, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_long_mode(motionRawAccel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(motionRawAccel, "X  --.-\nY  --.-\nZ  --.-");
  lv_obj_align_to(motionRawAccel, motionRawAccelCaption, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);

  motionRawGyroCaption = lv_label_create(motionRawCard);
  lv_obj_set_style_text_font(motionRawGyroCaption, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(motionRawGyroCaption, lvColor(110, 126, 146), 0);
  lv_label_set_text(motionRawGyroCaption, "GYROSCOPE");
  lv_obj_align(motionRawGyroCaption, LV_ALIGN_TOP_RIGHT, -28, 176);

  motionRawGyro = lv_label_create(motionRawCard);
  lv_obj_set_width(motionRawGyro, 124);
  lv_obj_set_style_text_font(motionRawGyro, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(motionRawGyro, lvColor(160, 194, 255), 0);
  lv_obj_set_style_text_line_space(motionRawGyro, 8, 0);
  lv_obj_set_style_text_align(motionRawGyro, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(motionRawGyro, LV_LABEL_LONG_WRAP);
  lv_label_set_text(motionRawGyro, "X  ---\nY  ---\nZ  ---");
  lv_obj_align_to(motionRawGyro, motionRawGyroCaption, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 10);

  motionTapOverlay = lv_obj_create(screen);
  lv_obj_set_size(motionTapOverlay, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_style_bg_opa(motionTapOverlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(motionTapOverlay, 0, 0);
  lv_obj_set_style_outline_width(motionTapOverlay, 0, 0);
  lv_obj_set_style_pad_all(motionTapOverlay, 0, 0);
  lv_obj_clear_flag(motionTapOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(motionTapOverlay);

  screenRoots[static_cast<size_t>(ScreenId::Motion)] = screen;
  motionBuilt = true;
}

void buildWeatherScreen()
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);
  weatherScreen = screen;

  weatherHero = lv_obj_create(screen);
  lv_obj_set_size(weatherHero, LCD_WIDTH, 224);
  lv_obj_align(weatherHero, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_border_width(weatherHero, 0, 0);
  lv_obj_set_style_pad_all(weatherHero, 0, 0);
  lv_obj_set_style_radius(weatherHero, 0, 0);

  weatherSun = lv_obj_create(weatherHero);
  lv_obj_set_size(weatherSun, 74, 74);
  lv_obj_set_style_radius(weatherSun, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(weatherSun, lvColor(255, 196, 72), 0);
  lv_obj_set_style_border_width(weatherSun, 0, 0);

  weatherMoon = lv_obj_create(weatherHero);
  lv_obj_set_size(weatherMoon, 66, 66);
  lv_obj_set_style_radius(weatherMoon, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(weatherMoon, lvColor(210, 220, 255), 0);
  lv_obj_set_style_border_width(weatherMoon, 0, 0);

  weatherClouds[0] = createCloud(weatherHero, 126, 44);
  weatherClouds[1] = createCloud(weatherHero, 106, 38);
  weatherClouds[2] = createCloud(weatherHero, 144, 48);

  for (size_t i = 0; i < kWeatherParticleCount; ++i) {
    weatherParticles[i] = lv_obj_create(weatherHero);
    lv_obj_set_size(weatherParticles[i], 4, 18);
    lv_obj_set_style_radius(weatherParticles[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(weatherParticles[i], 0, 0);
    lv_obj_set_style_bg_color(weatherParticles[i], lvColor(144, 194, 255), 0);
  }

  weatherBolt = lv_label_create(weatherHero);
  lv_obj_set_style_text_font(weatherBolt, &lv_font_montserrat_48, 0);
  lv_label_set_text(weatherBolt, LV_SYMBOL_CHARGE);
  lv_obj_set_style_text_color(weatherBolt, lvColor(255, 220, 84), 0);

  for (int i = 0; i < 3; ++i) {
    weatherFogBars[i] = lv_obj_create(weatherHero);
    lv_obj_set_size(weatherFogBars[i], 220, 8);
    lv_obj_set_style_radius(weatherFogBars[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(weatherFogBars[i], lvColor(214, 222, 232), 0);
    lv_obj_set_style_bg_opa(weatherFogBars[i], LV_OPA_70, 0);
    lv_obj_set_style_border_width(weatherFogBars[i], 0, 0);
  }

  weatherTempLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(weatherTempLabel, &lv_font_montserrat_48, 0);
  lv_label_set_text(weatherTempLabel, "--");
  lv_obj_align(weatherTempLabel, LV_ALIGN_TOP_LEFT, 24, 248);

  weatherConditionLabel = lv_label_create(screen);
  lv_obj_set_width(weatherConditionLabel, LCD_WIDTH - 48);
  lv_obj_set_style_text_font(weatherConditionLabel, &lv_font_montserrat_20, 0);
  lv_label_set_long_mode(weatherConditionLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(weatherConditionLabel, "Waiting for weather");
  lv_obj_align_to(weatherConditionLabel, weatherTempLabel, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);

  weatherPrimaryLabel = lv_label_create(screen);
  lv_obj_set_width(weatherPrimaryLabel, LCD_WIDTH - 48);
  lv_obj_set_style_text_font(weatherPrimaryLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(weatherPrimaryLabel, lvColor(208, 214, 226), 0);
  lv_label_set_long_mode(weatherPrimaryLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(weatherPrimaryLabel, "");
  lv_obj_align_to(weatherPrimaryLabel, weatherConditionLabel, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 14);

  weatherSecondaryLabel = lv_label_create(screen);
  lv_obj_set_width(weatherSecondaryLabel, LCD_WIDTH - 48);
  lv_obj_set_style_text_font(weatherSecondaryLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(weatherSecondaryLabel, lvColor(168, 178, 192), 0);
  lv_label_set_long_mode(weatherSecondaryLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(weatherSecondaryLabel, "");
  lv_obj_align_to(weatherSecondaryLabel, weatherPrimaryLabel, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);

  weatherUpdatedValueLabel = lv_label_create(screen);
  lv_obj_set_width(weatherUpdatedValueLabel, LCD_WIDTH - 48);
  lv_obj_set_style_text_font(weatherUpdatedValueLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(weatherUpdatedValueLabel, lvColor(116, 126, 140), 0);
  lv_label_set_long_mode(weatherUpdatedValueLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(weatherUpdatedValueLabel, "Waiting for Wi-Fi");
  lv_obj_align(weatherUpdatedValueLabel, LV_ALIGN_BOTTOM_LEFT, 24, -42);

  weatherHintLabel = lv_label_create(screen);
  lv_obj_set_width(weatherHintLabel, LCD_WIDTH - 48);
  lv_obj_set_style_text_font(weatherHintLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(weatherHintLabel, lvColor(88, 104, 124), 0);
  lv_obj_set_style_text_align(weatherHintLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(weatherHintLabel, "Tap to refresh");
  lv_obj_align(weatherHintLabel, LV_ALIGN_BOTTOM_MID, 0, -14);

  screenRoots[static_cast<size_t>(ScreenId::Weather)] = screen;
  weatherBuilt = true;
}

void buildOtaOverlay()
{
  otaOverlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(otaOverlay, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_style_bg_color(otaOverlay, lvColor(0, 0, 0), 0);
  lv_obj_set_style_bg_opa(otaOverlay, LV_OPA_90, 0);
  lv_obj_set_style_border_width(otaOverlay, 0, 0);
  lv_obj_clear_flag(otaOverlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *card = lv_obj_create(otaOverlay);
  lv_obj_set_size(card, 280, 212);
  lv_obj_center(card);
  lv_obj_set_style_radius(card, 24, 0);
  lv_obj_set_style_bg_color(card, lvColor(8, 12, 18), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lvColor(34, 60, 96), 0);
  lv_obj_set_style_pad_all(card, 18, 0);

  lv_obj_t *title = lv_label_create(card);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_label_set_text(title, "Firmware Update");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

  otaStatusLabel = lv_label_create(card);
  lv_obj_set_width(otaStatusLabel, 244);
  lv_obj_set_style_text_font(otaStatusLabel, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_align(otaStatusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(otaStatusLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(otaStatusLabel, "Ready");
  lv_obj_align(otaStatusLabel, LV_ALIGN_TOP_MID, 0, 48);

  otaPercentLabel = lv_label_create(card);
  lv_obj_set_style_text_font(otaPercentLabel, &lv_font_montserrat_48, 0);
  lv_label_set_text(otaPercentLabel, "0%");
  lv_obj_align(otaPercentLabel, LV_ALIGN_CENTER, 0, -8);

  otaBar = lv_bar_create(card);
  lv_obj_set_size(otaBar, 224, 10);
  lv_obj_align(otaBar, LV_ALIGN_CENTER, 0, 44);
  lv_bar_set_range(otaBar, 0, 100);
  lv_bar_set_value(otaBar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(otaBar, lvColor(28, 34, 40), 0);
  lv_obj_set_style_bg_color(otaBar, lvColor(52, 132, 255), LV_PART_INDICATOR);
  lv_obj_set_style_radius(otaBar, LV_RADIUS_CIRCLE, 0);

  otaFooterLabel = lv_label_create(card);
  lv_obj_set_width(otaFooterLabel, 244);
  lv_obj_set_style_text_font(otaFooterLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(otaFooterLabel, lvColor(168, 178, 194), 0);
  lv_obj_set_style_text_align(otaFooterLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(otaFooterLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(otaFooterLabel, "do not power off");
  lv_obj_align(otaFooterLabel, LV_ALIGN_BOTTOM_MID, 0, -6);

  hideOtaOverlay();
}

void buildPowerHoldOverlay()
{
  powerHoldOverlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(powerHoldOverlay, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_style_bg_color(powerHoldOverlay, lvColor(0, 0, 0), 0);
  lv_obj_set_style_bg_opa(powerHoldOverlay, LV_OPA_90, 0);
  lv_obj_set_style_border_width(powerHoldOverlay, 0, 0);
  lv_obj_clear_flag(powerHoldOverlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *card = lv_obj_create(powerHoldOverlay);
  lv_obj_set_size(card, 292, 228);
  lv_obj_center(card);
  lv_obj_set_style_radius(card, 24, 0);
  lv_obj_set_style_bg_color(card, lvColor(8, 12, 18), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lvColor(58, 68, 82), 0);
  lv_obj_set_style_pad_all(card, 18, 0);

  lv_obj_t *title = lv_label_create(card);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_label_set_text(title, "Power Off");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

  powerHoldStatusLabel = lv_label_create(card);
  lv_obj_set_width(powerHoldStatusLabel, 248);
  lv_obj_set_style_text_font(powerHoldStatusLabel, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_align(powerHoldStatusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(powerHoldStatusLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(powerHoldStatusLabel, "Power Off");
  lv_obj_align(powerHoldStatusLabel, LV_ALIGN_TOP_MID, 0, 48);

  powerHoldSecondsLabel = lv_label_create(card);
  lv_obj_set_style_text_font(powerHoldSecondsLabel, &lv_font_montserrat_48, 0);
  lv_label_set_text(powerHoldSecondsLabel, "4.0");
  lv_obj_align(powerHoldSecondsLabel, LV_ALIGN_CENTER, 0, -8);

  powerHoldBar = lv_bar_create(card);
  lv_obj_set_size(powerHoldBar, 224, 10);
  lv_obj_align(powerHoldBar, LV_ALIGN_CENTER, 0, 44);
  lv_bar_set_range(powerHoldBar, 0, static_cast<int>(kPowerButtonShutdownHoldMs / 100));
  lv_bar_set_value(powerHoldBar, static_cast<int>(kPowerButtonShutdownHoldMs / 100), LV_ANIM_OFF);
  lv_obj_set_style_bg_color(powerHoldBar, lvColor(28, 34, 40), 0);
  lv_obj_set_style_bg_color(powerHoldBar, lvColor(255, 255, 255), LV_PART_INDICATOR);
  lv_obj_set_style_radius(powerHoldBar, LV_RADIUS_CIRCLE, 0);

  powerHoldFooterLabel = lv_label_create(card);
  lv_obj_set_width(powerHoldFooterLabel, 248);
  lv_obj_set_style_text_font(powerHoldFooterLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(powerHoldFooterLabel, lvColor(168, 178, 194), 0);
  lv_obj_set_style_text_align(powerHoldFooterLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(powerHoldFooterLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(powerHoldFooterLabel, "release to cancel");
  lv_obj_align(powerHoldFooterLabel, LV_ALIGN_BOTTOM_MID, 0, -6);

  hidePowerHoldOverlay();
}

void buildUi()
{
  buildWatchfaceScreen();
  buildMotionScreen();
  buildWeatherScreen();
  buildOtaOverlay();
  buildPowerHoldOverlay();
}

void refreshWatchface()
{
  if (!watchfaceBuilt) {
    return;
  }

  String renderedTime = timeText();
  if (renderedTime.length() != 5) {
    renderedTime = "--:--";
  }
  for (size_t i = 0; i < 5; ++i) {
    char glyph[2] = {renderedTime[static_cast<int>(i)], '\0'};
    lv_label_set_text(watchTimeGlyphs[i], glyph);
  }
  lv_label_set_text(watchDateLabel, dateText().c_str());
  lv_label_set_text(watchTimezoneLabel, timezoneText().c_str());

  int percent = batteryPercentValue();
  int fillWidth = 0;
  if (percent < 0) {
    fillWidth = kBatteryTrackWidth / 3;
  } else {
    fillWidth = max(10, (kBatteryTrackWidth * percent) / 100);
  }

  lv_obj_set_width(watchBatteryFill, fillWidth);
  lv_obj_set_style_bg_color(watchBatteryFill, batteryIndicatorColor(percent, batteryIsCharging()), 0);

  lv_obj_set_style_text_color(watchWifiIcon,
                              networkIsOnline() ? lvColor(255, 255, 255) : lvColor(72, 82, 96),
                              0);
  lv_obj_set_style_text_color(watchBluetoothIcon, lvColor(72, 82, 96), 0);
  lv_obj_set_style_text_color(watchUsbIcon,
                              usbIsConnected() ? (batteryIsCharging() ? lvColor(0, 120, 255) : lvColor(255, 255, 255))
                                               : lvColor(72, 82, 96),
                              0);

  String status;
  if (connectivityState == ConnectivityState::Connecting) {
    status = "Connecting to Wi-Fi";
  } else if (networkIsOnline()) {
    status = WiFi.SSID() + "  " + WiFi.localIP().toString();
  } else {
    status = "Offline-first mode";
  }

  if (usbIsConnected()) {
    status += "  •  USB power";
  }

  if (expanderReady) {
    status += "  •  B4:";
    status += (expander.digitalRead(TOP_BUTTON_PIN) == HIGH) ? "H" : "L";
  }

  lv_label_set_text(watchStatusLabel, status.c_str());
}

void setMotionViewVisibility()
{
  if (renderedMotionViewMode != motionViewMode) {
    showMotionViewInstant(motionViewMode);
  }
}

void updateMotionDot()
{
  if (!motionState.valid) {
    return;
  }

  if (!motionReferenceReady) {
    if (!motionReferenceCapturePending) {
      captureMotionReference();
    }
  }

  const MotionState &displayState = motionState;
  Vec3 currentDown = normalizedDownVector(displayState);

  lv_area_t boundary = {};
  lv_obj_get_coords(motionDotBoundary, &boundary);
  int centerX = (boundary.x1 + boundary.x2) / 2;
  int centerY = (boundary.y1 + boundary.y2) / 2;

  if (motionReferenceCapturePending) {
    motionPitchZero = displayState.pitch;
    motionRollZero = displayState.roll;
    motionReferenceDown = currentDown;
    motionReferenceAxisA = stablePerpendicular(motionReferenceDown, vec3(1.0f, 0.0f, 0.0f));
    motionReferenceAxisB = normalizeVec3(crossVec3(motionReferenceDown, motionReferenceAxisA));
    if (lengthVec3(motionReferenceAxisB) < 0.1f) {
      motionReferenceAxisB = stablePerpendicular(motionReferenceDown, vec3(0.0f, 1.0f, 0.0f));
    }
    motionReferenceReady = true;
    motionReferenceCapturePending = false;
    motionDotPitch = 0.0f;
    motionDotRoll = 0.0f;
    motionFilterReady = true;
    lv_obj_set_pos(motionDot, centerX - (kDotDiameter / 2), centerY - (kDotDiameter / 2));
    return;
  }

  if (!motionReferenceReady) {
    return;
  }

  Vec3 downDelta = subtractVec3(currentDown, motionReferenceDown);
  float horizontalOffset = dotVec3(downDelta, motionReferenceAxisA);
  float verticalOffset = dotVec3(downDelta, motionReferenceAxisB);

  if (!motionFilterReady) {
    motionDotPitch = verticalOffset;
    motionDotRoll = horizontalOffset;
    motionFilterReady = true;
  } else {
    motionDotPitch += (verticalOffset - motionDotPitch) * kMotionIndicatorSmoothingAlpha;
    motionDotRoll += (horizontalOffset - motionDotRoll) * kMotionIndicatorSmoothingAlpha;
  }

  int travel = ((boundary.x2 - boundary.x1) / 2) - 22;
  float normalizedX = normalizeMotionIndicatorOffset(motionDotRoll);
  float normalizedY = normalizeMotionIndicatorOffset(motionDotPitch);
  int x = centerX + static_cast<int>(normalizedX * travel) - (kDotDiameter / 2);
  int y = centerY - static_cast<int>(normalizedY * travel) - (kDotDiameter / 2);
  x = constrain(x, 0, LCD_WIDTH - kDotDiameter);
  y = constrain(y, 0, LCD_HEIGHT - kDotDiameter);
  lv_obj_set_pos(motionDot, x, y);

}

void updateMotionRaw()
{
  if (!motionDisplayState.valid) {
    lv_label_set_text(motionRawOrientation, "Unavailable");
    lv_label_set_text(motionRawPitch, "--.-");
    lv_label_set_text(motionRawRoll, "--.-");
    lv_label_set_text(motionRawAccel, "X  --.-\nY  --.-\nZ  --.-");
    lv_label_set_text(motionRawGyro, "X  ---\nY  ---\nZ  ---");
    return;
  }

  char line[80];
  lv_label_set_text(motionRawOrientation, motionDisplayState.orientation);

  snprintf(line, sizeof(line), "%+.0f°", motionDisplayState.pitch);
  lv_label_set_text(motionRawPitch, line);
  snprintf(line, sizeof(line), "%+.0f°", motionDisplayState.roll);
  lv_label_set_text(motionRawRoll, line);
  snprintf(line,
           sizeof(line),
           "X  %+0.1f\nY  %+0.1f\nZ  %+0.1f",
           motionDisplayState.ax,
           motionDisplayState.ay,
           motionDisplayState.az);
  lv_label_set_text(motionRawAccel, line);
  snprintf(line,
           sizeof(line),
           "X  %+0.0f\nY  %+0.0f\nZ  %+0.0f",
           motionDisplayState.gx,
           motionDisplayState.gy,
           motionDisplayState.gz);
  lv_label_set_text(motionRawGyro, line);
}

void setLinePoints(lv_point_precise_t points[2], float x1, float y1, float x2, float y2)
{
  points[0].x = x1;
  points[0].y = y1;
  points[1].x = x2;
  points[1].y = y2;
}

void updateMotionCube()
{
  if (!motionDisplayState.valid) {
    return;
  }

  Vec3 down;
  Vec3 axisA;
  Vec3 axisB;
  motionCubeBasis(motionDisplayState, down, axisA, axisB);

  constexpr Vec3 corners[8] = {
      {-1.0f, -1.0f, -1.0f},
      {1.0f, -1.0f, -1.0f},
      {1.0f, 1.0f, -1.0f},
      {-1.0f, 1.0f, -1.0f},
      {-1.0f, -1.0f, 1.0f},
      {1.0f, -1.0f, 1.0f},
      {1.0f, 1.0f, 1.0f},
      {-1.0f, 1.0f, 1.0f},
  };

  const uint8_t edges[kCubeEdgeCount][2] = {
      {0, 1}, {1, 2}, {2, 3}, {3, 0},
      {4, 5}, {5, 6}, {6, 7}, {7, 4},
      {0, 4}, {1, 5}, {2, 6}, {3, 7},
  };

  lv_point_precise_t projected[8];
  int centerX = LCD_WIDTH / 2;
  int centerY = LCD_HEIGHT / 2;
  float scale = 78.0f;

  for (size_t i = 0; i < 8; ++i) {
    const Vec3 &corner = corners[i];
    float px = (corner.x * axisA.x + corner.y * axisB.x + corner.z * down.x);
    float py = (corner.x * axisA.y + corner.y * axisB.y + corner.z * down.y);
    float pz = (corner.x * axisA.z + corner.y * axisB.z + corner.z * down.z);
    float perspective = 1.0f + (pz * 0.28f);
    projected[i].x = centerX + (px * scale * perspective);
    projected[i].y = centerY - (py * scale * perspective);
  }

  for (size_t i = 0; i < kCubeEdgeCount; ++i) {
    setLinePoints(motionCubeEdgePoints[i],
                  projected[edges[i][0]].x,
                  projected[edges[i][0]].y,
                  projected[edges[i][1]].x,
                  projected[edges[i][1]].y);
    lv_obj_invalidate(motionCubeEdges[i]);
  }

  Vec3 arrows[kCubeArrowCount] = {scaleVec3(down, -1.0f), axisA, axisB};
  for (size_t i = 0; i < kCubeArrowCount; ++i) {
    Vec3 d = normalizeVec3(arrows[i]);
    float tipX = centerX + (d.x * 112.0f);
    float tipY = centerY - (d.y * 112.0f);
    float leftX = tipX - (d.x * 14.0f) - (d.y * 9.0f);
    float leftY = tipY + (d.y * 14.0f) - (d.x * 9.0f);
    float rightX = tipX - (d.x * 14.0f) + (d.y * 9.0f);
    float rightY = tipY + (d.y * 14.0f) + (d.x * 9.0f);

    size_t base = i * kCubeArrowSegmentCount;
    setLinePoints(motionCubeArrowPoints[base], centerX, centerY, tipX, tipY);
    setLinePoints(motionCubeArrowPoints[base + 1], tipX, tipY, leftX, leftY);
    setLinePoints(motionCubeArrowPoints[base + 2], tipX, tipY, rightX, rightY);
    lv_obj_invalidate(motionCubeArrows[base]);
    lv_obj_invalidate(motionCubeArrows[base + 1]);
    lv_obj_invalidate(motionCubeArrows[base + 2]);
  }
}

void refreshMotionScreen()
{
  if (!motionBuilt) {
    return;
  }

  setMotionViewVisibility();
  switch (motionViewMode) {
    case MotionViewMode::Dot:
      updateMotionDot();
      break;
    case MotionViewMode::Cube:
      if (touchPressed) {
        break;
      }
      updateMotionCube();
      break;
    case MotionViewMode::Raw:
      updateMotionRaw();
      break;
    default:
      break;
  }
}

void updateWeatherHero()
{
  if (!weatherBuilt) {
    return;
  }

  uint32_t tick = millis() / 90;
  WeatherSceneType scene = weatherState.hasData ? weatherSceneTypeForCode(weatherState.weatherCode) : WeatherSceneType::Cloudy;

  lv_color_t topColor = lvColor(24, 34, 52);
  lv_color_t bottomColor = lvColor(6, 10, 18);
  switch (scene) {
    case WeatherSceneType::Clear:
      topColor = weatherState.isDay ? lvColor(60, 110, 210) : lvColor(16, 26, 64);
      bottomColor = weatherState.isDay ? lvColor(148, 194, 255) : lvColor(6, 10, 24);
      break;
    case WeatherSceneType::PartlyCloudy:
      topColor = lvColor(58, 82, 140);
      bottomColor = lvColor(110, 150, 220);
      break;
    case WeatherSceneType::Cloudy:
      topColor = lvColor(42, 50, 62);
      bottomColor = lvColor(76, 88, 108);
      break;
    case WeatherSceneType::Rain:
      topColor = lvColor(26, 34, 48);
      bottomColor = lvColor(48, 66, 94);
      break;
    case WeatherSceneType::Snow:
      topColor = lvColor(82, 110, 156);
      bottomColor = lvColor(192, 214, 244);
      break;
    case WeatherSceneType::Storm:
      topColor = lvColor(20, 24, 34);
      bottomColor = lvColor(58, 70, 92);
      break;
    case WeatherSceneType::Fog:
      topColor = lvColor(72, 82, 96);
      bottomColor = lvColor(144, 154, 166);
      break;
  }

  lv_obj_set_style_bg_color(weatherHero, topColor, 0);
  lv_obj_set_style_bg_grad_color(weatherHero, bottomColor, 0);
  lv_obj_set_style_bg_grad_dir(weatherHero, LV_GRAD_DIR_VER, 0);

  lv_obj_add_flag(weatherSun, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(weatherMoon, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(weatherBolt, LV_OBJ_FLAG_HIDDEN);
  for (auto *cloud : weatherClouds) {
    lv_obj_add_flag(cloud, LV_OBJ_FLAG_HIDDEN);
  }
  for (auto *particle : weatherParticles) {
    lv_obj_add_flag(particle, LV_OBJ_FLAG_HIDDEN);
  }
  for (auto *bar : weatherFogBars) {
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
  }

  if (scene == WeatherSceneType::Clear || scene == WeatherSceneType::PartlyCloudy) {
    lv_obj_t *celestial = weatherState.isDay ? weatherSun : weatherMoon;
    lv_obj_clear_flag(celestial, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(celestial, 34 + static_cast<int>((tick % 18) / 3), 28 + static_cast<int>((tick / 2) % 10));
  }

  if (scene == WeatherSceneType::PartlyCloudy || scene == WeatherSceneType::Cloudy ||
      scene == WeatherSceneType::Rain || scene == WeatherSceneType::Snow ||
      scene == WeatherSceneType::Storm || scene == WeatherSceneType::Fog) {
    lv_obj_clear_flag(weatherClouds[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(weatherClouds[1], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(weatherClouds[2], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(weatherClouds[0], 168 + static_cast<int>((tick % 12) - 6), 52);
    lv_obj_set_pos(weatherClouds[1], 64 + static_cast<int>((tick % 16) - 8), 96);
    lv_obj_set_pos(weatherClouds[2], 188 + static_cast<int>((tick % 10) - 5), 112);
  }

  if (scene == WeatherSceneType::Rain || scene == WeatherSceneType::Storm) {
    for (size_t i = 0; i < kWeatherParticleCount; ++i) {
      lv_obj_clear_flag(weatherParticles[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_color(weatherParticles[i], lvColor(144, 194, 255), 0);
      lv_obj_set_size(weatherParticles[i], 4, 18);
      int x = 42 + static_cast<int>(i * 36);
      int y = 86 + static_cast<int>((tick * 12 + i * 23) % 110);
      lv_obj_set_pos(weatherParticles[i], x, y);
    }
  }

  if (scene == WeatherSceneType::Snow) {
    for (size_t i = 0; i < kWeatherParticleCount; ++i) {
      lv_obj_clear_flag(weatherParticles[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_color(weatherParticles[i], lvColor(248, 250, 255), 0);
      lv_obj_set_size(weatherParticles[i], 8, 8);
      lv_obj_set_style_radius(weatherParticles[i], LV_RADIUS_CIRCLE, 0);
      int x = 30 + static_cast<int>((i * 38 + tick * 5) % 280);
      int y = 74 + static_cast<int>((tick * 8 + i * 17) % 120);
      lv_obj_set_pos(weatherParticles[i], x, y);
    }
  }

  if (scene == WeatherSceneType::Storm) {
    lv_obj_clear_flag(weatherBolt, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(weatherBolt, 244, 74 + static_cast<int>((tick % 4) * 2));
  }

  if (scene == WeatherSceneType::Fog) {
    for (int i = 0; i < 3; ++i) {
      lv_obj_clear_flag(weatherFogBars[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(weatherFogBars[i], 48 + static_cast<int>((tick % 10) - 5), 82 + (i * 34));
    }
  }
}

void refreshWeatherScreen()
{
  if (!weatherBuilt) {
    return;
  }

  if (weatherState.hasData) {
    String temp = String(weatherState.temperatureF) + "\xC2\xB0";
    lv_label_set_text(weatherTempLabel, temp.c_str());
    lv_label_set_text(weatherConditionLabel, weatherState.condition.c_str());

    String primary = "High " + String(weatherState.highF) +
                     "  Low " + String(weatherState.lowF) +
                     "  Feels " + String(weatherState.feelsLikeF);
    String secondary = "Wind " + String(weatherState.windMph) +
                       " mph  Rain " + String(weatherState.precipitationPercent) +
                       "%  " + (weatherState.isDay ? "Sunset " : "Sunrise ") +
                       (weatherState.isDay ? weatherState.sunset : weatherState.sunrise);
    lv_label_set_text(weatherPrimaryLabel, primary.c_str());
    lv_label_set_text(weatherSecondaryLabel, secondary.c_str());
    lv_label_set_text(weatherUpdatedValueLabel,
                      (weatherState.stale ? "Offline - cached forecast" : weatherState.updated).c_str());
  } else {
    lv_label_set_text(weatherTempLabel, "--");
    lv_label_set_text(weatherConditionLabel, weatherState.condition.c_str());
    lv_label_set_text(weatherPrimaryLabel, networkIsOnline() ? "Refreshing weather" : "Weather offline");
    lv_label_set_text(weatherSecondaryLabel,
                      networkIsOnline() ? "Generated animation stays active" : "Waiting for Wi-Fi");
    lv_label_set_text(weatherUpdatedValueLabel, weatherState.updated.c_str());
  }

  updateWeatherHero();
}

void setupLvgl()
{
  lv_init();
  lv_tick_set_cb(millis);

  display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
  size_t bufferPixelCount = LCD_WIDTH * kLvglBufferRows;
  lvBuffer = static_cast<uint8_t *>(heap_caps_malloc(bufferPixelCount * sizeof(lv_color16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (!lvBuffer) {
    Serial.println("LVGL buffer allocation failed");
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

void refreshUi()
{
  refreshWatchface();
  refreshMotionScreen();
  refreshWeatherScreen();
}

} // namespace

void setup()
{
  Serial.begin(115200);
  delay(250);
  preferences.begin(kPreferencesNamespace, false);

  setenv("TZ", TIMEZONE_POSIX, 1);
  tzset();

  if (!gfx->begin()) {
    Serial.println("Display initialization failed");
    while (true) {
      delay(1000);
    }
  }

  gfx->setBrightness(kActiveBrightness);
  gfx->fillScreen(kBackgroundColor);
  gfx->displayOn();

  Wire.begin(IIC_SDA, IIC_SCL);
  pinMode(TP_INT, INPUT_PULLUP);

  initExpander();
  initPower();
  initRtc();
  initTouch();
  initImu();
  initSdCard();

  configuredNetworkCount = countConfiguredNetworks();
  nextWeatherRefreshAtMs = millis() + 1000;
  nextWifiRetryAtMs = 0;
  lastActivityAtMs = millis();

  uint8_t savedMotionView = preferences.getUChar(kPrefMotionViewKey, static_cast<uint8_t>(MotionViewMode::Dot));
  if (savedMotionView < static_cast<uint8_t>(MotionViewMode::Count)) {
    motionViewMode = static_cast<MotionViewMode>(savedMotionView);
  } else {
    motionViewMode = MotionViewMode::Dot;
  }

  setupLvgl();
  buildUi();
  uint8_t savedScreen = preferences.getUChar(kPrefScreenKey, 0);
  if (savedScreen >= kScreenCount) {
    savedScreen = 0;
  }
  showScreen(savedScreen);
  refreshUi();
  lastStatusRefreshAtMs = millis();
  lastMotionRefreshAtMs = millis();
  lastWeatherAnimAtMs = millis();
}

void loop()
{
  updateWiFi();
  updateWeather();
  updateImuState();
  updateTopButton();
  updatePowerKey();
  updatePowerButtonHold();
  updateIdleState();
  persistTimeToRtc();

  if (otaReady) {
    ArduinoOTA.handle();
  }

  uint32_t now = millis();
  if (now - lastStatusRefreshAtMs >= kStatusRefreshMs) {
    lastStatusRefreshAtMs = now;
    refreshWatchface();
    refreshWeatherScreen();
  }

  if (now - lastMotionRefreshAtMs >= kMotionRefreshMs) {
    lastMotionRefreshAtMs = now;
    refreshMotionScreen();
  }

  if (now - lastWeatherAnimAtMs >= kWeatherAnimRefreshMs) {
    lastWeatherAnimAtMs = now;
    updateWeatherHero();
  }

  lv_timer_handler();
  delay(5);
}
