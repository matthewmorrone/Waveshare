#include <Adafruit_XCA9554.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Arduino_DriveBus_Library.h>
#include <Arduino_GFX_Library.h>
#include <ESP_I2S.h>
#include <FS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <qrcode.h>
#include <SD_MMC.h>
#include <SensorPCF85063.hpp>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_sleep.h>
#include <lvgl.h>
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
#include "modules/battery.h"
#include "modules/imu_module.h"
#include "modules/math_utils.h"
#include "modules/ota_module.h"
#include "modules/storage.h"
#include "modules/weather_module.h"
#include "modules/wifi_manager.h"
#include "screens/screen_callbacks.h"
#include "state/settings_state.h"

#include <XPowersLib.h>

constexpr uint8_t kActiveBrightness = 255;
constexpr uint8_t kDimBrightness = 26;
constexpr uint16_t kBackgroundColor = 0x0000;
constexpr uint32_t kButtonDebounceMs = 180;
constexpr uint32_t kStatusRefreshMs = 250;
constexpr uint32_t kRecorderVisualRefreshMs = 33;
constexpr uint32_t kWeatherAnimRefreshMs = 90;
constexpr uint32_t kSkyRefreshMs = 1000;
constexpr uint32_t kAutoCycleMs = 5000;
constexpr bool kWeatherFetchEnabled = true;
constexpr uint32_t kWifiConnectTimeoutMs = 12000;
constexpr uint32_t kWifiRetryIntervalMs = 30000;
constexpr uint32_t kWifiServiceIntervalMs = 1000;
constexpr uint32_t kWifiServiceConnectingIntervalMs = 250;
constexpr uint32_t kRtcWriteIntervalSeconds = 60;
constexpr uint32_t kDimAfterMs = 30000;
constexpr uint32_t kSleepAfterMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kTapMaxDurationMs = 350;
constexpr uint32_t kShutdownHoldArmMs = 450;
constexpr uint32_t kSideButtonLongPressMs = 1200;
constexpr uint32_t kPowerButtonShutdownHoldMs = 4000;
constexpr uint32_t kLvglBufferRows = 40;
constexpr time_t kMinValidEpoch = 1704067200;
constexpr int kMotionSwipeMinDistancePx = 8;
constexpr int kMotionSwipeMinDominancePx = -6;
#ifdef SCREEN_QR
constexpr int kQrSwipeMinDistancePx = 18;
constexpr int kQrSwipeMinDominancePx = 2;
#endif
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
std::shared_ptr<Arduino_IIC_DriveBus> touchBus = std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);
Preferences preferences;
I2SClass audioI2s;
#ifdef SCREEN_GEO
extern uint32_t nextGeoRefreshAtMs;
void setGeoUnavailableState(const char *updatedLabel);
void updateGeo();
#endif
#ifdef SCREEN_SOLAR
extern uint32_t nextSolarRefreshAtMs;
void setSolarUnavailableState(const char *updatedLabel);
void updateSolarSystem();
#endif
#ifdef SCREEN_QR
extern size_t currentQrIndex;
extern bool qrNeedsRender;
#endif
#ifdef SCREEN_RECORDER
void updateRecorderAudio();
extern bool audioRecordingActive;
extern bool audioPlaybackActive;
#endif
void handleTouchInterrupt();
bool refreshManagedScreen(ScreenId id);
void saveMotionViewPreference();
void saveScreenPreference(size_t screenIndex);
void showNextScreen();
void showPreviousScreen();
void initRtc();
std::unique_ptr<Arduino_IIC> touchController(
    new Arduino_FT3x68(touchBus, FT3168_DEVICE_ADDRESS, DRIVEBUS_DEFAULT_VALUE, TP_INT, handleTouchInterrupt));

lv_display_t *display = nullptr;
lv_indev_t *touchInput = nullptr;
static uint8_t *lvBuffer = nullptr;

lv_obj_t *screenRoots[16] = {};  // sized to max possible screens
bool screenRefreshPending[16] = {};
size_t currentScreenIndex = 0;
bool screenPreferenceSavePending = false;
size_t pendingScreenPreferenceIndex = 0;

lv_obj_t *watchTimeLabel = nullptr;
lv_obj_t *watchTimeRow = nullptr;
lv_obj_t *watchTimeGlyphs[5] = {};
lv_obj_t *watchDateLabel = nullptr;
lv_obj_t *watchTimezoneLabel = nullptr;
lv_obj_t *watchBatteryTrack = nullptr;
lv_obj_t *watchBatteryFill = nullptr;
lv_obj_t *watchWifiIcon = nullptr;
lv_obj_t *watchBluetoothIcon = nullptr;
lv_obj_t *watchUsbIcon = nullptr;

lv_obj_t *cycleCountdownLabel = nullptr;
lv_obj_t *powerHoldOverlay = nullptr;
lv_obj_t *powerHoldStatusLabel = nullptr;
lv_obj_t *powerHoldFooterLabel = nullptr;
lv_obj_t *powerHoldSecondsLabel = nullptr;
lv_obj_t *powerHoldBar = nullptr;

bool expanderReady = false;
bool powerReady = false;
bool rtcReady = false;
bool touchReady = false;
bool displayDimmed = false;
bool inLightSleep = false;
bool gFaceDownBlacked = false;
bool touchPressed = false;
bool touchGestureConsumed = false;
bool sideButtonPressed = false;
bool sideButtonReleaseArmed = false;
bool powerButtonPressed = false;
bool powerButtonReleaseArmed = false;
bool wifiAttemptInProgress = false;
bool ntpConfigured = false;
bool sdMounted = false;
bool touchTapTracking = false;
bool powerButtonHoldActive = false;
bool powerButtonShutdownTriggered = false;
ConnectivityState connectivityState = ConnectivityState::Offline;

int16_t touchLastX = LCD_WIDTH / 2;
int16_t touchLastY = LCD_HEIGHT / 2;
int16_t touchPrevY = LCD_HEIGHT / 2;
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
uint32_t wifiConnectStartedMs = 0;
uint32_t nextWifiRetryAtMs = 0;
uint32_t lastWifiServiceAtMs = 0;
uint32_t lastActivityAtMs = 0;
time_t lastRtcWriteEpoch = 0;
uint32_t touchTapStartedAtMs = 0;
uint32_t lastSkyRefreshAtMs = 0;
uint32_t lastAutoCycleAtMs = 0;
bool autoCycleEnabled = false;

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
  preferences.putUChar(kPrefMotionViewKey, static_cast<uint8_t>(motionGetViewMode()));
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
  motionAdvanceView();
  saveMotionViewPreference();
}

void reverseMotionView()
{
  motionReverseView();
  saveMotionViewPreference();
}

void handleScreenTap()
{
  ScreenId currentScreen = static_cast<ScreenId>(currentScreenIndex);
  if (currentScreen == ScreenId::Motion) {
    MotionViewMode mode = motionGetViewMode();
    if (mode == MotionViewMode::Dot) {
      imuModuleCaptureReference();
      motionCenterDot();
    } else if (mode == MotionViewMode::Cube) {
      imuModuleCaptureReference();
    }
    noteActivity();
    return;
  }
#ifdef SCREEN_RECORDER
  if (currentScreen == ScreenId::Recorder) {
    recorderHandleTap();
    return;
  }
#endif
#ifdef SCREEN_STOPWATCH
  if (currentScreen == ScreenId::Stopwatch) {
    stopwatchHandleTap();
    return;
  }
#endif
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
  return screen == ScreenId::Weather
#ifdef SCREEN_GEO
         || screen == ScreenId::Geo
#endif
#ifdef SCREEN_SOLAR
         || screen == ScreenId::Solar
#endif
#ifdef SCREEN_SKY
         || screen == ScreenId::Sky
#endif
  ;
}

#ifdef SCREEN_QR
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
#endif

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
      touchPrevY = touchLastY;
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
#ifdef SCREEN_LAUNCHER
    } else if (pressedNow &&
               static_cast<ScreenId>(currentScreenIndex) == ScreenId::Launcher &&
               touchGestureConsumed) {
      // Continue scrolling after gesture was committed
      waveformLauncherScrollBy(touchPrevY - touchLastY);
    } else if (pressedNow && !touchGestureConsumed &&
               static_cast<ScreenId>(currentScreenIndex) == ScreenId::Launcher) {
      int deltaX = touchLastX - touchTapStartX;
      int deltaY = touchLastY - touchTapStartY;
      int absDx = abs(deltaX);
      int absDy = abs(deltaY);
      if (absDy >= kMotionSwipeMinDistancePx &&
          absDy >= (absDx + kMotionSwipeMinDominancePx)) {
        waveformLauncherScrollBy(touchPrevY - touchLastY);
        touchGestureConsumed = true;
        touchTapTracking = false;
      }
#endif
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
#ifdef SCREEN_QR
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
#endif
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
               static_cast<ScreenId>(currentScreenIndex) == ScreenId::Watch) {
      int deltaX = touchLastX - touchTapStartX;
      int deltaY = touchLastY - touchTapStartY;
      int absDx = abs(deltaX);
      int absDy = abs(deltaY);
      if (absDy >= kScreenNavSwipeMinDistancePx && absDy >= (absDx + kMotionSwipeMinDominancePx)) {
        if (watchCalendarIsVisible()) { watchDismissCalendar(); } else { watchShowCalendar(); }
        noteActivity();
        touchGestureConsumed = true;
        touchTapTracking = false;
      } else if (absDx >= kScreenNavSwipeMinDistancePx &&
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
#ifdef SCREEN_QR
      } else if (static_cast<ScreenId>(currentScreenIndex) == ScreenId::Qr &&
                 absDy >= kQrSwipeMinDistancePx &&
                 absDy >= (absDx + kQrSwipeMinDominancePx)) {
        handleQrSwipe(deltaY);
#endif
      } else if (isWeatherStackScreen(static_cast<ScreenId>(currentScreenIndex)) &&
                 absDy >= kWeatherNavSwipeMinDistancePx &&
                 absDy >= (absDx + kWeatherNavSwipeMinDominancePx)) {
        handleWeatherStackSwipe(deltaY);
      } else if (static_cast<ScreenId>(currentScreenIndex) == ScreenId::Weather &&
                 absDx >= kWeatherSwipeMinDistancePx &&
                 absDx >= (absDy + 10)) {
        handleWeatherSwipe(deltaX);
      } else if (static_cast<ScreenId>(currentScreenIndex) == ScreenId::Watch &&
                 absDy >= kScreenNavSwipeMinDistancePx && absDy >= (absDx + kMotionSwipeMinDominancePx)) {
        if (watchCalendarIsVisible()) { watchDismissCalendar(); } else { watchShowCalendar(); }
        noteActivity();
      } else if (static_cast<ScreenId>(currentScreenIndex) == ScreenId::Watch &&
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
#ifdef SCREEN_STOPWATCH
      else if (static_cast<ScreenId>(currentScreenIndex) == ScreenId::Stopwatch) {
        uint32_t held = millis() - touchTapStartedAtMs;
        if (held >= kStopwatchLongPressMs) {
          stopwatchHandleLongPress();
          touchTapTracking = false;
          touchGestureConsumed = true;
        }
      }
#endif
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

void setOfflineMode(const char *reason)
{
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
  }

  connectivityState = ConnectivityState::Offline;
  wifiAttemptInProgress = false;
  ntpConfigured = false;

  weatherModuleMarkOffline();

  Serial.printf("Offline mode: %s\n", reason);
}

bool otaUpdateInProgress()
{
  return otaModuleIsInProgress();
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
    otaModuleStart();
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
  gFaceDownBlacked = false;

  initTouch();
  imuModuleInit();
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
  if (inLightSleep || otaModuleIsInProgress() || usbIsConnected()) {
    return;
  }

  Serial.println("Entering light sleep");
  inLightSleep = true;

  setOfflineMode("entering light sleep");
  WiFi.mode(WIFI_OFF);

  enableWakeOnPinChange(TP_INT);
  if (imuModuleConfigureWakeOnMotion()) {
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
  if (otaModuleIsInProgress() || inLightSleep
#ifdef SCREEN_RECORDER
      || audioRecordingActive || audioPlaybackActive
#endif
  ) {
    return;
  }

  // Face-down blackout — runs before USB check so it works while charging
  if (settingsState().faceDownBlackout && imuModuleIsReady()) {
    const MotionState &ms = imuModuleState();
    bool faceDown = ms.valid && strcmp(ms.orientation, "Face Down") == 0;
    if (faceDown && !gFaceDownBlacked) {
      gFaceDownBlacked = true;
      gfx->displayOff();
      setDisplayBrightness(0);
    } else if (!faceDown && gFaceDownBlacked) {
      gFaceDownBlacked = false;
      displayDimmed = false;
      gfx->displayOn();
      setDisplayBrightness(settingsState().brightness);
      noteActivity();
    }
  } else if (gFaceDownBlacked) {
    gFaceDownBlacked = false;
    displayDimmed = false;
    gfx->displayOn();
    setDisplayBrightness(settingsState().brightness);
    noteActivity();
  }

  if (usbIsConnected()) {
    if (displayDimmed) {
      gfx->displayOn();
      setDisplayBrightness(settingsState().brightness);
      displayDimmed = false;
    }
    lastActivityAtMs = millis();
    return;
  }

  if (gFaceDownBlacked) {
    return;  // don't advance dim/sleep timers while blacked out
  }

  uint32_t idleMs = millis() - lastActivityAtMs;
  if (!displayDimmed && idleMs >= kDimAfterMs) {
    setDisplayBrightness(kDimBrightness);
    displayDimmed = true;
  }

  uint32_t sleepMs = settingsState().sleepAfterMs;
  if (sleepMs > 0 && idleMs >= sleepMs) {
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
  screenManagerEnter(nextScreen);
  screenManagerRefresh(nextScreen);
  lv_screen_load(screenManagerRoot(nextScreen));
  lv_refr_now(display);

  // Free the previous screen after the new one is rendered to avoid mid-frame deletion.
  // Keep Watch and Launcher cached for instant switching.
  if (previousScreen != nextScreen) {
#ifdef SCREEN_WATCH
    if (previousScreen != ScreenId::Watch)
#endif
    {
#ifdef SCREEN_LAUNCHER
      if (previousScreen != ScreenId::Launcher)
#endif
      {
        screenManagerDestroy(previousScreen);
      }
    }
  }

  scheduleScreenRefresh(nextScreen);
  pendingScreenPreferenceIndex = currentScreenIndex;
  screenPreferenceSavePending = true;
  (void)previousScreenIndex;
  return true;
}

bool showScreenById(ScreenId id)
{
  return showScreen(static_cast<size_t>(id));
}

void showNextScreen()
{
  if (watchCalendarIsVisible()) watchDismissCalendarSilent();
  showScreen(screenManagerNextEnabledIndex(currentScreenIndex));
}

void showPreviousScreen()
{
  if (watchCalendarIsVisible()) watchDismissCalendarSilent();
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
      }
    }
    return;
  }

  if (sideButtonPressed) {
    uint32_t heldMs = now - sideButtonPressedAtMs;
    sideButtonPressed = false;
    sideButtonReleaseArmed = false;
    if (heldMs >= kSideButtonLongPressMs && settingsState().autoCycleEnabled) {
      autoCycleEnabled = !autoCycleEnabled;
      lastAutoCycleAtMs = now;
    } else {
      showNextScreen();
      noteActivity();
    }
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
  bool longPress = power.isPekeyLongPressIrq();
  bool releasedEdge = power.isPekeyNegativeIrq();

  if (longPress && !powerButtonShutdownTriggered && settingsState().autoCycleEnabled) {
    autoCycleEnabled = !autoCycleEnabled;
    lastAutoCycleAtMs = millis();
    cancelPowerButtonHold();
    noteActivity();
  }

  if (shortPress) {
    powerButtonPressed = false;
    bool shouldAct = !powerButtonShutdownTriggered &&
                     millis() - lastPowerButtonEdgeAtMs >= kButtonDebounceMs;
    cancelPowerButtonHold();
    if (shouldAct) {
      lastPowerButtonEdgeAtMs = millis();
      showNextScreen();
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

void buildCycleCountdown()
{
  cycleCountdownLabel = lv_label_create(lv_layer_top());
  lv_obj_set_style_text_font(cycleCountdownLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(cycleCountdownLabel, lvColor(255, 255, 255), 0);
  lv_obj_set_style_text_opa(cycleCountdownLabel, LV_OPA_70, 0);
  lv_label_set_text(cycleCountdownLabel, "");
  lv_obj_align(cycleCountdownLabel, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_add_flag(cycleCountdownLabel, LV_OBJ_FLAG_HIDDEN);
}

void buildUi()
{
  otaModuleBuildOverlay();
  buildPowerHoldOverlay();
  buildCycleCountdown();
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
  settingsLoad(preferences);
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
  imuModuleInit();
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
#ifdef SCREEN_GEO
  if (configuredNetworkCount == 0) {
    setGeoUnavailableState("Offline - no Wi-Fi configured");
  }
  nextGeoRefreshAtMs = 0;
#endif
#ifdef SCREEN_SOLAR
  if (configuredNetworkCount == 0) {
    setSolarUnavailableState("Offline - no Wi-Fi configured");
  }
  nextSolarRefreshAtMs = 0;
#endif
  weatherModuleScheduleRefreshIn(1000);
  nextWifiRetryAtMs = 0;
  lastActivityAtMs = millis();

  uint8_t savedMotionView = preferences.getUChar(kPrefMotionViewKey, static_cast<uint8_t>(MotionViewMode::Dot));
  if (savedMotionView < static_cast<uint8_t>(MotionViewMode::Count)) {
    motionSetViewMode(static_cast<MotionViewMode>(savedMotionView));
  } else {
    motionSetViewMode(MotionViewMode::Dot);
  }

  setupLvgl();
  buildUi();
  setDisplayBrightness(settingsState().brightness);
  autoCycleEnabled = settingsState().autoCycleEnabled;
  uint8_t savedScreen = preferences.getUChar(kPrefScreenKey, 0);
  if (savedScreen >= kScreenCount) {
    savedScreen = 0;
  }

  if (!showScreen(savedScreen) && savedScreen != static_cast<uint8_t>(ScreenId::Watch)) {
    showScreen(static_cast<size_t>(ScreenId::Watch));
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
#ifdef SCREEN_GEO
  updateGeo();
#endif
#ifdef SCREEN_SOLAR
  updateSolarSystem();
#endif
  imuModuleUpdate();
#ifdef SCREEN_RECORDER
  updateRecorderAudio();
#endif
  updateTopButton();
  updatePowerKey();
  updatePowerButtonHold();
  updateIdleState();
  persistTimeToRtc();

  otaModuleHandle();
  otaModuleUpdate();

  ScreenId activeScreen = static_cast<ScreenId>(currentScreenIndex);

  if (now - lastStatusRefreshAtMs >= kStatusRefreshMs) {
    lastStatusRefreshAtMs = now;
    if (activeScreen != ScreenId::Motion && activeScreen != ScreenId::Weather
#ifdef SCREEN_SKY
        && activeScreen != ScreenId::Sky
#endif
#ifdef SCREEN_SPECTRUM
        && activeScreen != ScreenId::Spectrum
#endif
    ) {
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


  if (now - lastWeatherAnimAtMs >= kWeatherAnimRefreshMs) {
    lastWeatherAnimAtMs = now;
    if (activeScreen == ScreenId::Weather) {
      screenManagerTick(activeScreen, now);
    }
  }

#ifdef SCREEN_SPECTRUM
  if (activeScreen == ScreenId::Spectrum) {
    screenManagerTick(activeScreen, now);
  }
#endif
#ifdef SCREEN_STOPWATCH
  if (activeScreen == ScreenId::Stopwatch) {
    screenManagerTick(activeScreen, now);
  }
#endif
#ifdef SCREEN_TIMER
  if (activeScreen == ScreenId::Timer) {
    screenManagerTick(activeScreen, now);
  }
#endif

#ifdef SCREEN_SKY
  if (now - lastSkyRefreshAtMs >= kSkyRefreshMs) {
    lastSkyRefreshAtMs = now;
    if (activeScreen == ScreenId::Sky) {
      refreshManagedScreen(activeScreen);
    }
  }
#endif

  uint32_t autoCycleIntervalMs = settingsState().autoCycleIntervalMs;
  if (autoCycleEnabled && now - lastAutoCycleAtMs >= autoCycleIntervalMs) {
    lastAutoCycleAtMs = now;
    showNextScreen();
  }

  if (cycleCountdownLabel) {
    if (autoCycleEnabled) {
      uint32_t elapsed = now - lastAutoCycleAtMs;
      uint32_t remaining = elapsed < autoCycleIntervalMs ? (autoCycleIntervalMs - elapsed + 999) / 1000 : 1;
      char buf[4];
      snprintf(buf, sizeof(buf), "%u", remaining);
      lv_label_set_text(cycleCountdownLabel, buf);
      lv_obj_clear_flag(cycleCountdownLabel, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(cycleCountdownLabel, LV_OBJ_FLAG_HIDDEN);
    }
  }

  lv_timer_handler();
  delay(5);
}
