#include <Adafruit_XCA9554.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <Arduino_DriveBus_Library.h>
#include <Arduino_GFX_Library.h>
#include <ESP_I2S.h>
#include <FS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <qrcode.h>
#include <SD_MMC.h>
#include <SensorPCF85063.hpp>
#include <SensorQMI8658.hpp>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_sleep.h>
#include <lvgl.h>
#include <math.h>
#include <memory>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "config/ota_config.h"
#include "config/pin_config.h"
#include "config/screen_constants.h"
#include "core/screen_manager.h"
#include "drivers/es8311.h"
#include "modules/storage.h"
#include "modules/weather_module.h"
#include "modules/wifi_manager.h"
#include "screens/screen_callbacks.h"

#include <XPowersLib.h>

constexpr uint16_t kBackgroundColor = 0x0000;
constexpr uint8_t kActiveBrightness = 255;
constexpr uint8_t kDimBrightness = 26;
constexpr uint32_t kButtonDebounceMs = 180;
constexpr uint32_t kStatusRefreshMs = 250;
constexpr uint32_t kRecorderVisualRefreshMs = 33;
constexpr uint32_t kWeatherAnimRefreshMs = 90;
constexpr uint32_t kSkyRefreshMs = 1000;
constexpr bool kWeatherFetchEnabled = true;
constexpr uint32_t kWifiConnectTimeoutMs = 12000;
constexpr uint32_t kWifiRetryIntervalMs = 30000;
constexpr uint32_t kWifiServiceIntervalMs = 1000;
constexpr uint32_t kWifiServiceConnectingIntervalMs = 250;
constexpr uint32_t kRtcWriteIntervalSeconds = 60;
constexpr uint32_t kDimAfterMs = 30000;
constexpr uint32_t kSleepAfterMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kImuSampleIntervalMs = 20;
constexpr uint32_t kTapMaxDurationMs = 350;
constexpr uint32_t kShutdownHoldArmMs = 450;
constexpr uint32_t kPowerButtonShutdownHoldMs = 4000;
constexpr uint32_t kLvglBufferRows = 40;
constexpr time_t kMinValidEpoch = 1704067200;
constexpr int kMotionSwipeMinDistancePx = 8;
constexpr int kMotionSwipeMinDominancePx = -6;
constexpr int kQrSwipeMinDistancePx = 18;
constexpr int kQrSwipeMinDominancePx = 2;
constexpr int kScreenNavSwipeMinDistancePx = 20;
constexpr int kScreenNavSwipeMinDominancePx = 4;
constexpr int kTapMaxTravelPx = 12;
constexpr const char *kPreferencesNamespace = "waveform";
constexpr const char *kPrefScreenKey = "screen";
constexpr const char *kPrefMotionViewKey = "motionview";
constexpr const char *kPrefWeatherCacheKey = "weather";

struct WiFiCredential
{
  const char *ssid;
  const char *password;
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
I2SClass audioI2s;
extern uint32_t nextGeoRefreshAtMs;
extern uint32_t nextSolarRefreshAtMs;
extern size_t currentQrIndex;
extern bool qrNeedsRender;
void setGeoUnavailableState(const char *updatedLabel);
void setSolarUnavailableState(const char *updatedLabel);
void updateGeo();
void updateSolarSystem();
void handleTouchInterrupt();
void captureMotionReference();
bool refreshManagedScreen(ScreenId id);
void saveMotionViewPreference();
void saveScreenPreference(size_t screenIndex);
void showNextScreen();
void showPreviousScreen();
void updateRecorderAudio();
void initRtc();
void updateRecorderVisuals();
extern bool audioRecordingActive;
extern bool audioPlaybackActive;
std::unique_ptr<Arduino_IIC> touchController(
    new Arduino_FT3x68(touchBus, FT3168_DEVICE_ADDRESS, DRIVEBUS_DEFAULT_VALUE, TP_INT, handleTouchInterrupt));

lv_display_t *display = nullptr;
lv_indev_t *touchInput = nullptr;
static uint8_t *lvBuffer = nullptr;

lv_obj_t *screenRoots[kScreenCount] = {};
bool screenRefreshPending[kScreenCount] = {};
size_t currentScreenIndex = 0;
bool screenPreferenceSavePending = false;
size_t pendingScreenPreferenceIndex = 0;

lv_obj_t *watchTimeLabel = nullptr;
lv_obj_t *watchTimeRow = nullptr;
lv_obj_t *watchTimeGlyphs[5] = {nullptr};
lv_obj_t *watchDateLabel = nullptr;
lv_obj_t *watchTimezoneLabel = nullptr;
lv_obj_t *watchFirmwareLabel = nullptr;
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
bool otaOverlaySticky = false;
bool ntpConfigured = false;
bool motionReferenceReady = false;
bool motionReferenceCapturePending = false;
bool motionDisplayValid = false;
bool motionFilterReady = false;
bool haveLastAccelSample = false;
bool motionBuilt = false;
bool sdMounted = false;
bool touchTapTracking = false;
bool powerButtonHoldActive = false;
bool powerButtonShutdownTriggered = false;
ConnectivityState connectivityState = ConnectivityState::Offline;
MotionViewMode motionViewMode = MotionViewMode::Dot;
MotionViewMode renderedMotionViewMode = MotionViewMode::Count;
MotionState motionState;
MotionState motionDisplayState;
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
uint32_t lastRecorderVisualRefreshAtMs = 0;
uint32_t lastImuSampleAtMs = 0;
uint32_t wifiConnectStartedMs = 0;
uint32_t nextWifiRetryAtMs = 0;
uint32_t lastWifiServiceAtMs = 0;
uint32_t lastActivityAtMs = 0;
uint32_t otaOverlayHideAtMs = 0;
time_t lastRtcWriteEpoch = 0;
uint32_t touchTapStartedAtMs = 0;
uint32_t lastSkyRefreshAtMs = 0;

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

void saveMotionViewPreference()
{
  preferences.putUChar(kPrefMotionViewKey, static_cast<uint8_t>(motionViewMode));
}

void saveScreenPreference(size_t screenIndex)
{
  preferences.putUChar(kPrefScreenKey, static_cast<uint8_t>(screenIndex));
}

void scheduleScreenRefresh(ScreenId id)
{
  screenRefreshPending[static_cast<size_t>(id)] = true;
}

String firmwareUpdatedText()
{
  char text[40];
  snprintf(text, sizeof(text), "FW %s %.5s", __DATE__, __TIME__);
  return String(text);
}

void advanceMotionView()
{
  renderedMotionTransitionDirection = -1;
  motionViewMode = static_cast<MotionViewMode>((static_cast<uint8_t>(motionViewMode) + 1) %
                                               static_cast<uint8_t>(MotionViewMode::Count));
  saveMotionViewPreference();
}

void reverseMotionView()
{
  renderedMotionTransitionDirection = 1;
  motionViewMode = static_cast<MotionViewMode>(
      (static_cast<uint8_t>(motionViewMode) + static_cast<uint8_t>(MotionViewMode::Count) - 1) %
      static_cast<uint8_t>(MotionViewMode::Count));
  saveMotionViewPreference();
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

bool isWeatherStackScreen(ScreenId screen)
{
  return screen == ScreenId::Weather || screen == ScreenId::Geo ||
         screen == ScreenId::Solar || screen == ScreenId::Sky;
}

void handleQrSwipe(int deltaY)
{
  if (deltaY <= -kQrSwipeMinDistancePx) {
    currentQrIndex = (currentQrIndex + 1) % kQrEntryCount;
    qrNeedsRender = true;
    scheduleScreenRefresh(ScreenId::Qr);
    noteActivity();
    return;
  }

  if (deltaY >= kQrSwipeMinDistancePx) {
    currentQrIndex = (currentQrIndex + kQrEntryCount - 1) % kQrEntryCount;
    qrNeedsRender = true;
    scheduleScreenRefresh(ScreenId::Qr);
    noteActivity();
  }
}

void handleScreenNavigationSwipe(int deltaX)
{
  if (deltaX <= -kScreenNavSwipeMinDistancePx) {
    showNextScreen();
    noteActivity();
    return;
  }

  if (deltaX >= kScreenNavSwipeMinDistancePx) {
    showPreviousScreen();
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
    } else if (pressedNow && !touchGestureConsumed &&
               static_cast<ScreenId>(currentScreenIndex) == ScreenId::Qr) {
      int deltaX = touchLastX - touchTapStartX;
      int deltaY = touchLastY - touchTapStartY;
      int absDx = abs(deltaX);
      int absDy = abs(deltaY);
      if (absDy >= kQrSwipeMinDistancePx &&
          absDy >= (absDx + kQrSwipeMinDominancePx)) {
        handleQrSwipe(deltaY);
        touchGestureConsumed = true;
        touchTapTracking = false;
      }
    } else if (pressedNow && !touchGestureConsumed &&
               isWeatherStackScreen(static_cast<ScreenId>(currentScreenIndex))) {
      int deltaX = touchLastX - touchTapStartX;
      int deltaY = touchLastY - touchTapStartY;
      int absDx = abs(deltaX);
      int absDy = abs(deltaY);
      if (absDy >= kWeatherNavSwipeMinDistancePx &&
          absDy >= (absDx + kWeatherNavSwipeMinDominancePx)) {
        handleWeatherStackSwipe(deltaY);
        touchGestureConsumed = true;
        touchTapTracking = false;
      } else if (static_cast<ScreenId>(currentScreenIndex) == ScreenId::Weather &&
                 absDx >= kWeatherSwipeMinDistancePx &&
                 absDx >= (absDy + 10)) {
        handleWeatherSwipe(deltaX);
        touchGestureConsumed = true;
        touchTapTracking = false;
      }
    } else if (pressedNow && !touchGestureConsumed &&
               static_cast<ScreenId>(currentScreenIndex) == ScreenId::Watchface) {
      int deltaX = touchLastX - touchTapStartX;
      int deltaY = touchLastY - touchTapStartY;
      int absDx = abs(deltaX);
      int absDy = abs(deltaY);
      if (absDx >= kScreenNavSwipeMinDistancePx &&
          absDx >= (absDy + kScreenNavSwipeMinDominancePx)) {
        handleScreenNavigationSwipe(deltaX);
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
      } else if (static_cast<ScreenId>(currentScreenIndex) == ScreenId::Qr &&
                 absDy >= kQrSwipeMinDistancePx &&
                 absDy >= (absDx + kQrSwipeMinDominancePx)) {
        handleQrSwipe(deltaY);
      } else if (isWeatherStackScreen(static_cast<ScreenId>(currentScreenIndex)) &&
                 absDy >= kWeatherNavSwipeMinDistancePx &&
                 absDy >= (absDx + kWeatherNavSwipeMinDominancePx)) {
        handleWeatherStackSwipe(deltaY);
      } else if (static_cast<ScreenId>(currentScreenIndex) == ScreenId::Weather &&
                 absDx >= kWeatherSwipeMinDistancePx &&
                 absDx >= (absDy + 10)) {
        handleWeatherSwipe(deltaX);
      } else if (static_cast<ScreenId>(currentScreenIndex) == ScreenId::Watchface &&
                 absDx >= kScreenNavSwipeMinDistancePx &&
                 absDx >= (absDy + kScreenNavSwipeMinDominancePx)) {
        handleScreenNavigationSwipe(deltaX);
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

void initExpander()
{
  expanderReady = false;
  for (int attempt = 0; attempt < 3 && !expanderReady; ++attempt) {
    if (attempt > 0) {
      Wire.end();
      delay(10);
      Wire.begin(IIC_SDA, IIC_SCL);
      delay(10);
    }
    expanderReady = expander.begin(0x20);
  }
  if (!expanderReady) {
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
  localTime.tm_isdst = -1;
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

float degreesToRadians(float degrees)
{
  return degrees * (PI / 180.0f);
}

float radiansToDegrees(float radians)
{
  return radians * (180.0f / PI);
}

float normalizeDegrees(float degrees)
{
  while (degrees < 0.0f) {
    degrees += 360.0f;
  }
  while (degrees >= 360.0f) {
    degrees -= 360.0f;
  }
  return degrees;
}

double julianDateFromUnix(time_t unixTime)
{
  return (static_cast<double>(unixTime) / 86400.0) + 2440587.5;
}

double localSiderealTimeDegrees(time_t unixTime, float longitudeDegrees)
{
  double jd = julianDateFromUnix(unixTime);
  double t = (jd - 2451545.0) / 36525.0;
  double gmst = 280.46061837 +
                (360.98564736629 * (jd - 2451545.0)) +
                (0.000387933 * t * t) -
                (t * t * t / 38710000.0);
  return normalizeDegrees(static_cast<float>(gmst + longitudeDegrees));
}

bool equatorialToHorizontal(float raHours,
                            float decDegrees,
                            float latitudeDegrees,
                            float longitudeDegrees,
                            time_t unixTime,
                            float &altitudeDegrees,
                            float &azimuthDegrees)
{
  double raDegrees = static_cast<double>(raHours) * 15.0;
  double decRadians = degreesToRadians(decDegrees);
  double latRadians = degreesToRadians(latitudeDegrees);
  double hourAngleDegrees = localSiderealTimeDegrees(unixTime, longitudeDegrees) - raDegrees;
  while (hourAngleDegrees < -180.0) {
    hourAngleDegrees += 360.0;
  }
  while (hourAngleDegrees > 180.0) {
    hourAngleDegrees -= 360.0;
  }

  double hourAngleRadians = degreesToRadians(static_cast<float>(hourAngleDegrees));
  double sinAltitude = (sin(decRadians) * sin(latRadians)) +
                       (cos(decRadians) * cos(latRadians) * cos(hourAngleRadians));
  sinAltitude = fmax(-1.0, fmin(1.0, sinAltitude));
  double altitudeRadians = asin(sinAltitude);
  double cosAltitude = cos(altitudeRadians);

  if (fabs(cosAltitude) < 0.000001) {
    altitudeDegrees = 90.0f;
    azimuthDegrees = 0.0f;
    return true;
  }

  double cosAzimuth = (sin(decRadians) - (sin(altitudeRadians) * sin(latRadians))) /
                      (cosAltitude * cos(latRadians));
  cosAzimuth = fmax(-1.0, fmin(1.0, cosAzimuth));
  double azimuthRadians = acos(cosAzimuth);
  if (sin(hourAngleRadians) > 0.0) {
    azimuthRadians = (2.0 * PI) - azimuthRadians;
  }

  altitudeDegrees = radiansToDegrees(static_cast<float>(altitudeRadians));
  azimuthDegrees = radiansToDegrees(static_cast<float>(azimuthRadians));
  return true;
}

float compressedOrbitRadius(float distanceAu)
{
  return sqrtf(fmaxf(distanceAu, 0.0f));
}

bool parseHorizonsVectorRow(const String &result, float &xAu, float &yAu, float &zAu)
{
  int startIndex = result.indexOf("$$SOE");
  if (startIndex < 0) {
    return false;
  }

  startIndex = result.indexOf('\n', startIndex);
  if (startIndex < 0) {
    return false;
  }
  ++startIndex;

  int endIndex = result.indexOf('\n', startIndex);
  if (endIndex < 0) {
    return false;
  }

  String row = result.substring(startIndex, endIndex);
  row.trim();
  if (row.length() == 0) {
    return false;
  }

  float parsedX = 0.0f;
  float parsedY = 0.0f;
  float parsedZ = 0.0f;
  if (sscanf(row.c_str(), " %*[^,], %*[^,], %f, %f, %f", &parsedX, &parsedY, &parsedZ) != 3) {
    return false;
  }

  xAu = parsedX;
  yAu = parsedY;
  zAu = parsedZ;
  return true;
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

  weatherModuleMarkOffline();

  Serial.printf("Offline mode: %s\n", reason);
}

bool otaUpdateInProgress()
{
  return otaInProgress;
}

bool lightSleepActive()
{
  return inLightSleep;
}

void beginWifiAttempt(size_t credentialIndex)
{
  const WiFiCredential &credential = kWiFiCredentials[credentialIndex];

  currentCredentialIndex = credentialIndex;
  wifiAttemptInProgress = true;
  connectivityState = ConnectivityState::Connecting;
  wifiConnectStartedMs = millis();

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.disconnect(true, true);
  delay(40);
  WiFi.mode(WIFI_OFF);
  delay(40);
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

void scheduleOtaOverlayHide(uint32_t delayMs)
{
  otaOverlaySticky = true;
  otaOverlayHideAtMs = millis() + delayMs;
}

void hideOtaOverlay()
{
  if (!otaOverlay) {
    return;
  }

  lv_obj_add_flag(otaOverlay, LV_OBJ_FLAG_HIDDEN);
  otaOverlaySticky = false;
  otaOverlayHideAtMs = 0;
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
        otaOverlaySticky = false;
        otaOverlayHideAtMs = 0;
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
        scheduleOtaOverlayHide(1600);
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
      weatherModuleScheduleRefreshIn(1000);
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

  if (static_cast<ScreenId>(currentScreenIndex) == ScreenId::Weather) {
    weatherModuleClearDebugOverride();
    weatherModuleRefreshUi();
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
  if (otaInProgress || inLightSleep || audioRecordingActive || audioPlaybackActive) {
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

bool showScreen(size_t index)
{
  if (index >= kScreenCount) {
    return false;
  }

  ScreenId nextScreen = static_cast<ScreenId>(index);
  ScreenId previousScreen = static_cast<ScreenId>(currentScreenIndex);
  if (previousScreen != nextScreen) {
    screenManagerLeave(previousScreen);
  }

  if (!screenManagerEnsureBuilt(nextScreen)) {
    screenManagerShowFallback(nextScreen, screenManagerFailureReason(nextScreen));
    lv_refr_now(display);
    return false;
  }

  size_t previousScreenIndex = currentScreenIndex;
  currentScreenIndex = index;
  lv_screen_load(screenManagerRoot(nextScreen));
  lv_refr_now(display);

  screenManagerEnter(nextScreen);
  scheduleScreenRefresh(nextScreen);
  pendingScreenPreferenceIndex = currentScreenIndex;
  screenPreferenceSavePending = true;
  (void)previousScreenIndex;
  return true;
}

void showNextScreen()
{
  showScreen(screenManagerNextEnabledIndex(currentScreenIndex));
}

void showPreviousScreen()
{
  showScreen(screenManagerPreviousEnabledIndex(currentScreenIndex));
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
  buildOtaOverlay();
  buildPowerHoldOverlay();
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
  refreshManagedScreen(static_cast<ScreenId>(currentScreenIndex));
}

bool refreshManagedScreen(ScreenId id)
{
  if (!screenManagerRefresh(id)) {
    if (static_cast<ScreenId>(currentScreenIndex) == id) {
      screenManagerShowFallback(id, screenManagerFailureReason(id));
    }
    return false;
  }

  return true;
}

void setup()
{
  Serial.begin(115200);
  delay(250);
  randomSeed(static_cast<unsigned long>(micros()));
  preferences.begin(kPreferencesNamespace, false);
  weatherModuleConfigure({
      &preferences,
      kPrefWeatherCacheKey,
      kWeatherRefreshIntervalMs,
      kWeatherRetryIntervalMs,
      kWeatherFetchTimeoutMs,
      kWeatherFetchEnabled,
      networkIsOnline,
      otaUpdateInProgress,
      lightSleepActive,
      weatherApiUrl,
      weatherUpdatedLabel,
  });
  weatherModuleRestoreCache();

  setenv("TZ", TIMEZONE_POSIX, 1);
  tzset();

  Wire.begin(IIC_SDA, IIC_SCL);
  pinMode(TP_INT, INPUT_PULLUP);

  initExpander();
  initPower();
  initRtc();
  initTouch();
  initImu();
  sdMounted = false;

  if (!gfx->begin()) {
    Serial.println("Display initialization failed");
    while (true) {
      delay(1000);
    }
  }

  gfx->setBrightness(kActiveBrightness);
  gfx->fillScreen(kBackgroundColor);
  gfx->displayOn();
  initSdCard();

#ifdef WAVEFORM_DIAG_DISPLAY
  gfx->fillScreen(0xFFFF);
  Serial.println("WAVEFORM_DIAG_DISPLAY active");
  return;
#endif

  configuredNetworkCount = countConfiguredNetworks();
  if (configuredNetworkCount == 0 && !weatherModuleHasData()) {
    weatherModuleSetUnavailable("Offline - no Wi-Fi configured");
  }
  if (configuredNetworkCount == 0) {
    setGeoUnavailableState("Offline - no Wi-Fi configured");
    setSolarUnavailableState("Offline - no Wi-Fi configured");
  }
  weatherModuleScheduleRefreshIn(1000);
  nextGeoRefreshAtMs = 0;
  nextSolarRefreshAtMs = 0;
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

  if (!showScreen(savedScreen) && savedScreen != static_cast<uint8_t>(ScreenId::Watchface)) {
    showScreen(static_cast<size_t>(ScreenId::Watchface));
  }
  refreshUi();
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(display);
  lastStatusRefreshAtMs = millis();
  lastMotionRefreshAtMs = millis();
  lastWeatherAnimAtMs = millis();
  lastRecorderVisualRefreshAtMs = millis();
  lastSkyRefreshAtMs = millis();
}

void loop()
{
#ifdef WAVEFORM_DIAG_DISPLAY
  delay(50);
  return;
#endif

  uint32_t now = millis();
  uint32_t wifiServiceIntervalMs =
      wifiAttemptInProgress ? kWifiServiceConnectingIntervalMs : kWifiServiceIntervalMs;
  if (now - lastWifiServiceAtMs >= wifiServiceIntervalMs) {
    lastWifiServiceAtMs = now;
    updateWiFi();
  }
  weatherModuleUpdate();
  updateGeo();
  updateSolarSystem();
  updateImuState();
  updateRecorderAudio();
  updateTopButton();
  updatePowerKey();
  updatePowerButtonHold();
  updateIdleState();
  persistTimeToRtc();

  if (otaReady) {
    ArduinoOTA.handle();
  }

  if (!otaInProgress && otaOverlaySticky && static_cast<int32_t>(millis() - otaOverlayHideAtMs) >= 0) {
    hideOtaOverlay();
  }

  ScreenId activeScreen = static_cast<ScreenId>(currentScreenIndex);

  if (now - lastStatusRefreshAtMs >= kStatusRefreshMs) {
    lastStatusRefreshAtMs = now;
    if (activeScreen != ScreenId::Motion && activeScreen != ScreenId::Weather && activeScreen != ScreenId::Sky) {
      refreshManagedScreen(activeScreen);
    }
  }

  if (screenRefreshPending[currentScreenIndex]) {
    screenRefreshPending[currentScreenIndex] = false;
    if (refreshManagedScreen(activeScreen) && screenPreferenceSavePending &&
        pendingScreenPreferenceIndex == currentScreenIndex) {
      saveScreenPreference(currentScreenIndex);
      screenPreferenceSavePending = false;
    }
  }

  if (now - lastMotionRefreshAtMs >= kMotionRefreshMs) {
    lastMotionRefreshAtMs = now;
    if (activeScreen == ScreenId::Motion) {
      refreshManagedScreen(activeScreen);
    }
  }

  if (now - lastRecorderVisualRefreshAtMs >= kRecorderVisualRefreshMs) {
    lastRecorderVisualRefreshAtMs = now;
    if (static_cast<ScreenId>(currentScreenIndex) == ScreenId::Recorder) {
      updateRecorderVisuals();
    }
  }

  if (now - lastWeatherAnimAtMs >= kWeatherAnimRefreshMs) {
    lastWeatherAnimAtMs = now;
    if (activeScreen == ScreenId::Weather) {
      screenManagerTick(activeScreen, now);
    }
  }

  if (now - lastSkyRefreshAtMs >= kSkyRefreshMs) {
    lastSkyRefreshAtMs = now;
    if (activeScreen == ScreenId::Sky) {
      refreshManagedScreen(activeScreen);
    }
  }

  lv_timer_handler();
  delay(5);
}
