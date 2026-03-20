#include <Adafruit_XCA9554.h>
#include <Arduino.h>
#include <Arduino_DriveBus_Library.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <Arduino_GFX_Library.h>
#include <BLEDevice.h>
#include <BLESecurity.h>
#include <HTTPClient.h>
#include "pin_config.h"
#include <SensorPCF85063.hpp>
#include <SensorQMI8658.hpp>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <XPowersLib.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

#include "AvenirNextCondensedDemiBoldClock60pt7b.h"
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <math.h>
#include <memory>
#include <sys/time.h>
#include <time.h>

#include "ota_config.h"

namespace
{
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

struct WiFiCredential
{
  const char *ssid;
  const char *password;
};

enum class ConnectivityState
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

enum class MotionViewMode : uint8_t
{
  Dot,
  Cube,
  Raw,
  Count,
};

struct Rect
{
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
};

struct TextLayout
{
  int16_t cursorX = 0;
  int16_t cursorY = 0;
  Rect bounds = {0, 0, 0, 0};
};

enum class TextAlign
{
  Left,
  Center,
  Right,
};

struct WatchfaceState
{
  String time;
  String timezone;
  String date;
  int batteryPercent;
  bool batteryAvailable;
  bool chargingActive;
  bool wifiConnected;
  bool bluetoothConnected;
  bool bluetoothActivityActive;
  bool usbConnected;
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

struct MotionIndicatorPosition
{
  int16_t x = 0;
  int16_t y = 0;
};

struct Vec3
{
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
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
  String condition;
  String updated;
  String sunrise;
  String sunset;
  String location = WEATHER_LOCATION_LABEL;
};

struct KeyboardBootReport
{
  uint8_t modifiers = 0;
  uint8_t reserved = 0;
  uint8_t keys[6] = {0, 0, 0, 0, 0, 0};
} __attribute__((packed));

constexpr WiFiCredential kWiFiCredentials[] = {
    {WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY},
    {WIFI_SSID_FALLBACK, WIFI_PASSWORD_FALLBACK},
};

constexpr uint8_t kI2cExpanderAddress = 0x20;
constexpr uint8_t kTopButtonPin = 4;
constexpr uint8_t kPmuIrqPin = 5;

constexpr uint32_t kWifiConnectTimeoutMs = 12000;
constexpr uint32_t kWifiRetryIntervalMs = 30000;
constexpr uint32_t kWatchfaceRenderIntervalMs = 250;
constexpr uint32_t kMotionRenderIntervalMs = 90;
constexpr uint32_t kWeatherRenderIntervalMs = 100;
constexpr uint32_t kPowerHoldRenderIntervalMs = 50;
constexpr uint32_t kButtonDebounceMs = 180;
constexpr uint32_t kTouchDebounceMs = 180;
constexpr uint32_t kRtcWriteIntervalSeconds = 300;
constexpr uint32_t kImuSampleIntervalMs = 80;
constexpr uint32_t kWeatherRefreshIntervalMs = 20UL * 60UL * 1000UL;
constexpr uint32_t kWeatherRetryIntervalMs = 2UL * 60UL * 1000UL;
constexpr uint32_t kWeatherFetchTimeoutMs = 8000;

constexpr time_t kMinValidEpoch = 1704067200; // 2024-01-01 00:00:00 UTC

constexpr uint16_t kBackgroundColor = RGB565_BLACK;
constexpr uint16_t kForegroundColor = RGB565_WHITE;
constexpr uint16_t kMutedTextColor = rgb565(150, 150, 164);
constexpr uint16_t kDisconnectedColor = rgb565(88, 88, 100);
constexpr uint16_t kSurfaceColor = rgb565(22, 22, 28);
constexpr uint16_t kDividerColor = rgb565(34, 34, 40);
constexpr uint16_t kAccentBlueColor = rgb565(0, 120, 255);
constexpr uint16_t kBatteryUnavailableColor = rgb565(80, 80, 80);
constexpr uint16_t kWeatherSkyDayColor = rgb565(12, 40, 84);
constexpr uint16_t kWeatherSkyNightColor = rgb565(4, 8, 18);
constexpr uint16_t kWeatherCloudColor = rgb565(210, 218, 232);
constexpr uint16_t kWeatherCloudShadeColor = rgb565(120, 132, 154);
constexpr uint16_t kWeatherRainColor = rgb565(92, 180, 255);
constexpr uint16_t kWeatherSunColor = rgb565(255, 193, 32);
constexpr uint16_t kWeatherMoonColor = rgb565(196, 208, 255);
constexpr uint16_t kWeatherSnowColor = rgb565(240, 248, 255);
constexpr uint16_t kWeatherStormColor = rgb565(255, 230, 120);
constexpr uint16_t kWeatherFogColor = rgb565(126, 136, 152);

constexpr uint8_t kActiveBrightness = 255;
constexpr uint8_t kDimBrightness = 24;
constexpr uint32_t kDimAfterMs = 30000;
constexpr uint32_t kSleepAfterMs = 300000;
constexpr uint32_t kPowerButtonShutdownHoldMs = 4000;
constexpr float kMotionDeltaThreshold = 0.12f;
constexpr float kMotionDisplaySmoothingAlpha = 0.22f;
constexpr float kMotionReadoutSmoothingAlpha = 0.14f;
constexpr float kMotionIndicatorSmoothingAlpha = 0.12f;
constexpr float kMotionIndicatorDeadzoneDegrees = 2.0f;
constexpr float kMotionIndicatorFullScaleDegrees = 38.0f;
constexpr uint8_t kWakeOnMotionThresholdMg = 160;

constexpr Rect kTimeRect = {2, 104, LCD_WIDTH - 4, 156};
constexpr Rect kDateRect = {18, 270, LCD_WIDTH - 36, 20};
constexpr Rect kTimezoneRect = {18, 296, LCD_WIDTH - 36, 16};
constexpr Rect kKeyboardToastRect = {18, 320, LCD_WIDTH - 36, 30};
constexpr Rect kBatteryBarRect = {51, 358, 266, 12};
constexpr Rect kStatusIconsRect = {LCD_WIDTH - 126, 18, 110, 34};
constexpr Rect kBluetoothIconRect = {LCD_WIDTH - 122, 18, 24, 28};
constexpr Rect kWiFiIconRect = {LCD_WIDTH - 84, 22, 28, 22};
constexpr Rect kPowerIconRect = {LCD_WIDTH - 44, 18, 24, 30};

constexpr Rect kHeaderTitleRect = {18, 20, LCD_WIDTH - 36, 28};
constexpr Rect kHeaderSubtitleRect = {18, 54, LCD_WIDTH - 36, 16};
constexpr Rect kMotionFullRect = {0, 0, LCD_WIDTH, LCD_HEIGHT};
constexpr Rect kMotionDotRect = {12, 12, LCD_WIDTH - 24, LCD_HEIGHT - 24};
constexpr Rect kMotionRawOrientationRect = {18, 72, LCD_WIDTH - 36, 32};
constexpr Rect kMotionRawPitchRect = {18, 136, LCD_WIDTH - 36, 24};
constexpr Rect kMotionRawRollRect = {18, 172, LCD_WIDTH - 36, 24};
constexpr Rect kMotionRawAccelRect = {18, 230, LCD_WIDTH - 36, 22};
constexpr Rect kMotionRawGyroRect = {18, 266, LCD_WIDTH - 36, 22};

constexpr Rect kOtaTitleRect = {0, 98, LCD_WIDTH, 40};
constexpr Rect kOtaPercentRect = {0, 168, LCD_WIDTH, 56};
constexpr Rect kOtaStatusRect = {0, 236, LCD_WIDTH, 24};
constexpr Rect kOtaFooterRect = {0, 344, LCD_WIDTH, 20};
constexpr Rect kOtaBarOuterRect = {44, 274, LCD_WIDTH - 88, 28};
constexpr Rect kOtaBarInnerRect = {48, 278, LCD_WIDTH - 96, 20};
constexpr Rect kPowerHoldTitleRect = {0, 106, LCD_WIDTH, 34};
constexpr Rect kPowerHoldSubtitleRect = {0, 154, LCD_WIDTH, 20};
constexpr Rect kPowerHoldSecondsRect = {0, 196, LCD_WIDTH, 64};
constexpr Rect kPowerHoldHintRect = {0, 354, LCD_WIDTH, 20};
constexpr Rect kPowerHoldBarOuterRect = {52, 288, LCD_WIDTH - 104, 22};
constexpr Rect kPowerHoldBarInnerRect = {56, 292, LCD_WIDTH - 112, 14};
constexpr Rect kWeatherHeroRect = {0, 0, LCD_WIDTH, 232};
constexpr Rect kWeatherLocationRect = {18, 22, LCD_WIDTH - 36, 18};
constexpr Rect kWeatherTempRect = {18, 242, LCD_WIDTH - 36, 74};
constexpr Rect kWeatherConditionRect = {18, 316, LCD_WIDTH - 36, 28};
constexpr Rect kWeatherDetailPrimaryRect = {18, 352, LCD_WIDTH - 36, 18};
constexpr Rect kWeatherDetailSecondaryRect = {18, 378, LCD_WIDTH - 36, 18};
constexpr Rect kWeatherUpdatedRect = {18, 404, LCD_WIDTH - 36, 18};

constexpr char kKeyboardTargetName[] = "K808";
constexpr char kKeyboardTargetAddressString[] = "41:83:B6:D7:2E:2D";
constexpr uint32_t kKeyboardScanDurationSeconds = 5;
constexpr uint32_t kKeyboardReconnectIntervalMs = 8000;
constexpr uint32_t kKeyboardToastDurationMs = 3500;
constexpr uint32_t kKeyboardNotificationDedupMs = 180;
constexpr size_t kKeyboardPreviewMaxLength = 28;

const BLEUUID kKeyboardHidServiceUuid((uint16_t)0x1812);
const BLEUUID kKeyboardReportCharacteristicUuid((uint16_t)0x2A4D);
const BLEUUID kKeyboardBootInputCharacteristicUuid((uint16_t)0x2A22);
const BLEUUID kKeyboardProtocolModeCharacteristicUuid((uint16_t)0x2A4E);
const BLEUUID kKeyboardReportReferenceDescriptorUuid((uint16_t)0x2908);
const BLEAddress kKeyboardTargetAddress{String(kKeyboardTargetAddressString)};

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
void handleTouchInterrupt();
std::unique_ptr<Arduino_IIC> touchController(
    new Arduino_FT3x68(touchBus, FT3168_DEVICE_ADDRESS, DRIVEBUS_DEFAULT_VALUE, TP_INT, handleTouchInterrupt));

ConnectivityState connectivityState = ConnectivityState::Offline;
ScreenId currentScreen = ScreenId::Watchface;
ScreenId lastRenderedScreen = ScreenId::Count;
MotionViewMode motionViewMode = MotionViewMode::Dot;

bool otaReady = false;
bool expanderReady = false;
bool powerReady = false;
bool rtcReady = false;
bool imuReady = false;
bool touchReady = false;
bool wifiAttemptInProgress = false;
bool ntpConfigured = false;
bool otaScreenActive = false;
bool otaInProgress = false;
bool otaScreenDirty = false;
bool displayDimmed = false;
bool inLightSleep = false;
bool sideButtonPressed = false;
bool haveLastAccelSample = false;
bool hasRenderedWatchface = false;
bool screenDirty = true;
bool touchPressed = false;
bool touchTapPending = false;
bool keyboardBleReady = false;
bool keyboardScanActive = false;
bool keyboardDoConnect = false;
bool keyboardConnected = false;
bool keyboardHaveLastReport = false;
bool sideButtonReleaseArmed = false;
bool powerButtonHoldActive = false;
bool powerButtonShutdownTriggered = false;
bool weatherFetchInProgress = false;
bool hasRenderedWeatherScreen = false;

size_t configuredNetworkCount = 0;
size_t currentCredentialIndex = 0;
size_t nextCredentialIndex = 0;

uint32_t wifiConnectStartedMs = 0;
uint32_t nextWifiRetryAtMs = 0;
uint32_t lastRenderAtMs = 0;
uint32_t otaScreenDismissAtMs = 0;
uint32_t lastActivityAtMs = 0;
uint32_t lastImuSampleAtMs = 0;
uint32_t lastNavigationAtMs = 0;
uint32_t lastTouchTapAtMs = 0;
uint32_t keyboardNextRetryAtMs = 0;
uint32_t keyboardToastUntilMs = 0;
uint32_t sideButtonPressedAtMs = 0;
uint32_t keyboardActivityFlashUntilMs = 0;
uint32_t keyboardLastKeyToastAtMs = 0;
uint32_t powerButtonHoldStartedAtMs = 0;
uint32_t nextWeatherRefreshAtMs = 0;

time_t lastRtcWriteEpoch = 0;
WatchfaceState lastRenderedWatchface;
MotionState motionState;
MotionState motionDisplayState;
MotionState lastRenderedMotionState;
WeatherState weatherState;
int otaProgressPercent = 0;
int otaRenderedPercent = -1;
String otaStatusText;
String otaFooterText;
String lastRenderedOtaStatus;
String lastRenderedOtaFooter;
float lastAccelX = 0.0f;
float lastAccelY = 0.0f;
float lastAccelZ = 0.0f;
float motionPitchZero = 0.0f;
float motionRollZero = 0.0f;
int16_t touchLastX = 0;
int16_t touchLastY = 0;
KeyboardBootReport keyboardLastReport;
BLEScan *keyboardScan = nullptr;
BLEClient *keyboardClient = nullptr;
BLEAdvertisedDevice *keyboardAdvertisedDevice = nullptr;
String keyboardPreviewText;
String keyboardToastText;
String keyboardLastNotificationSignature;
String lastRenderedKeyboardToast;
String lastRenderedMotionPitchLine;
String lastRenderedMotionRollLine;
String lastRenderedMotionAccelLine;
String lastRenderedMotionGyroLine;
String lastRenderedMotionOrientation;
String lastRenderedWeatherLocation;
String lastRenderedWeatherTemp;
String lastRenderedWeatherCondition;
String lastRenderedWeatherPrimary;
String lastRenderedWeatherSecondary;
String lastRenderedWeatherUpdated;
bool lastRenderedKeyboardToastVisible = false;
bool hasRenderedMotionScreen = false;
bool lastRenderedMotionUnavailable = false;
bool lastRenderedWeatherHasData = false;
bool lastRenderedWeatherStale = false;
bool lastRenderedWeatherIsDay = true;
int lastRenderedWeatherCode = -2;
bool motionIndicatorFilterReady = false;
uint32_t keyboardLastNotificationAtMs = 0;
int lastRenderedPowerHoldTenths = -1;
MotionIndicatorPosition lastRenderedMotionDot;
float motionIndicatorPitchOffset = 0.0f;
float motionIndicatorRollOffset = 0.0f;
MotionViewMode lastRenderedMotionViewMode = MotionViewMode::Count;
Vec3 motionReferenceDown = {0.0f, 0.0f, 1.0f};
Vec3 motionReferenceAxisA = {1.0f, 0.0f, 0.0f};
Vec3 motionReferenceAxisB = {0.0f, 1.0f, 0.0f};
bool motionReferenceReady = false;

void beginOtaScreen(const String &status, const String &footer, int percent);
void updateOtaScreen(const String &status, const String &footer, int percent);
void endOtaScreen(uint32_t dismissDelayMs);
void renderOtaScreen(bool force = false);
void renderPowerHoldScreen(bool force = false);
void renderCurrentScreen(bool force = false);
void setOfflineMode(const char *reason);
void beginNextWifiCycle();
void noteActivity();
bool initTouch();
void updateTouchInput();
void handleMotionTouch();
void beginMotionScreen();
void updateWeather();
void fetchWeatherIfDue();
bool fetchWeather();
String weatherApiUrl();
String weatherConditionLabel(int weatherCode, bool isDay);
WeatherSceneType weatherSceneTypeForCode(int weatherCode);
void renderWeatherScreen(bool force);
void initKeyboardBle();
void updateKeyboard();
bool connectToKeyboard();
void startKeyboardScan();
void scheduleKeyboardRetry(uint32_t delayMs = kKeyboardReconnectIntervalMs);
void showKeyboardToast(const String &text, uint32_t durationMs = kKeyboardToastDurationMs);
void renderKeyboardToast(bool force = false);
void startPowerButtonHold();
void cancelPowerButtonHold();
void updatePowerButtonHold();
void triggerPowerButtonShutdown();
void navigateScreen(int direction);

bool keyboardToastVisible()
{
  return !keyboardToastText.isEmpty() &&
         static_cast<int32_t>(millis() - keyboardToastUntilMs) < 0;
}

String formatKeyboardBytes(const uint8_t *data, size_t length, size_t maxBytes = 10);

bool keyboardActivityVisible()
{
  return static_cast<int32_t>(millis() - keyboardActivityFlashUntilMs) < 0;
}

void markKeyboardActivity(uint32_t durationMs = 1000)
{
  keyboardActivityFlashUntilMs = millis() + durationMs;
  screenDirty = true;
}

bool keyboardKeyCodeLooksValid(uint8_t keyCode)
{
  return keyCode == 0 || keyCode <= 0x73;
}

bool keyboardKeyArrayLooksValid(const uint8_t *keys, size_t count)
{
  if (keys == nullptr) {
    return false;
  }

  for (size_t i = 0; i < count; ++i) {
    if (!keyboardKeyCodeLooksValid(keys[i])) {
      return false;
    }
  }
  return true;
}

bool keyboardRawFrameHasSignal(const uint8_t *data, size_t length)
{
  if (data == nullptr || length == 0) {
    return false;
  }

  bool anyNonZero = false;
  bool anyNonZeroBeyondFirst = false;
  for (size_t i = 0; i < length; ++i) {
    if (data[i] == 0) {
      continue;
    }

    anyNonZero = true;
    if (i > 0) {
      anyNonZeroBeyondFirst = true;
      break;
    }
  }

  return anyNonZeroBeyondFirst || (anyNonZero && length == 1);
}

bool keyboardFrameIsAllZero(const uint8_t *data, size_t length)
{
  if (data == nullptr || length == 0) {
    return true;
  }

  for (size_t i = 0; i < length; ++i) {
    if (data[i] != 0) {
      return false;
    }
  }

  return true;
}

bool isKeyboardShiftActive(uint8_t modifiers)
{
  return (modifiers & 0x22) != 0;
}

bool keyboardReportContainsKey(const KeyboardBootReport &report, uint8_t keyCode)
{
  for (uint8_t key : report.keys) {
    if (key == keyCode) {
      return true;
    }
  }
  return false;
}

bool keyboardReportHasAnyKeys(const KeyboardBootReport &report)
{
  for (uint8_t key : report.keys) {
    if (key != 0) {
      return true;
    }
  }
  return false;
}

char decodeKeyboardPrintable(uint8_t keyCode, bool shifted)
{
  if (keyCode >= 0x04 && keyCode <= 0x1d) {
    char base = shifted ? 'A' : 'a';
    return static_cast<char>(base + (keyCode - 0x04));
  }

  if (keyCode >= 0x1e && keyCode <= 0x27) {
    static const char kDigitChars[] = "1234567890";
    static const char kShiftedDigitChars[] = "!@#$%^&*()";
    size_t index = keyCode - 0x1e;
    return shifted ? kShiftedDigitChars[index] : kDigitChars[index];
  }

  switch (keyCode) {
    case 0x2c: return ' ';
    case 0x2d: return shifted ? '_' : '-';
    case 0x2e: return shifted ? '+' : '=';
    case 0x2f: return shifted ? '{' : '[';
    case 0x30: return shifted ? '}' : ']';
    case 0x31: return shifted ? '|' : '\\';
    case 0x33: return shifted ? ':' : ';';
    case 0x34: return shifted ? '"' : '\'';
    case 0x35: return shifted ? '~' : '`';
    case 0x36: return shifted ? '<' : ',';
    case 0x37: return shifted ? '>' : '.';
    case 0x38: return shifted ? '?' : '/';
    default: return 0;
  }
}

String keyboardSpecialKeyLabel(uint8_t keyCode)
{
  switch (keyCode) {
    case 0x28: return "Enter";
    case 0x29: return "Escape";
    case 0x2a: return "Backspace";
    case 0x2b: return "Tab";
    case 0x39: return "Caps Lock";
    case 0x4f: return "Right";
    case 0x50: return "Left";
    case 0x51: return "Down";
    case 0x52: return "Up";
    default: {
      char buffer[16];
      snprintf(buffer, sizeof(buffer), "Key 0x%02X", keyCode);
      return String(buffer);
    }
  }
}

String keyboardModifierLabel(uint8_t modifiers)
{
  if ((modifiers & 0x22) != 0) {
    return "Shift";
  }
  if ((modifiers & 0x11) != 0) {
    return "Ctrl";
  }
  if ((modifiers & 0x44) != 0) {
    return "Alt";
  }
  if ((modifiers & 0x88) != 0) {
    return "Meta";
  }
  return "";
}

String keyboardKeyLabel(uint8_t keyCode, uint8_t modifiers)
{
  char printable = decodeKeyboardPrintable(keyCode, isKeyboardShiftActive(modifiers));
  if (printable != 0) {
    if (printable >= 'a' && printable <= 'z') {
      printable = static_cast<char>(printable - ('a' - 'A'));
    }
    return String(printable);
  }

  return keyboardSpecialKeyLabel(keyCode);
}

String keyboardLabelFromRawFrame(const uint8_t *data, size_t length)
{
  if (!keyboardRawFrameHasSignal(data, length)) {
    return "";
  }

  auto firstCandidateInRange = [&](size_t start, size_t end, uint8_t &keyCode) -> bool {
    size_t clampedEnd = min(end, length);
    for (size_t i = start; i < clampedEnd; ++i) {
      uint8_t value = data[i];
      if (value >= 0x04 && value <= 0x73) {
        keyCode = value;
        return true;
      }
    }
    return false;
  };

  uint8_t keyCode = 0;
  if (length >= 9 && data[2] == 0 && firstCandidateInRange(3, 9, keyCode)) {
    return keyboardKeyLabel(keyCode, data[1]);
  }

  if (length >= 8 && data[1] == 0 && firstCandidateInRange(2, 8, keyCode)) {
    return keyboardKeyLabel(keyCode, data[0]);
  }

  if (length >= 8 && firstCandidateInRange(2, 8, keyCode)) {
    return keyboardKeyLabel(keyCode, data[1]);
  }

  if (length >= 7 && firstCandidateInRange(1, 7, keyCode)) {
    return keyboardKeyLabel(keyCode, data[0]);
  }

  String modifierLabel = keyboardModifierLabel(data[0]);
  if (!modifierLabel.isEmpty()) {
    return modifierLabel;
  }

  return formatKeyboardBytes(data, length, 8);
}

String keyboardLabelFromUsageFrame(const uint8_t *data, size_t length)
{
  if (data == nullptr || length < 2) {
    return "";
  }

  for (size_t offset = 0; offset + 1 < length; ++offset) {
    uint16_t usage = static_cast<uint16_t>(data[offset]) |
                     (static_cast<uint16_t>(data[offset + 1]) << 8);
    if (usage < 0x04 || usage > 0x73) {
      continue;
    }

    uint8_t modifiers = 0;
    if (offset > 0) {
      modifiers = data[offset - 1];
    }

    String label = keyboardKeyLabel(static_cast<uint8_t>(usage), modifiers);
    if (!label.isEmpty()) {
      return label;
    }
  }

  return "";
}

String formatKeyboardBytes(const uint8_t *data, size_t length, size_t maxBytes)
{
  if (data == nullptr || length == 0) {
    return "no data";
  }

  String formatted;
  size_t bytesToShow = min(length, maxBytes);
  for (size_t i = 0; i < bytesToShow; ++i) {
    if (i > 0) {
      formatted += ' ';
    }

    char chunk[4];
    snprintf(chunk, sizeof(chunk), "%02X", data[i]);
    formatted += chunk;
  }

  if (length > bytesToShow) {
    formatted += " ...";
  }

  return formatted;
}

String keyboardNotificationSignature(BLERemoteCharacteristic *characteristic, const uint8_t *data, size_t length)
{
  String signature;
  if (characteristic != nullptr) {
    char prefix[16];
    snprintf(prefix, sizeof(prefix), "%04X:", characteristic->getHandle());
    signature = prefix;
  }

  signature += formatKeyboardBytes(data, length, 12);
  return signature;
}

bool keyboardShouldSuppressDuplicateNotification(const String &signature)
{
  uint32_t now = millis();
  if (signature == keyboardLastNotificationSignature &&
      now - keyboardLastNotificationAtMs < kKeyboardNotificationDedupMs) {
    return true;
  }

  keyboardLastNotificationSignature = signature;
  keyboardLastNotificationAtMs = now;
  return false;
}

String keyboardDebugToastText(BLERemoteCharacteristic *characteristic,
                              const uint8_t *data,
                              size_t length,
                              const String &preferredLabel)
{
  if (!preferredLabel.isEmpty()) {
    return preferredLabel;
  }

  String usageLabel = keyboardLabelFromUsageFrame(data, length);
  if (!usageLabel.isEmpty()) {
    return usageLabel;
  }

  if (keyboardFrameIsAllZero(data, length)) {
    return "";
  }

  String summary;
  if (characteristic != nullptr) {
    char prefix[10];
    snprintf(prefix, sizeof(prefix), "H%04X ", characteristic->getHandle());
    summary = prefix;
  }

  summary += formatKeyboardBytes(data, length, 8);
  return summary;
}

void pushKeyboardPreviewChar(char value)
{
  keyboardPreviewText += value;
  if (keyboardPreviewText.length() > kKeyboardPreviewMaxLength) {
    keyboardPreviewText.remove(0, keyboardPreviewText.length() - kKeyboardPreviewMaxLength);
  }
}

void showKeyboardToast(const String &text, uint32_t durationMs)
{
  keyboardToastText = text;
  keyboardToastUntilMs = millis() + durationMs;
  screenDirty = true;
}

void handleKeyboardKeyPress(uint8_t keyCode, uint8_t modifiers)
{
  noteActivity();

  if (keyCode == 0) {
    String modifierLabel = keyboardModifierLabel(modifiers);
    if (!modifierLabel.isEmpty()) {
      Serial.printf("K808 modifier: %s\n", modifierLabel.c_str());
      keyboardLastKeyToastAtMs = millis();
      showKeyboardToast(modifierLabel);
    }
    return;
  }

  String label = keyboardKeyLabel(keyCode, modifiers);
  Serial.printf("K808 key: %s\n", label.c_str());
  keyboardLastKeyToastAtMs = millis();
  showKeyboardToast(label);
}

bool processKeyboardReport(const KeyboardBootReport &report)
{
  bool showedKey = false;
  for (uint8_t keyCode : report.keys) {
    if (keyCode == 0 || (keyboardHaveLastReport && keyboardReportContainsKey(keyboardLastReport, keyCode))) {
      continue;
    }
    handleKeyboardKeyPress(keyCode, report.modifiers);
    showedKey = true;
  }

  if (!showedKey && !keyboardReportHasAnyKeys(report) &&
      (!keyboardHaveLastReport || keyboardLastReport.modifiers != report.modifiers)) {
    handleKeyboardKeyPress(0, report.modifiers);
    showedKey = report.modifiers != 0;
  }

  keyboardLastReport = report;
  keyboardHaveLastReport = true;
  return showedKey;
}

bool extractKeyboardBootReport(const uint8_t *data, size_t length, KeyboardBootReport &report)
{
  if (data == nullptr || length == 0) {
    return false;
  }

  auto assignKeyboardReport = [&](uint8_t modifiers, uint8_t reserved, const uint8_t *keys) {
    report.modifiers = modifiers;
    report.reserved = reserved;
    memcpy(report.keys, keys, sizeof(report.keys));
  };

  if (length >= 9) {
    for (size_t offset = 0; offset + 9 <= length; ++offset) {
      const uint8_t *window = data + offset;
      if (window[2] != 0 || !keyboardKeyArrayLooksValid(window + 3, sizeof(report.keys))) {
        continue;
      }

      assignKeyboardReport(window[1], window[2], window + 3);
      return true;
    }
  }

  if (length >= sizeof(KeyboardBootReport)) {
    for (size_t offset = 0; offset + sizeof(KeyboardBootReport) <= length; ++offset) {
      const uint8_t *window = data + offset;
      if (window[1] != 0 || !keyboardKeyArrayLooksValid(window + 2, sizeof(report.keys))) {
        continue;
      }

      assignKeyboardReport(window[0], window[1], window + 2);
      return true;
    }
  }

  if (length >= 8) {
    for (size_t offset = 0; offset + 8 <= length; ++offset) {
      const uint8_t *window = data + offset;
      if (!keyboardKeyArrayLooksValid(window + 2, sizeof(report.keys))) {
        continue;
      }

      assignKeyboardReport(window[1], 0, window + 2);
      return true;
    }
  }

  if (length >= 7) {
    for (size_t offset = 0; offset + 7 <= length; ++offset) {
      const uint8_t *window = data + offset;
      if (!keyboardKeyArrayLooksValid(window + 1, sizeof(report.keys))) {
        continue;
      }

      assignKeyboardReport(window[0], 0, window + 1);
      return true;
    }
  }

  return false;
}

void keyboardNotifyCallback(BLERemoteCharacteristic *pRemoteCharacteristic, uint8_t *data, size_t length, bool)
{
  if (length == 0 || data == nullptr) {
    return;
  }

  markKeyboardActivity();
  noteActivity();

  String notificationSignature = keyboardNotificationSignature(pRemoteCharacteristic, data, length);
  if (keyboardShouldSuppressDuplicateNotification(notificationSignature)) {
    return;
  }

  KeyboardBootReport report = {};
  if (!extractKeyboardBootReport(data, length, report)) {
    String rawLabel = keyboardLabelFromRawFrame(data, length);
    String debugLabel = keyboardDebugToastText(pRemoteCharacteristic, data, length, rawLabel);
    if (debugLabel.isEmpty()) {
      return;
    }

    Serial.printf("K808 report handle 0x%04X: %u bytes [%s]\n",
                  pRemoteCharacteristic->getHandle(),
                  static_cast<unsigned>(length),
                  debugLabel.c_str());
    showKeyboardToast(debugLabel);
    return;
  }

  if (!processKeyboardReport(report)) {
    String rawLabel = keyboardLabelFromRawFrame(data, length);
    String debugLabel = keyboardDebugToastText(pRemoteCharacteristic, data, length, rawLabel);
    if (debugLabel.isEmpty()) {
      return;
    }
    if (millis() - keyboardLastKeyToastAtMs < 220) {
      return;
    }
    showKeyboardToast(debugLabel);
  }
}

bool keyboardMatchesTarget(BLEAdvertisedDevice &advertisedDevice)
{
  if (advertisedDevice.getAddress().equals(kKeyboardTargetAddress)) {
    return true;
  }

  if (advertisedDevice.haveName()) {
    String name = advertisedDevice.getName();
    return name.equalsIgnoreCase(kKeyboardTargetName) || name.indexOf(kKeyboardTargetName) >= 0;
  }

  return false;
}

void onKeyboardScanComplete(BLEScanResults)
{
  keyboardScanActive = false;
  if (!keyboardConnected && !keyboardDoConnect) {
    scheduleKeyboardRetry();
  }
}

class KeyboardClientCallbacks : public BLEClientCallbacks
{
  void onConnect(BLEClient *) override
  {
    Serial.println("K808 connected");
  }

  void onDisconnect(BLEClient *) override
  {
    keyboardConnected = false;
    keyboardDoConnect = false;
    keyboardHaveLastReport = false;
    keyboardLastReport = {};
    keyboardLastNotificationSignature = "";
    keyboardLastNotificationAtMs = 0;
    keyboardLastKeyToastAtMs = 0;
    showKeyboardToast("K808 disconnected");
    scheduleKeyboardRetry(2000);
    Serial.println("K808 disconnected");
  }
};

class KeyboardAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice) override
  {
    if (!keyboardMatchesTarget(advertisedDevice)) {
      return;
    }

    Serial.printf("Found target keyboard: %s\n", advertisedDevice.toString().c_str());
    keyboardScanActive = false;
    BLEDevice::getScan()->stop();

    if (keyboardAdvertisedDevice != nullptr) {
      delete keyboardAdvertisedDevice;
      keyboardAdvertisedDevice = nullptr;
    }

    keyboardAdvertisedDevice = new BLEAdvertisedDevice(advertisedDevice);
    keyboardDoConnect = true;
  }
};

KeyboardClientCallbacks keyboardClientCallbacks;
KeyboardAdvertisedDeviceCallbacks keyboardAdvertisedDeviceCallbacks;

void scheduleKeyboardRetry(uint32_t delayMs)
{
  keyboardNextRetryAtMs = millis() + delayMs;
}

void startKeyboardScan()
{
  if (!keyboardBleReady || keyboardScan == nullptr || keyboardConnected || keyboardScanActive || otaInProgress || inLightSleep) {
    return;
  }

  Serial.println("Scanning for K808...");
  bool started = keyboardScan->start(kKeyboardScanDurationSeconds, onKeyboardScanComplete, false);
  keyboardScanActive = started;

  if (!started) {
    Serial.println("Failed to start BLE scan for K808");
    scheduleKeyboardRetry(4000);
  }
}

bool registerKeyboardInputReport(BLERemoteCharacteristic *characteristic)
{
  if (characteristic == nullptr || !characteristic->canNotify()) {
    return false;
  }

  characteristic->setAuth(ESP_GATT_AUTH_REQ_NO_MITM);

  BLERemoteDescriptor *reportReferenceDescriptor =
      characteristic->getDescriptor(kKeyboardReportReferenceDescriptorUuid);
  if (reportReferenceDescriptor != nullptr) {
    reportReferenceDescriptor->setAuth(ESP_GATT_AUTH_REQ_NO_MITM);
    String reportReference = reportReferenceDescriptor->readValue();
    if (reportReference.length() >= 2) {
      uint8_t reportId = static_cast<uint8_t>(reportReference[0]);
      uint8_t reportType = static_cast<uint8_t>(reportReference[1]);
      if (reportType != 1) {
        Serial.printf("Skipping non-input report type %u on handle 0x%04X\n", reportType, characteristic->getHandle());
        return false;
      }
      Serial.printf("Input report handle 0x%04X uses report id %u\n",
                    characteristic->getHandle(),
                    reportId);
    }
  }

  characteristic->registerForNotify(keyboardNotifyCallback);
  Serial.printf("Subscribed to keyboard input report handle 0x%04X\n", characteristic->getHandle());
  return true;
}

bool connectToKeyboard()
{
  if (!keyboardBleReady || keyboardAdvertisedDevice == nullptr) {
    return false;
  }

  Serial.printf("Connecting to K808 at %s\n", keyboardAdvertisedDevice->getAddress().toString().c_str());

  if (keyboardClient == nullptr) {
    keyboardClient = BLEDevice::createClient();
    keyboardClient->setClientCallbacks(&keyboardClientCallbacks);
  }

  if (!keyboardClient->connect(keyboardAdvertisedDevice)) {
    Serial.println("Failed to connect to K808");
    return false;
  }

  keyboardClient->setMTU(185);

  BLERemoteService *hidService = keyboardClient->getService(kKeyboardHidServiceUuid);
  if (hidService == nullptr) {
    Serial.println("K808 did not expose HID service");
    keyboardClient->disconnect();
    return false;
  }

  size_t subscriptionCount = 0;
  BLERemoteCharacteristic *protocolModeCharacteristic =
      hidService->getCharacteristic(kKeyboardProtocolModeCharacteristicUuid);
  if (protocolModeCharacteristic != nullptr && protocolModeCharacteristic->canWrite()) {
    protocolModeCharacteristic->setAuth(ESP_GATT_AUTH_REQ_NO_MITM);
    protocolModeCharacteristic->writeValue(static_cast<uint8_t>(0), true);
    Serial.println("Requested HID boot protocol mode");
  }

  BLERemoteCharacteristic *bootInputCharacteristic = hidService->getCharacteristic(kKeyboardBootInputCharacteristicUuid);
  if (registerKeyboardInputReport(bootInputCharacteristic)) {
    ++subscriptionCount;
    Serial.println("Using boot keyboard input path");
  }

  std::map<std::string, BLERemoteCharacteristic *> *characteristics = hidService->getCharacteristics();
  for (auto const &entry : *characteristics) {
    BLERemoteCharacteristic *characteristic = entry.second;
    if (!characteristic->getUUID().equals(kKeyboardReportCharacteristicUuid)) {
      continue;
    }

    if (registerKeyboardInputReport(characteristic)) {
      ++subscriptionCount;
    }
  }

  if (subscriptionCount == 0) {
    Serial.println("No usable keyboard input report characteristics found");
    keyboardClient->disconnect();
    return false;
  }

  keyboardConnected = true;
  keyboardHaveLastReport = false;
  keyboardLastReport = {};
  keyboardLastNotificationSignature = "";
  keyboardLastNotificationAtMs = 0;
  keyboardLastKeyToastAtMs = 0;
  showKeyboardToast("K808 connected");
  Serial.println("K808 is ready");
  return true;
}

void initKeyboardBle()
{
  if (keyboardBleReady) {
    return;
  }

  BLEDevice::init("Waveform");

  BLESecurity *security = new BLESecurity();
  security->setCapability(ESP_IO_CAP_NONE);
  security->setAuthenticationMode(true, false, true);
  BLEDevice::setSecurityCallbacks(new BLESecurityCallbacks());

  keyboardScan = BLEDevice::getScan();
  keyboardScan->setAdvertisedDeviceCallbacks(&keyboardAdvertisedDeviceCallbacks);
  keyboardScan->setInterval(1349);
  keyboardScan->setWindow(449);
  keyboardScan->setActiveScan(true);

  keyboardBleReady = true;
  scheduleKeyboardRetry(1200);
}

void updateKeyboard()
{
  if (!keyboardBleReady || otaInProgress || inLightSleep) {
    return;
  }

  if (keyboardDoConnect) {
    keyboardDoConnect = false;
    if (!connectToKeyboard()) {
      scheduleKeyboardRetry(3000);
    }
    return;
  }

  if (keyboardConnected || keyboardScanActive) {
    return;
  }

  if (static_cast<int32_t>(millis() - keyboardNextRetryAtMs) < 0) {
    return;
  }

  startKeyboardScan();
}

void applyTimeZone()
{
  setenv("TZ", TIMEZONE_POSIX, 1);
  tzset();
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

bool networkIsOnline()
{
  return connectivityState == ConnectivityState::Online && WiFi.status() == WL_CONNECTED;
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

void startPowerButtonHold()
{
  powerButtonHoldActive = true;
  powerButtonShutdownTriggered = false;
  powerButtonHoldStartedAtMs = millis();
  lastRenderedPowerHoldTenths = -1;
  noteActivity();
  screenDirty = true;
  lastRenderedScreen = ScreenId::Count;
}

void cancelPowerButtonHold()
{
  if (!powerButtonHoldActive && !powerButtonShutdownTriggered) {
    return;
  }

  powerButtonHoldActive = false;
  powerButtonShutdownTriggered = false;
  powerButtonHoldStartedAtMs = 0;
  lastRenderedPowerHoldTenths = -1;
  noteActivity();
  screenDirty = true;
  lastRenderedScreen = ScreenId::Count;
}

void updatePowerButtonHold()
{
  if (!powerButtonHoldActive || powerButtonShutdownTriggered) {
    return;
  }

  screenDirty = true;
  if (millis() - powerButtonHoldStartedAtMs >= kPowerButtonShutdownHoldMs) {
    triggerPowerButtonShutdown();
  }
}

bool pointInRect(int16_t x, int16_t y, const Rect &rect)
{
  return x >= rect.x && x < rect.x + rect.width &&
         y >= rect.y && y < rect.y + rect.height;
}

float blendMotionValue(float previousValue, float newValue)
{
  return previousValue + (newValue - previousValue) * kMotionDisplaySmoothingAlpha;
}

float blendMotionReadoutValue(float previousValue, float newValue)
{
  return previousValue + (newValue - previousValue) * kMotionReadoutSmoothingAlpha;
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

Vec3 scaleVec3(const Vec3 &v, float scalar)
{
  return {v.x * scalar, v.y * scalar, v.z * scalar};
}

float dotVec3(const Vec3 &a, const Vec3 &b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 crossVec3(const Vec3 &a, const Vec3 &b)
{
  return {
      a.y * b.z - a.z * b.y,
      a.z * b.x - a.x * b.z,
      a.x * b.y - a.y * b.x,
  };
}

float lengthVec3(const Vec3 &v)
{
  return sqrtf(dotVec3(v, v));
}

Vec3 normalizeVec3(const Vec3 &v)
{
  float magnitude = lengthVec3(v);
  if (magnitude < 0.0001f) {
    return {0.0f, 0.0f, 0.0f};
  }

  return scaleVec3(v, 1.0f / magnitude);
}

Vec3 normalizedDownVector(const MotionState &state)
{
  Vec3 down = normalizeVec3(vec3(state.ax, state.ay, state.az));
  if (lengthVec3(down) < 0.1f) {
    return {0.0f, 0.0f, 1.0f};
  }
  return down;
}

Vec3 projectOntoPlane(const Vec3 &vector, const Vec3 &normal)
{
  return subtractVec3(vector, scaleVec3(normal, dotVec3(vector, normal)));
}

Vec3 stablePerpendicular(const Vec3 &down, const Vec3 &preferred)
{
  Vec3 projected = normalizeVec3(projectOntoPlane(preferred, down));
  if (lengthVec3(projected) >= 0.1f) {
    return projected;
  }

  Vec3 fallback = fabsf(down.x) < 0.85f ? vec3(1.0f, 0.0f, 0.0f) : vec3(0.0f, 1.0f, 0.0f);
  projected = normalizeVec3(projectOntoPlane(fallback, down));
  if (lengthVec3(projected) >= 0.1f) {
    return projected;
  }

  return {1.0f, 0.0f, 0.0f};
}

void captureMotionReferenceFrame()
{
  if (!motionState.valid) {
    motionReferenceReady = false;
    return;
  }

  motionReferenceDown = normalizedDownVector(motionState);
  motionReferenceAxisA = stablePerpendicular(motionReferenceDown, vec3(1.0f, 0.0f, 0.0f));
  motionReferenceAxisB = normalizeVec3(crossVec3(motionReferenceDown, motionReferenceAxisA));
  if (lengthVec3(motionReferenceAxisB) < 0.1f) {
    motionReferenceAxisB = stablePerpendicular(motionReferenceDown, vec3(0.0f, 1.0f, 0.0f));
  }
  motionReferenceReady = true;
}

void motionCubeBasis(const MotionState &state, Vec3 &down, Vec3 &axisA, Vec3 &axisB)
{
  down = normalizedDownVector(state);

  if (!motionReferenceReady) {
    captureMotionReferenceFrame();
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

float normalizeMotionIndicatorOffset(float offsetDegrees)
{
  float magnitude = fabsf(offsetDegrees);
  if (magnitude <= kMotionIndicatorDeadzoneDegrees) {
    return 0.0f;
  }

  float adjustedMagnitude = (magnitude - kMotionIndicatorDeadzoneDegrees) /
                            (kMotionIndicatorFullScaleDegrees - kMotionIndicatorDeadzoneDegrees);
  adjustedMagnitude = constrain(adjustedMagnitude, 0.0f, 1.0f);
  return copysignf(adjustedMagnitude, offsetDegrees);
}

void resetMotionIndicatorFilter()
{
  motionIndicatorFilterReady = false;
  motionIndicatorPitchOffset = 0.0f;
  motionIndicatorRollOffset = 0.0f;
}

void updateMotionIndicatorFilter(float pitch, float roll)
{
  float pitchOffset = pitch - motionPitchZero;
  float rollOffset = roll - motionRollZero;

  if (!motionIndicatorFilterReady) {
    motionIndicatorPitchOffset = pitchOffset;
    motionIndicatorRollOffset = rollOffset;
    motionIndicatorFilterReady = true;
    return;
  }

  motionIndicatorPitchOffset += (pitchOffset - motionIndicatorPitchOffset) * kMotionIndicatorSmoothingAlpha;
  motionIndicatorRollOffset += (rollOffset - motionIndicatorRollOffset) * kMotionIndicatorSmoothingAlpha;
}

MotionIndicatorPosition motionIndicatorPositionForRect(const Rect &rect, float pitch, float roll)
{
  int16_t centerX = rect.x + rect.width / 2;
  int16_t centerY = rect.y + rect.height / 2;
  int16_t travel = min(rect.width, rect.height) / 2 - 24;

  if (!motionIndicatorFilterReady) {
    updateMotionIndicatorFilter(pitch, roll);
  }

  float normalizedRoll = normalizeMotionIndicatorOffset(motionIndicatorRollOffset);
  float normalizedPitch = normalizeMotionIndicatorOffset(motionIndicatorPitchOffset);

  MotionIndicatorPosition position;
  position.x = centerX + static_cast<int16_t>(normalizedRoll * travel);
  position.y = centerY - static_cast<int16_t>(normalizedPitch * travel);
  return position;
}

MotionIndicatorPosition motionIndicatorPosition(float pitch, float roll)
{
  return motionIndicatorPositionForRect(kMotionDotRect, pitch, roll);
}

void updateMotionDisplayState()
{
  if (!motionState.valid) {
    motionDisplayState.valid = false;
    return;
  }

  if (!motionDisplayState.valid) {
    motionDisplayState = motionState;
    return;
  }

  motionDisplayState.valid = true;
  motionDisplayState.ax = blendMotionReadoutValue(motionDisplayState.ax, motionState.ax);
  motionDisplayState.ay = blendMotionReadoutValue(motionDisplayState.ay, motionState.ay);
  motionDisplayState.az = blendMotionReadoutValue(motionDisplayState.az, motionState.az);
  motionDisplayState.gx = blendMotionReadoutValue(motionDisplayState.gx, motionState.gx);
  motionDisplayState.gy = blendMotionReadoutValue(motionDisplayState.gy, motionState.gy);
  motionDisplayState.gz = blendMotionReadoutValue(motionDisplayState.gz, motionState.gz);
  motionDisplayState.pitch = blendMotionReadoutValue(motionDisplayState.pitch, motionState.pitch);
  motionDisplayState.roll = blendMotionReadoutValue(motionDisplayState.roll, motionState.roll);
  snprintf(motionDisplayState.orientation, sizeof(motionDisplayState.orientation), "%s", motionState.orientation);
}

void handleTouchInterrupt()
{
  if (touchController) {
    touchController->IIC_Interrupt_Flag = true;
  }
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
  touchTapPending = false;
  return true;
}

void setOrientationLabel(float ax, float ay, float az)
{
  float absX = fabsf(ax);
  float absY = fabsf(ay);
  float absZ = fabsf(az);

  const char *label = "Portrait";
  if (absZ >= absX && absZ >= absY) {
    label = az >= 0.0f ? "Face Down" : "Face Up";
  } else if (absY >= absX) {
    label = ay >= 0.0f ? "Landscape Right" : "Landscape Left";
  } else if (ax >= 0.0f) {
    label = "Upside Down";
  }

  snprintf(motionState.orientation, sizeof(motionState.orientation), "%s", label);
}

bool initImu()
{
  imuReady = imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (!imuReady) {
    Serial.println("QMI8658 not found; motion features disabled");
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

  haveLastAccelSample = false;
  lastImuSampleAtMs = 0;
  motionState.valid = false;
  resetMotionIndicatorFilter();
  return true;
}

bool configureImuWakeOnMotion()
{
  if (!imuReady) {
    return false;
  }

  if (!imu.configWakeOnMotion(kWakeOnMotionThresholdMg,
                              SensorQMI8658::ACC_ODR_LOWPOWER_128Hz,
                              SensorQMI8658::INTERRUPT_PIN_2)) {
    Serial.println("QMI8658 wake-on-motion configuration failed");
    return false;
  }

  pinMode(IMU_IRQ, INPUT_PULLUP);
  return true;
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

  if (motionState.valid) {
    ax = blendMotionValue(motionState.ax, rawAx);
    ay = blendMotionValue(motionState.ay, rawAy);
    az = blendMotionValue(motionState.az, rawAz);
    gx = blendMotionValue(motionState.gx, rawGx);
    gy = blendMotionValue(motionState.gy, rawGy);
    gz = blendMotionValue(motionState.gz, rawGz);
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
  updateMotionIndicatorFilter(motionState.pitch, motionState.roll);

  if (!haveLastAccelSample) {
    lastAccelX = rawAx;
    lastAccelY = rawAy;
    lastAccelZ = rawAz;
    haveLastAccelSample = true;
    if (currentScreen == ScreenId::Motion) {
      screenDirty = true;
    }
    return;
  }

  float delta = fabsf(rawAx - lastAccelX) + fabsf(rawAy - lastAccelY) + fabsf(rawAz - lastAccelZ);
  lastAccelX = rawAx;
  lastAccelY = rawAy;
  lastAccelZ = rawAz;

  if (delta >= kMotionDeltaThreshold) {
    noteActivity();
  }

  if (currentScreen == ScreenId::Motion) {
    screenDirty = true;
  }
}

void updateTouchActivity()
{
  if (digitalRead(TP_INT) == LOW) {
    noteActivity();
  }
}

void updateTouchInput()
{
  if (!touchReady) {
    updateTouchActivity();
    return;
  }

  bool shouldRead = touchPressed || touchController->IIC_Interrupt_Flag || digitalRead(TP_INT) == LOW;
  if (!shouldRead) {
    return;
  }

  int fingers = static_cast<int>(touchController->IIC_Read_Device_Value(
      touchController->Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER));
  bool pressedNow = fingers > 0;

  if (pressedNow) {
    touchLastX = static_cast<int16_t>(touchController->IIC_Read_Device_Value(
        touchController->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X));
    touchLastY = static_cast<int16_t>(touchController->IIC_Read_Device_Value(
        touchController->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y));
    noteActivity();
  } else if (touchPressed) {
    uint32_t now = millis();
    if (now - lastTouchTapAtMs >= kTouchDebounceMs) {
      touchTapPending = true;
      lastTouchTapAtMs = now;
    }
  }

  touchPressed = pressedNow;
  touchController->IIC_Interrupt_Flag = false;
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

  struct timeval tv = {
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
                  rtcTime.tm_year + 1900, rtcTime.tm_mon + 1, rtcTime.tm_mday,
                  rtcTime.tm_hour, rtcTime.tm_min, rtcTime.tm_sec);
  }
}

void initExpander()
{
  expanderReady = expander.begin(kI2cExpanderAddress);
  if (!expanderReady) {
    Serial.println("I2C expander not found; continuing without it");
    return;
  }

  expander.pinMode(kPmuIrqPin, INPUT);
  expander.pinMode(kTopButtonPin, INPUT);
  expander.pinMode(1, OUTPUT);
  expander.pinMode(2, OUTPUT);
  expander.digitalWrite(1, LOW);
  expander.digitalWrite(2, LOW);
  delay(20);
  expander.digitalWrite(1, HIGH);
  expander.digitalWrite(2, HIGH);
  sideButtonPressed = expander.digitalRead(kTopButtonPin) == HIGH;
  sideButtonReleaseArmed = false;
  sideButtonPressedAtMs = millis();
}

void initPower()
{
  powerReady = power.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (!powerReady) {
    Serial.println("PMU not found; battery and USB status unavailable");
    return;
  }

  power.enableBattDetection();
  power.enableVbusVoltageMeasure();
  power.enableBattVoltageMeasure();
  power.enableSystemVoltageMeasure();
  power.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  power.clearIrqStatus();
  power.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
  power.enableIRQ(XPOWERS_AXP2101_PKEY_POSITIVE_IRQ |
                  XPOWERS_AXP2101_PKEY_NEGATIVE_IRQ |
                  XPOWERS_AXP2101_PKEY_LONG_IRQ);
}

void initRtc()
{
  rtcReady = rtc.begin(Wire, IIC_SDA, IIC_SCL);
  if (!rtcReady) {
    Serial.println("RTC not found; boot time requires Wi-Fi");
    return;
  }

  loadTimeFromRtc();
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
    hasRenderedWeatherScreen = false;
  }

  Serial.printf("Offline mode: %s\n", reason);
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
        beginOtaScreen("Receiving firmware", "do not power off", 0);
      })
      .onEnd([]() {
        Serial.println("\nOTA complete");
        updateOtaScreen("Finishing update", "restarting", 100);
        renderOtaScreen(true);
        endOtaScreen(0);
        delay(350);
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        static int lastPercent = -1;
        static int lastLoggedPercent = -1;
        int percent = static_cast<int>((progress * 100U) / total);
        if (percent != lastPercent) {
          updateOtaScreen("Receiving firmware", "do not power off", percent);
          lastPercent = percent;
        }
        if (percent != lastLoggedPercent && percent % 10 == 0) {
          lastLoggedPercent = percent;
          Serial.printf("OTA progress: %d%%\n", percent);
        }
      })
      .onError([](ota_error_t error) {
        Serial.printf("OTA error[%u]\n", error);
        String message = "update failed";
        switch (error) {
          case OTA_AUTH_ERROR:
            message = "auth failed";
            break;
          case OTA_BEGIN_ERROR:
            message = "begin failed";
            break;
          case OTA_CONNECT_ERROR:
            message = "connect failed";
            break;
          case OTA_RECEIVE_ERROR:
            message = "receive failed";
            break;
          case OTA_END_ERROR:
            message = "finalize failed";
            break;
        }
        updateOtaScreen(message, "returning to watch face", otaProgressPercent);
        renderOtaScreen(true);
        endOtaScreen(2200);
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
      screenDirty = true;
      Serial.printf("Wi-Fi connected to \"%s\": %s\n",
                    WiFi.SSID().c_str(),
                    WiFi.localIP().toString().c_str());
    }
    requestNtpSync();
    startOta();
    return;
  }

  if (connectivityState == ConnectivityState::Online) {
    setOfflineMode("connection lost; rendering continues from local state");
    nextWifiRetryAtMs = millis();
    screenDirty = true;
  }

  if (wifiAttemptInProgress) {
    if (millis() - wifiConnectStartedMs < kWifiConnectTimeoutMs) {
      return;
    }

    Serial.printf("Timed out on \"%s\"\n", kWiFiCredentials[currentCredentialIndex].ssid);
    wifiAttemptInProgress = false;

    while (nextCredentialIndex < (sizeof(kWiFiCredentials) / sizeof(kWiFiCredentials[0]))) {
      if (strlen(kWiFiCredentials[nextCredentialIndex].ssid) > 0) {
        beginWifiAttempt(nextCredentialIndex++);
        return;
      }
      ++nextCredentialIndex;
    }

    setOfflineMode("all Wi-Fi networks unavailable; retrying in background");
    nextWifiRetryAtMs = millis() + kWifiRetryIntervalMs;
    screenDirty = true;
    return;
  }

  if (static_cast<int32_t>(millis() - nextWifiRetryAtMs) < 0) {
    return;
  }

  beginNextWifiCycle();
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

String weatherConditionLabel(int weatherCode, bool isDay)
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
        weatherState.condition = weatherConditionLabel(weatherState.weatherCode, weatherState.isDay);
        weatherState.updated = weatherUpdatedLabel();
        weatherState.sunrise =
            !sunriseArray.isNull() && sunriseArray.size() > 0 ? weatherTimeFragment(sunriseArray[0].as<const char *>()) : "--:--";
        weatherState.sunset =
            !sunsetArray.isNull() && sunsetArray.size() > 0 ? weatherTimeFragment(sunsetArray[0].as<const char *>()) : "--:--";

        success = true;
        nextWeatherRefreshAtMs = millis() + kWeatherRefreshIntervalMs;
        hasRenderedWeatherScreen = false;
        screenDirty = true;
        Serial.printf("Weather updated: %s, %dF\n",
                      weatherState.condition.c_str(),
                      weatherState.temperatureF);
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
    hasRenderedWeatherScreen = false;
    screenDirty = true;
  }

  weatherFetchInProgress = false;
  return success;
}

void fetchWeatherIfDue()
{
  if (weatherFetchInProgress || otaInProgress || inLightSleep || !networkIsOnline()) {
    return;
  }

  if (static_cast<int32_t>(millis() - nextWeatherRefreshAtMs) < 0) {
    return;
  }

  fetchWeather();
}

void updateWeather()
{
  if (!networkIsOnline()) {
    if (weatherState.hasData && !weatherState.stale) {
      weatherState.stale = true;
      weatherState.updated = "Offline - cached weather";
      hasRenderedWeatherScreen = false;
      screenDirty = true;
    }
    return;
  }

  fetchWeatherIfDue();
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
  if (!batteryIsAvailable()) {
    return false;
  }

  int percent = power.getBatteryPercent();
  if (usbIsConnected() && percent >= 100) {
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

String dateText()
{
  if (!hasValidTime()) {
    return "Waiting for RTC or Wi-Fi";
  }

  char buffer[16];
  struct tm localTime = {};
  time_t now = time(nullptr);
  localtime_r(&now, &localTime);
  strftime(buffer, sizeof(buffer), "%a %d %m", &localTime);
  return String(buffer);
}

WatchfaceState buildWatchfaceState()
{
  return {
      .time = timeText(),
      .timezone = timezoneText(),
      .date = dateText(),
      .batteryPercent = batteryPercentValue(),
      .batteryAvailable = batteryIsAvailable(),
      .chargingActive = batteryIsCharging(),
      .wifiConnected = networkIsOnline(),
      .bluetoothConnected = keyboardConnected,
      .bluetoothActivityActive = keyboardActivityVisible(),
      .usbConnected = usbIsConnected(),
  };
}

uint16_t batteryIndicatorColor(int percent, bool charging)
{
  if (charging) {
    return kAccentBlueColor;
  }

  if (percent < 0) {
    return kBatteryUnavailableColor;
  }

  int clampedPercent = constrain(percent, 0, 100);
  uint8_t red = 0;
  uint8_t green = 0;

  if (clampedPercent <= 50) {
    red = 255;
    green = static_cast<uint8_t>((clampedPercent * 255) / 50);
  } else {
    red = static_cast<uint8_t>(((100 - clampedPercent) * 255) / 50);
    green = 255;
  }

  return rgb565(red, green, 0);
}

bool rectHasArea(const Rect &rect)
{
  return rect.width > 0 && rect.height > 0;
}

Rect intersectRect(const Rect &a, const Rect &b)
{
  int16_t left = max(a.x, b.x);
  int16_t top = max(a.y, b.y);
  int16_t right = min(static_cast<int16_t>(a.x + a.width), static_cast<int16_t>(b.x + b.width));
  int16_t bottom = min(static_cast<int16_t>(a.y + a.height), static_cast<int16_t>(b.y + b.height));

  if (right <= left || bottom <= top) {
    return {0, 0, 0, 0};
  }

  return {left, top, static_cast<int16_t>(right - left), static_cast<int16_t>(bottom - top)};
}

Rect unionRect(const Rect &a, const Rect &b)
{
  if (!rectHasArea(a)) {
    return b;
  }
  if (!rectHasArea(b)) {
    return a;
  }

  int16_t left = min(a.x, b.x);
  int16_t top = min(a.y, b.y);
  int16_t right = max(static_cast<int16_t>(a.x + a.width), static_cast<int16_t>(b.x + b.width));
  int16_t bottom = max(static_cast<int16_t>(a.y + a.height), static_cast<int16_t>(b.y + b.height));
  return {left, top, static_cast<int16_t>(right - left), static_cast<int16_t>(bottom - top)};
}

TextLayout layoutTextInRect(const Rect &rect,
                            const String &text,
                            const GFXfont *font,
                            TextAlign align,
                            uint8_t textScale = 1,
                            int16_t padding = 0)
{
  gfx->setFont(font);
  gfx->setTextSize(textScale, textScale, 0);
  gfx->setTextWrap(false);

  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t textWidth = 0;
  uint16_t textHeight = 0;
  gfx->getTextBounds(text.c_str(), 0, 0, &x1, &y1, &textWidth, &textHeight);

  int16_t cursorX = rect.x - x1;
  if (align == TextAlign::Center) {
    cursorX = rect.x + (rect.width - static_cast<int16_t>(textWidth)) / 2 - x1;
  } else if (align == TextAlign::Right) {
    cursorX = rect.x + rect.width - static_cast<int16_t>(textWidth) - x1;
  }

  int16_t cursorY = rect.y + (rect.height - static_cast<int16_t>(textHeight)) / 2 - y1;
  Rect bounds = {
      static_cast<int16_t>(cursorX + x1 - padding),
      static_cast<int16_t>(cursorY + y1 - padding),
      static_cast<int16_t>(static_cast<int16_t>(textWidth) + static_cast<int16_t>(padding * 2)),
      static_cast<int16_t>(static_cast<int16_t>(textHeight) + static_cast<int16_t>(padding * 2)),
  };
  Rect clippedBounds = intersectRect(bounds, rect);

  return {
      .cursorX = cursorX,
      .cursorY = cursorY,
      .bounds = clippedBounds,
  };
}

void drawTextInRect(const Rect &rect,
                    const String &text,
                    const GFXfont *font,
                    TextAlign align,
                    uint16_t textColor = kForegroundColor,
                    uint16_t backgroundColor = kBackgroundColor,
                    uint8_t textScale = 1,
                    bool clearBackground = true)
{
  if (clearBackground) {
    gfx->fillRect(rect.x, rect.y, rect.width, rect.height, backgroundColor);
  }

  TextLayout layout = layoutTextInRect(rect, text, font, align, textScale);
  gfx->setFont(font);
  gfx->setTextColor(textColor, backgroundColor);
  gfx->setTextSize(textScale, textScale, 0);
  gfx->setTextWrap(false);
  gfx->setCursor(layout.cursorX, layout.cursorY);
  gfx->print(text);
}

void drawChangingTextInRect(const Rect &rect,
                            const String &text,
                            const String &previousText,
                            const GFXfont *font,
                            TextAlign align,
                            uint16_t textColor = kForegroundColor,
                            uint16_t backgroundColor = kBackgroundColor,
                            uint8_t textScale = 1,
                            bool force = false)
{
  if (force) {
    drawTextInRect(rect, text, font, align, textColor, backgroundColor, textScale, true);
    return;
  }

  Rect currentBounds = layoutTextInRect(rect, text, font, align, textScale).bounds;
  Rect previousBounds = layoutTextInRect(rect, previousText, font, align, textScale).bounds;
  Rect dirtyRect = unionRect(currentBounds, previousBounds);
  if (rectHasArea(dirtyRect)) {
    gfx->fillRect(dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height, backgroundColor);
  }

  drawTextInRect(rect, text, font, align, textColor, backgroundColor, textScale, false);
}

void drawUtilityHeader(const String &title, const String &subtitle)
{
  drawTextInRect(kHeaderTitleRect, title, &FreeSansBold18pt7b, TextAlign::Left);
  drawTextInRect(kHeaderSubtitleRect, subtitle, &FreeSans9pt7b, TextAlign::Left, kMutedTextColor);
}

void drawArcSegment(int16_t centerX,
                    int16_t centerY,
                    int16_t radius,
                    float startDegrees,
                    float endDegrees,
                    uint16_t color)
{
  bool havePoint = false;
  int16_t previousX = 0;
  int16_t previousY = 0;

  for (int degrees = static_cast<int>(startDegrees); degrees <= static_cast<int>(endDegrees); degrees += 6) {
    float radians = degrees * PI / 180.0f;
    int16_t x = static_cast<int16_t>(roundf(centerX + cosf(radians) * radius));
    int16_t y = static_cast<int16_t>(roundf(centerY + sinf(radians) * radius));
    if (havePoint) {
      gfx->drawLine(previousX, previousY, x, y, color);
    }
    previousX = x;
    previousY = y;
    havePoint = true;
  }
}

void drawWiFiSymbol(const Rect &rect, bool connected)
{
  uint16_t color = connected ? kForegroundColor : kDisconnectedColor;
  int16_t centerX = rect.x + rect.width / 2;
  int16_t centerY = rect.y + rect.height - 6;

  drawArcSegment(centerX, centerY, 6, 220.0f, 320.0f, color);
  drawArcSegment(centerX, centerY, 10, 220.0f, 320.0f, color);
  drawArcSegment(centerX, centerY, 14, 220.0f, 320.0f, color);
  gfx->fillCircle(centerX, centerY, connected ? 3 : 2, color);

  if (!connected) {
    gfx->drawLine(rect.x + rect.width - 3, rect.y + 4, rect.x + 6, rect.y + rect.height - 4, color);
  }
}

void drawBluetoothSymbol(const Rect &rect, bool connected, bool active)
{
  uint16_t color = connected ? (active ? kAccentBlueColor : kForegroundColor) : kDisconnectedColor;
  int16_t centerX = rect.x + rect.width / 2;
  int16_t topY = rect.y + 2;
  int16_t middleY = rect.y + rect.height / 2;
  int16_t bottomY = rect.y + rect.height - 2;
  int16_t wingX = rect.x + rect.width - 4;
  int16_t innerX = rect.x + 6;

  gfx->drawLine(centerX, topY, centerX, bottomY, color);
  gfx->drawLine(centerX, topY, wingX, middleY - 5, color);
  gfx->drawLine(centerX, bottomY, wingX, middleY + 5, color);
  gfx->drawLine(centerX, middleY, wingX, middleY - 5, color);
  gfx->drawLine(centerX, middleY, wingX, middleY + 5, color);
  gfx->drawLine(centerX, topY, innerX, middleY - 5, color);
  gfx->drawLine(centerX, bottomY, innerX, middleY + 5, color);
}

void drawLightningBolt(int16_t x, int16_t y, uint16_t color)
{
  gfx->fillTriangle(x + 7, y, x, y + 11, x + 6, y + 11, color);
  gfx->fillTriangle(x + 8, y + 11, x + 4, y + 22, x + 13, y + 9, color);
}

void drawBatteryBar(const Rect &rect, int percent, bool available, uint16_t fillColor)
{
  gfx->fillRoundRect(rect.x, rect.y, rect.width, rect.height, rect.height / 2, kSurfaceColor);

  if (!available || percent <= 0) {
    return;
  }

  int16_t fillWidth = (rect.width * constrain(percent, 0, 100)) / 100;
  fillWidth = max(fillWidth, rect.height);
  fillWidth = min(fillWidth, rect.width);
  gfx->fillRoundRect(rect.x, rect.y, fillWidth, rect.height, rect.height / 2, fillColor);
}

void drawMotionDotBackground()
{
  int16_t centerX = kMotionDotRect.x + kMotionDotRect.width / 2;
  int16_t centerY = kMotionDotRect.y + kMotionDotRect.height / 2;
  int16_t travel = min(kMotionDotRect.width, kMotionDotRect.height) / 2 - 24;

  gfx->drawCircle(centerX, centerY, travel, kDividerColor);
  gfx->drawCircle(centerX, centerY, max<int16_t>(travel - 36, 12), kSurfaceColor);
  gfx->drawFastHLine(kMotionDotRect.x + 12, centerY, kMotionDotRect.width - 24, kDividerColor);
  gfx->drawFastVLine(centerX, kMotionDotRect.y + 12, kMotionDotRect.height - 24, kDividerColor);
  gfx->fillCircle(centerX, centerY, 5, kMutedTextColor);
}

void drawMotionDot(MotionIndicatorPosition dot)
{
  gfx->fillCircle(dot.x, dot.y, 14, kAccentBlueColor);
  gfx->drawCircle(dot.x, dot.y, 18, kForegroundColor);
}

Vec3 rotateForMotionCamera(const Vec3 &v)
{
  constexpr float yaw = 35.0f * PI / 180.0f;
  constexpr float pitch = -28.0f * PI / 180.0f;

  float cosYaw = cosf(yaw);
  float sinYaw = sinf(yaw);
  float cosPitch = cosf(pitch);
  float sinPitch = sinf(pitch);

  Vec3 yawRotated = {
      v.x * cosYaw - v.y * sinYaw,
      v.x * sinYaw + v.y * cosYaw,
      v.z,
  };

  return {
      yawRotated.x,
      yawRotated.y * cosPitch - yawRotated.z * sinPitch,
      yawRotated.y * sinPitch + yawRotated.z * cosPitch,
  };
}

MotionIndicatorPosition projectMotionPoint(const Vec3 &point, int16_t centerX, int16_t centerY, float scale)
{
  Vec3 rotated = rotateForMotionCamera(point);
  return {
      static_cast<int16_t>(centerX + rotated.x * scale),
      static_cast<int16_t>(centerY - rotated.y * scale),
  };
}

void drawProjectedArrow(const Vec3 &direction, int16_t centerX, int16_t centerY, float length, uint16_t color)
{
  Vec3 unit = normalizeVec3(direction);
  MotionIndicatorPosition tip = projectMotionPoint(scaleVec3(unit, length), centerX, centerY, 1.0f);
  gfx->drawLine(centerX, centerY, tip.x, tip.y, color);

  float dx = static_cast<float>(tip.x - centerX);
  float dy = static_cast<float>(tip.y - centerY);
  float magnitude = sqrtf(dx * dx + dy * dy);
  if (magnitude < 1.0f) {
    return;
  }

  float ux = dx / magnitude;
  float uy = dy / magnitude;
  float head = 12.0f;
  int16_t leftX = static_cast<int16_t>(tip.x - ux * head - uy * 6.0f);
  int16_t leftY = static_cast<int16_t>(tip.y - uy * head + ux * 6.0f);
  int16_t rightX = static_cast<int16_t>(tip.x - ux * head + uy * 6.0f);
  int16_t rightY = static_cast<int16_t>(tip.y - uy * head - ux * 6.0f);
  gfx->drawLine(tip.x, tip.y, leftX, leftY, color);
  gfx->drawLine(tip.x, tip.y, rightX, rightY, color);
}

void drawMotionCube(const MotionState &state)
{
  gfx->fillRect(kMotionFullRect.x, kMotionFullRect.y, kMotionFullRect.width, kMotionFullRect.height, kBackgroundColor);

  int16_t centerX = LCD_WIDTH / 2;
  int16_t centerY = LCD_HEIGHT / 2;
  float cubeScale = 54.0f;
  float arrowLength = 88.0f;

  Vec3 down;
  Vec3 axisA;
  Vec3 axisB;
  motionCubeBasis(state, down, axisA, axisB);
  Vec3 up = scaleVec3(down, -1.0f);

  Vec3 vertices[8];
  MotionIndicatorPosition projected[8];
  size_t index = 0;
  for (int sx = -1; sx <= 1; sx += 2) {
    for (int sy = -1; sy <= 1; sy += 2) {
      for (int sz = -1; sz <= 1; sz += 2) {
        vertices[index] = addVec3(addVec3(scaleVec3(axisA, static_cast<float>(sx)),
                                          scaleVec3(axisB, static_cast<float>(sy))),
                                  scaleVec3(up, static_cast<float>(sz)));
        projected[index] = projectMotionPoint(scaleVec3(vertices[index], cubeScale), centerX, centerY, 1.0f);
        ++index;
      }
    }
  }

  const uint8_t edges[][2] = {
      {0, 1}, {0, 2}, {0, 4},
      {1, 3}, {1, 5},
      {2, 3}, {2, 6},
      {3, 7},
      {4, 5}, {4, 6},
      {5, 7},
      {6, 7},
  };

  for (const auto &edge : edges) {
    gfx->drawLine(projected[edge[0]].x, projected[edge[0]].y, projected[edge[1]].x, projected[edge[1]].y, kDividerColor);
  }

  gfx->fillCircle(centerX, centerY, 4, kMutedTextColor);
  drawProjectedArrow(down, centerX, centerY, arrowLength, kAccentBlueColor);
  drawProjectedArrow(axisA, centerX, centerY, arrowLength * 0.92f, kForegroundColor);
  drawProjectedArrow(axisB, centerX, centerY, arrowLength * 0.92f, kMutedTextColor);
}

void drawStatusIcons(const WatchfaceState &state, uint16_t backgroundColor = kBackgroundColor)
{
  gfx->fillRect(kStatusIconsRect.x, kStatusIconsRect.y, kStatusIconsRect.width, kStatusIconsRect.height, backgroundColor);
  drawBluetoothSymbol(kBluetoothIconRect, state.bluetoothConnected, state.bluetoothActivityActive);
  drawWiFiSymbol(kWiFiIconRect, state.wifiConnected);
  uint16_t boltColor = state.chargingActive ? kAccentBlueColor : (state.usbConnected ? kForegroundColor : kDisconnectedColor);
  drawLightningBolt(kPowerIconRect.x + 5, kPowerIconRect.y + 4, boltColor);
}

uint16_t weatherSceneBackgroundColor(const WeatherState &state)
{
  WeatherSceneType scene = state.hasData ? weatherSceneTypeForCode(state.weatherCode) : WeatherSceneType::Cloudy;

  switch (scene) {
    case WeatherSceneType::Clear:
      return state.isDay ? kWeatherSkyDayColor : kWeatherSkyNightColor;
    case WeatherSceneType::PartlyCloudy:
      return state.isDay ? rgb565(10, 30, 60) : rgb565(6, 10, 18);
    case WeatherSceneType::Cloudy:
      return rgb565(10, 14, 22);
    case WeatherSceneType::Rain:
      return rgb565(8, 12, 20);
    case WeatherSceneType::Snow:
      return state.isDay ? rgb565(18, 22, 32) : rgb565(8, 10, 18);
    case WeatherSceneType::Storm:
      return rgb565(6, 8, 14);
    case WeatherSceneType::Fog:
      return rgb565(14, 16, 24);
    default:
      return kWeatherSkyNightColor;
  }
}

void drawCloudShape(int16_t x, int16_t y, int16_t unit, uint16_t bodyColor, uint16_t shadeColor)
{
  if (unit < 5) {
    unit = 5;
  }

  gfx->fillRoundRect(x + unit / 2, y + unit, unit * 4, unit * 2, unit, shadeColor);
  gfx->fillRoundRect(x, y + unit / 2, unit * 4, unit * 2, unit, bodyColor);
  gfx->fillCircle(x + unit, y + unit, unit, bodyColor);
  gfx->fillCircle(x + unit * 2, y + unit / 2, unit + 2, bodyColor);
  gfx->fillCircle(x + unit * 3, y + unit, unit + 1, bodyColor);
}

void drawStar(int16_t x, int16_t y, uint16_t color)
{
  gfx->drawPixel(x, y, color);
  gfx->drawPixel(x - 1, y, color);
  gfx->drawPixel(x + 1, y, color);
  gfx->drawPixel(x, y - 1, color);
  gfx->drawPixel(x, y + 1, color);
}

void drawWeatherCelestialBody(const WeatherState &state, uint16_t backgroundColor)
{
  float timeBase = millis() * 0.001f;
  int16_t centerX = static_cast<int16_t>(LCD_WIDTH * 0.75f + sinf(timeBase * 0.22f) * 8.0f);
  int16_t centerY = static_cast<int16_t>(72 + sinf(timeBase * 0.31f) * 6.0f);

  if (state.isDay) {
    gfx->fillCircle(centerX, centerY, 34, rgb565(96, 72, 0));
    gfx->fillCircle(centerX, centerY, 24, rgb565(180, 126, 0));
    gfx->fillCircle(centerX, centerY, 16, kWeatherSunColor);
    return;
  }

  gfx->fillCircle(centerX, centerY, 18, kWeatherMoonColor);
  gfx->fillCircle(centerX + 8, centerY - 5, 15, backgroundColor);

  for (int i = 0; i < 12; ++i) {
    float phase = timeBase * 0.7f + i * 0.83f;
    int16_t starX = 20 + (i * 31) % (LCD_WIDTH - 40);
    int16_t starY = 28 + static_cast<int16_t>((i * 17) % 72);
    if (sinf(phase) > 0.1f) {
      drawStar(starX, starY, rgb565(190, 208, 255));
    }
  }
}

void drawWeatherRain()
{
  float timeBase = millis() * 0.001f;
  for (int i = 0; i < 34; ++i) {
    float fall = fmodf(timeBase * 150.0f + i * 19.0f, kWeatherHeroRect.height + 30.0f);
    int16_t x = static_cast<int16_t>(12 + ((i * 29) + static_cast<int>(timeBase * 18.0f)) % (LCD_WIDTH - 24));
    int16_t y = static_cast<int16_t>(kWeatherHeroRect.y + fall) - 24;
    gfx->drawLine(x, y, x - 6, y + 14, kWeatherRainColor);
  }
}

void drawWeatherSnow()
{
  float timeBase = millis() * 0.001f;
  for (int i = 0; i < 24; ++i) {
    float fall = fmodf(timeBase * (28.0f + (i % 5) * 4.0f) + i * 23.0f, kWeatherHeroRect.height + 24.0f);
    int16_t x = static_cast<int16_t>(24 + (i * 37) % (LCD_WIDTH - 48) + sinf(timeBase * 0.9f + i) * 10.0f);
    int16_t y = static_cast<int16_t>(kWeatherHeroRect.y + fall) - 12;
    gfx->fillCircle(x, y, (i % 3 == 0) ? 3 : 2, kWeatherSnowColor);
  }
}

void drawWeatherFog()
{
  float timeBase = millis() * 0.001f;
  for (int i = 0; i < 4; ++i) {
    int16_t y = 86 + i * 30 + static_cast<int16_t>(sinf(timeBase * 0.6f + i) * 4.0f);
    int16_t inset = 10 + (i % 2) * 12;
    gfx->fillRoundRect(kWeatherHeroRect.x + inset, y, kWeatherHeroRect.width - inset * 2, 16, 8, kWeatherFogColor);
  }
}

void drawWeatherStormBolt()
{
  uint32_t cycle = millis() % 4800;
  bool flash = cycle < 110 || (cycle > 220 && cycle < 280);
  if (!flash) {
    return;
  }

  drawLightningBolt(LCD_WIDTH - 118, 88, kWeatherStormColor);
  drawLightningBolt(80, 110, kWeatherStormColor);
}

void drawWeatherHero(const WeatherState &state)
{
  WeatherSceneType scene = state.hasData ? weatherSceneTypeForCode(state.weatherCode) : WeatherSceneType::Cloudy;
  uint16_t backgroundColor = weatherSceneBackgroundColor(state);
  uint32_t cycle = millis() % 4800;
  if (scene == WeatherSceneType::Storm && (cycle < 110 || (cycle > 220 && cycle < 280))) {
    backgroundColor = rgb565(84, 88, 104);
  }

  gfx->fillRect(kWeatherHeroRect.x, kWeatherHeroRect.y, kWeatherHeroRect.width, kWeatherHeroRect.height, backgroundColor);

  if (scene != WeatherSceneType::Fog) {
    drawWeatherCelestialBody(state, backgroundColor);
  }

  float timeBase = millis() * 0.001f;
  int cloudCount = scene == WeatherSceneType::Cloudy ? 5 : (scene == WeatherSceneType::PartlyCloudy ? 3 : 4);
  for (int i = 0; i < cloudCount; ++i) {
    float drift = fmodf(timeBase * (10.0f + i * 2.0f) + i * 63.0f, LCD_WIDTH + 140.0f);
    int16_t x = static_cast<int16_t>(-100 + drift);
    int16_t y = static_cast<int16_t>(36 + i * 28 + sinf(timeBase * 0.45f + i) * 6.0f);
    int16_t unit = (scene == WeatherSceneType::Cloudy || scene == WeatherSceneType::Storm) ? 14 + (i % 2) * 2 : 11 + (i % 2) * 2;
    uint16_t bodyColor = (scene == WeatherSceneType::Storm) ? rgb565(122, 130, 146) : kWeatherCloudColor;
    uint16_t shadeColor = (scene == WeatherSceneType::Storm) ? rgb565(70, 76, 88) : kWeatherCloudShadeColor;
    drawCloudShape(x, y, unit, bodyColor, shadeColor);
  }

  if (scene == WeatherSceneType::Rain || scene == WeatherSceneType::Storm) {
    drawWeatherRain();
  }
  if (scene == WeatherSceneType::Snow) {
    drawWeatherSnow();
  }
  if (scene == WeatherSceneType::Fog) {
    drawWeatherFog();
  }
  if (scene == WeatherSceneType::Storm) {
    drawWeatherStormBolt();
  }

  gfx->fillRect(0, kWeatherHeroRect.y + kWeatherHeroRect.height - 24, LCD_WIDTH, 24, kBackgroundColor);
  gfx->drawFastHLine(0, kWeatherHeroRect.y + kWeatherHeroRect.height - 1, LCD_WIDTH, rgb565(18, 22, 30));

  WatchfaceState statusState = buildWatchfaceState();
  drawStatusIcons(statusState, backgroundColor);
  drawTextInRect(kWeatherLocationRect,
                 state.location,
                 &FreeSans9pt7b,
                 TextAlign::Left,
                 kForegroundColor,
                 backgroundColor);
}

void drawOtaProgressBar(int percent, bool force = false)
{
  if (force) {
    gfx->fillRect(kOtaBarOuterRect.x, kOtaBarOuterRect.y, kOtaBarOuterRect.width, kOtaBarOuterRect.height, kBackgroundColor);
    gfx->drawRect(kOtaBarOuterRect.x, kOtaBarOuterRect.y, kOtaBarOuterRect.width, kOtaBarOuterRect.height, kForegroundColor);
  }

  gfx->fillRect(kOtaBarInnerRect.x, kOtaBarInnerRect.y, kOtaBarInnerRect.width, kOtaBarInnerRect.height, kBackgroundColor);

  int filledWidth = (kOtaBarInnerRect.width * constrain(percent, 0, 100)) / 100;
  if (filledWidth > 0) {
    gfx->fillRect(kOtaBarInnerRect.x, kOtaBarInnerRect.y, filledWidth, kOtaBarInnerRect.height, kAccentBlueColor);
  }
}

void renderPowerHoldScreen(bool force)
{
  if (!powerButtonHoldActive && !powerButtonShutdownTriggered) {
    return;
  }

  if (force) {
    gfx->fillScreen(kBackgroundColor);
    drawTextInRect(kPowerHoldTitleRect, "Power Off", &FreeSansBold18pt7b, TextAlign::Center);
    drawTextInRect(kPowerHoldSubtitleRect, "keep holding", &FreeSans12pt7b, TextAlign::Center, kMutedTextColor);
    drawTextInRect(kPowerHoldHintRect, "release to cancel", &FreeSans9pt7b, TextAlign::Center, kMutedTextColor);
    gfx->fillRect(kPowerHoldBarOuterRect.x, kPowerHoldBarOuterRect.y, kPowerHoldBarOuterRect.width, kPowerHoldBarOuterRect.height, kBackgroundColor);
    gfx->drawRect(kPowerHoldBarOuterRect.x, kPowerHoldBarOuterRect.y, kPowerHoldBarOuterRect.width, kPowerHoldBarOuterRect.height, kForegroundColor);
  }

  uint32_t elapsedMs = powerButtonShutdownTriggered ? kPowerButtonShutdownHoldMs : (millis() - powerButtonHoldStartedAtMs);
  uint32_t remainingMs = elapsedMs >= kPowerButtonShutdownHoldMs ? 0 : (kPowerButtonShutdownHoldMs - elapsedMs);
  int remainingTenths = static_cast<int>((remainingMs + 99) / 100);
  if (remainingTenths != lastRenderedPowerHoldTenths || force) {
    char countdown[8];
    snprintf(countdown, sizeof(countdown), "%d.%d", remainingTenths / 10, remainingTenths % 10);
    drawTextInRect(kPowerHoldSecondsRect, String(countdown), &FreeSansBold24pt7b, TextAlign::Center);

    gfx->fillRect(kPowerHoldBarInnerRect.x, kPowerHoldBarInnerRect.y, kPowerHoldBarInnerRect.width, kPowerHoldBarInnerRect.height, kBackgroundColor);
    int filledWidth = (kPowerHoldBarInnerRect.width * static_cast<int>(remainingMs)) / static_cast<int>(kPowerButtonShutdownHoldMs);
    if (filledWidth > 0) {
      gfx->fillRect(kPowerHoldBarInnerRect.x, kPowerHoldBarInnerRect.y, filledWidth, kPowerHoldBarInnerRect.height, kForegroundColor);
    }

    lastRenderedPowerHoldTenths = remainingTenths;
  }
}

void triggerPowerButtonShutdown()
{
  if (powerButtonShutdownTriggered || !powerReady) {
    return;
  }

  powerButtonShutdownTriggered = true;
  renderPowerHoldScreen(true);
  drawTextInRect(kPowerHoldSubtitleRect, "powering off", &FreeSans12pt7b, TextAlign::Center, kMutedTextColor);
  drawTextInRect(kPowerHoldHintRect, "", &FreeSans9pt7b, TextAlign::Center, kMutedTextColor);
  delay(160);
  power.shutdown();
  delay(200);
  cancelPowerButtonHold();
}

void renderKeyboardToast(bool force)
{
  bool visible = keyboardToastVisible();
  if (!force && visible == lastRenderedKeyboardToastVisible && keyboardToastText == lastRenderedKeyboardToast) {
    return;
  }

  gfx->fillRect(kKeyboardToastRect.x, kKeyboardToastRect.y, kKeyboardToastRect.width, kKeyboardToastRect.height, kBackgroundColor);
  if (visible) {
    gfx->fillRoundRect(kKeyboardToastRect.x, kKeyboardToastRect.y, kKeyboardToastRect.width, kKeyboardToastRect.height, 12, kSurfaceColor);
    gfx->drawRoundRect(kKeyboardToastRect.x, kKeyboardToastRect.y, kKeyboardToastRect.width, kKeyboardToastRect.height, 12, kDividerColor);
    drawTextInRect(kKeyboardToastRect, keyboardToastText, &FreeSans9pt7b, TextAlign::Center, kForegroundColor, kSurfaceColor);
  }

  lastRenderedKeyboardToastVisible = visible;
  lastRenderedKeyboardToast = keyboardToastText;
}

void beginOtaScreen(const String &status, const String &footer, int percent)
{
  otaScreenActive = true;
  otaScreenDirty = true;
  otaScreenDismissAtMs = 0;
  otaStatusText = status;
  otaFooterText = footer;
  otaProgressPercent = constrain(percent, 0, 100);
  otaRenderedPercent = -1;
  lastRenderedOtaStatus = "";
  lastRenderedOtaFooter = "";
}

void updateOtaScreen(const String &status, const String &footer, int percent)
{
  int clampedPercent = constrain(percent, 0, 100);
  if (status != otaStatusText || footer != otaFooterText || clampedPercent != otaProgressPercent) {
    otaStatusText = status;
    otaFooterText = footer;
    otaProgressPercent = clampedPercent;
    otaScreenDirty = true;
  }
}

void endOtaScreen(uint32_t dismissDelayMs)
{
  otaInProgress = false;
  otaScreenDirty = true;
  otaScreenDismissAtMs = dismissDelayMs == 0 ? 0 : millis() + dismissDelayMs;
}

void renderOtaScreen(bool force)
{
  if (!otaScreenActive) {
    return;
  }

  if (!force && !otaScreenDirty) {
    return;
  }

  if (force) {
    gfx->fillScreen(kBackgroundColor);
    drawTextInRect(kOtaTitleRect, "Firmware Update", &FreeSansBold18pt7b, TextAlign::Center);
  }

  String percentText = String(otaProgressPercent) + "%";

  if (force || otaStatusText != lastRenderedOtaStatus) {
    drawTextInRect(kOtaStatusRect, otaStatusText, &FreeSans12pt7b, TextAlign::Center);
  }
  if (force || otaFooterText != lastRenderedOtaFooter) {
    drawTextInRect(kOtaFooterRect, otaFooterText, &FreeSans12pt7b, TextAlign::Center, kMutedTextColor, kBackgroundColor);
  }
  if (force || otaProgressPercent != otaRenderedPercent) {
    drawTextInRect(kOtaPercentRect, percentText, &FreeSansBold24pt7b, TextAlign::Center);
    drawOtaProgressBar(otaProgressPercent, force);
  }

  lastRenderedOtaStatus = otaStatusText;
  lastRenderedOtaFooter = otaFooterText;
  otaRenderedPercent = otaProgressPercent;
  otaScreenDirty = false;
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

  lastRenderedScreen = ScreenId::Count;
  hasRenderedWatchface = false;
  screenDirty = true;
  renderCurrentScreen(true);
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
  if (otaInProgress || inLightSleep || powerButtonHoldActive || powerButtonShutdownTriggered) {
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

uint32_t screenRenderInterval(ScreenId screen)
{
  if (powerButtonHoldActive || powerButtonShutdownTriggered) {
    return kPowerHoldRenderIntervalMs;
  }
  switch (screen) {
    case ScreenId::Motion:
      return kMotionRenderIntervalMs;
    case ScreenId::Weather:
      return kWeatherRenderIntervalMs;
    default:
      return kWatchfaceRenderIntervalMs;
  }
}

void navigateScreen(int direction)
{
  if (otaScreenActive) {
    return;
  }

  uint32_t now = millis();
  if (now - lastNavigationAtMs < kButtonDebounceMs) {
    return;
  }
  lastNavigationAtMs = now;

  int screenCount = static_cast<int>(ScreenId::Count);
  int nextScreen = (static_cast<int>(currentScreen) + direction) % screenCount;
  if (nextScreen < 0) {
    nextScreen += screenCount;
  }

  currentScreen = static_cast<ScreenId>(nextScreen);

  noteActivity();
  screenDirty = true;
  lastRenderedScreen = ScreenId::Count;
}

void updateTopButton()
{
  if (!expanderReady) {
    return;
  }

  uint32_t now = millis();
  bool pressed = expander.digitalRead(kTopButtonPin) == HIGH;

  if (pressed) {
    noteActivity();
    if (!sideButtonPressed) {
      sideButtonPressed = true;
      sideButtonReleaseArmed = true;
      sideButtonPressedAtMs = now;
    }
    return;
  }

  if (sideButtonPressed) {
    sideButtonPressed = false;
    if (sideButtonReleaseArmed && now - sideButtonPressedAtMs >= 30) {
      navigateScreen(1);
    }
    sideButtonReleaseArmed = false;
  }
}

void updatePowerKey()
{
  if (!expanderReady || !powerReady) {
    return;
  }

  if (expander.digitalRead(kPmuIrqPin) != HIGH) {
    return;
  }

  power.getIrqStatus();
  bool pressedEdge = power.isPekeyPositiveIrq();
  bool releasedEdge = power.isPekeyNegativeIrq();
  bool longPress = power.isPekeyLongPressIrq();

  if (pressedEdge) {
    startPowerButtonHold();
  }

  if (longPress) {
    triggerPowerButtonShutdown();
  }

  if (releasedEdge) {
    bool shouldToggle = powerButtonHoldActive &&
                        !powerButtonShutdownTriggered &&
                        (millis() - powerButtonHoldStartedAtMs) >= 30;
    cancelPowerButtonHold();
    if (shouldToggle) {
      navigateScreen(-1);
    }
  }

  power.clearIrqStatus();
}

void beginMotionScreen()
{
  motionViewMode = MotionViewMode::Dot;
  lastRenderedMotionViewMode = MotionViewMode::Count;
  hasRenderedMotionScreen = false;
  lastRenderedMotionUnavailable = false;
  lastRenderedMotionState.valid = false;

  if (motionState.valid) {
    motionPitchZero = motionState.pitch;
    motionRollZero = motionState.roll;
    resetMotionIndicatorFilter();
    captureMotionReferenceFrame();
  } else {
    motionReferenceReady = false;
  }
}

void handleMotionTouch()
{
  if (!touchTapPending) {
    return;
  }

  touchTapPending = false;
  if (currentScreen == ScreenId::Motion) {
    motionViewMode = static_cast<MotionViewMode>((static_cast<uint8_t>(motionViewMode) + 1) % static_cast<uint8_t>(MotionViewMode::Count));
    hasRenderedMotionScreen = false;
    lastRenderedMotionViewMode = MotionViewMode::Count;
    noteActivity();
    screenDirty = true;
  }
}

void renderWatchface(bool force)
{
  WatchfaceState currentState = buildWatchfaceState();

  if (force) {
    gfx->fillScreen(kBackgroundColor);
  }

  bool timeChanged = force || !hasRenderedWatchface || currentState.time != lastRenderedWatchface.time;

  if (timeChanged) {
    if (!force && hasRenderedWatchface) {
      Rect currentBounds = layoutTextInRect(kTimeRect, currentState.time, &Avenir_Next_Condensed60pt7b, TextAlign::Center, 1, 1).bounds;
      Rect previousBounds = layoutTextInRect(kTimeRect, lastRenderedWatchface.time, &Avenir_Next_Condensed60pt7b, TextAlign::Center, 1, 1).bounds;
      Rect dirtyRect = unionRect(currentBounds, previousBounds);
      if (rectHasArea(dirtyRect)) {
        gfx->fillRect(dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height, kBackgroundColor);
      }
    }

    drawTextInRect(kTimeRect,
                   currentState.time,
                   &Avenir_Next_Condensed60pt7b,
                   TextAlign::Center,
                   kForegroundColor,
                   kBackgroundColor,
                   1,
                   false);
  }
  if (force || !hasRenderedWatchface || timeChanged || currentState.date != lastRenderedWatchface.date) {
    if (!force && hasRenderedWatchface) {
      Rect currentBounds = layoutTextInRect(kDateRect, currentState.date, &FreeSans12pt7b, TextAlign::Center).bounds;
      Rect previousBounds = layoutTextInRect(kDateRect, lastRenderedWatchface.date, &FreeSans12pt7b, TextAlign::Center).bounds;
      Rect dirtyRect = unionRect(currentBounds, previousBounds);
      if (rectHasArea(dirtyRect)) {
        gfx->fillRect(dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height, kBackgroundColor);
      }
    }
    drawTextInRect(kDateRect, currentState.date, &FreeSans12pt7b, TextAlign::Center, kForegroundColor, kBackgroundColor, 1, false);
  }
  if (force || !hasRenderedWatchface || timeChanged || currentState.timezone != lastRenderedWatchface.timezone) {
    if (!force && hasRenderedWatchface) {
      Rect currentBounds = layoutTextInRect(kTimezoneRect, currentState.timezone, &FreeSans9pt7b, TextAlign::Center).bounds;
      Rect previousBounds = layoutTextInRect(kTimezoneRect, lastRenderedWatchface.timezone, &FreeSans9pt7b, TextAlign::Center).bounds;
      Rect dirtyRect = unionRect(currentBounds, previousBounds);
      if (rectHasArea(dirtyRect)) {
        gfx->fillRect(dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height, kBackgroundColor);
      }
    }
    drawTextInRect(kTimezoneRect, currentState.timezone, &FreeSans9pt7b, TextAlign::Center, kMutedTextColor, kBackgroundColor, 1, false);
  }
  if (force || !hasRenderedWatchface ||
      currentState.batteryPercent != lastRenderedWatchface.batteryPercent ||
      currentState.batteryAvailable != lastRenderedWatchface.batteryAvailable ||
      currentState.chargingActive != lastRenderedWatchface.chargingActive) {
    drawBatteryBar(kBatteryBarRect,
                   currentState.batteryPercent,
                   currentState.batteryAvailable,
                   batteryIndicatorColor(currentState.batteryPercent, currentState.chargingActive));
  }
  if (force || !hasRenderedWatchface ||
      currentState.wifiConnected != lastRenderedWatchface.wifiConnected ||
      currentState.bluetoothConnected != lastRenderedWatchface.bluetoothConnected ||
      currentState.bluetoothActivityActive != lastRenderedWatchface.bluetoothActivityActive ||
      currentState.usbConnected != lastRenderedWatchface.usbConnected ||
      currentState.chargingActive != lastRenderedWatchface.chargingActive) {
    drawStatusIcons(currentState);
  }

  renderKeyboardToast(force);

  lastRenderedWatchface = currentState;
  hasRenderedWatchface = true;
}

void renderMotionScreen(bool force)
{
  updateMotionDisplayState();

  if (force) {
    gfx->fillScreen(kBackgroundColor);
    hasRenderedMotionScreen = false;
    lastRenderedMotionUnavailable = false;
  }

  if (!motionState.valid) {
    if (force || !lastRenderedMotionUnavailable) {
      gfx->fillRect(kMotionFullRect.x, kMotionFullRect.y, kMotionFullRect.width, kMotionFullRect.height, kBackgroundColor);
      drawTextInRect(kMotionFullRect, "Sensor unavailable", &FreeSans12pt7b, TextAlign::Center, kMutedTextColor);
      lastRenderedMotionUnavailable = true;
      hasRenderedMotionScreen = false;
      lastRenderedMotionState.valid = false;
    }
    renderKeyboardToast(force);
    return;
  }

  char line[64];
  String orientationText = String(motionDisplayState.orientation);
  snprintf(line, sizeof(line), "Pitch  %5.0f deg", motionDisplayState.pitch);
  String pitchLine = String(line);
  snprintf(line, sizeof(line), "Roll   %5.0f deg", motionDisplayState.roll);
  String rollLine = String(line);
  snprintf(line, sizeof(line), "a x %4.1f  y %4.1f  z %4.1f", motionDisplayState.ax, motionDisplayState.ay, motionDisplayState.az);
  String accelLine = String(line);
  snprintf(line, sizeof(line), "g x %4.0f  y %4.0f  z %4.0f", motionDisplayState.gx, motionDisplayState.gy, motionDisplayState.gz);
  String gyroLine = String(line);
  MotionIndicatorPosition dot = motionIndicatorPosition(motionDisplayState.pitch, motionDisplayState.roll);

  if (lastRenderedMotionUnavailable && !force) {
    gfx->fillRect(kMotionFullRect.x, kMotionFullRect.y, kMotionFullRect.width, kMotionFullRect.height, kBackgroundColor);
    hasRenderedMotionScreen = false;
  }

  if (motionViewMode != lastRenderedMotionViewMode) {
    force = true;
  }

  switch (motionViewMode) {
    case MotionViewMode::Dot: {
      if (force || !hasRenderedMotionScreen) {
        gfx->fillRect(kMotionFullRect.x, kMotionFullRect.y, kMotionFullRect.width, kMotionFullRect.height, kBackgroundColor);
        drawMotionDotBackground();
        drawMotionDot(dot);
      } else if (abs(dot.x - lastRenderedMotionDot.x) >= 2 || abs(dot.y - lastRenderedMotionDot.y) >= 2) {
        gfx->fillCircle(lastRenderedMotionDot.x, lastRenderedMotionDot.y, 20, kBackgroundColor);
        drawMotionDotBackground();
        drawMotionDot(dot);
      }
      break;
    }

    case MotionViewMode::Cube: {
      bool needsRedraw = force || !hasRenderedMotionScreen || !lastRenderedMotionState.valid ||
                         fabsf(motionDisplayState.pitch - lastRenderedMotionState.pitch) >= 0.6f ||
                         fabsf(motionDisplayState.roll - lastRenderedMotionState.roll) >= 0.6f ||
                         fabsf(motionDisplayState.ax - lastRenderedMotionState.ax) >= 0.05f ||
                         fabsf(motionDisplayState.ay - lastRenderedMotionState.ay) >= 0.05f ||
                         fabsf(motionDisplayState.az - lastRenderedMotionState.az) >= 0.05f;
      if (needsRedraw) {
        drawMotionCube(motionDisplayState);
      }
      break;
    }

    case MotionViewMode::Raw: {
      if (force || !hasRenderedMotionScreen) {
        gfx->fillRect(kMotionFullRect.x, kMotionFullRect.y, kMotionFullRect.width, kMotionFullRect.height, kBackgroundColor);
      }
      if (force || !hasRenderedMotionScreen || orientationText != lastRenderedMotionOrientation) {
        drawChangingTextInRect(kMotionRawOrientationRect, orientationText, lastRenderedMotionOrientation, &FreeSansBold18pt7b, TextAlign::Center, kForegroundColor, kBackgroundColor, 1, force || !hasRenderedMotionScreen);
      }
      if (force || !hasRenderedMotionScreen || pitchLine != lastRenderedMotionPitchLine) {
        drawChangingTextInRect(kMotionRawPitchRect, pitchLine, lastRenderedMotionPitchLine, &FreeSans12pt7b, TextAlign::Center, kForegroundColor, kBackgroundColor, 1, force || !hasRenderedMotionScreen);
      }
      if (force || !hasRenderedMotionScreen || rollLine != lastRenderedMotionRollLine) {
        drawChangingTextInRect(kMotionRawRollRect, rollLine, lastRenderedMotionRollLine, &FreeSans12pt7b, TextAlign::Center, kForegroundColor, kBackgroundColor, 1, force || !hasRenderedMotionScreen);
      }
      if (force || !hasRenderedMotionScreen || accelLine != lastRenderedMotionAccelLine) {
        drawChangingTextInRect(kMotionRawAccelRect, accelLine, lastRenderedMotionAccelLine, &FreeSans9pt7b, TextAlign::Left, kMutedTextColor, kBackgroundColor, 1, force || !hasRenderedMotionScreen);
      }
      if (force || !hasRenderedMotionScreen || gyroLine != lastRenderedMotionGyroLine) {
        drawChangingTextInRect(kMotionRawGyroRect, gyroLine, lastRenderedMotionGyroLine, &FreeSans9pt7b, TextAlign::Left, kMutedTextColor, kBackgroundColor, 1, force || !hasRenderedMotionScreen);
      }
      break;
    }

    default:
      break;
  }
  renderKeyboardToast(force);

  lastRenderedMotionState = motionDisplayState;
  lastRenderedMotionOrientation = orientationText;
  lastRenderedMotionPitchLine = pitchLine;
  lastRenderedMotionRollLine = rollLine;
  lastRenderedMotionAccelLine = accelLine;
  lastRenderedMotionGyroLine = gyroLine;
  lastRenderedMotionDot = dot;
  lastRenderedMotionViewMode = motionViewMode;
  lastRenderedMotionUnavailable = false;
  hasRenderedMotionScreen = true;
}

void renderWeatherScreen(bool force)
{
  if (force) {
    gfx->fillScreen(kBackgroundColor);
    hasRenderedWeatherScreen = false;
  }

  drawWeatherHero(weatherState);

  String temperatureText = "--";
  String conditionText = networkIsOnline() ? "Refreshing weather" : "Weather offline";
  String primaryLine = networkIsOnline() ? "Fetching live forecast" : "Connect Wi-Fi to load forecast";
  String secondaryLine = weatherFetchInProgress ? "Pulling latest conditions" : "Generated animation stays active";
  String updatedLine = weatherState.updated.isEmpty() ? "Will retry automatically" : weatherState.updated;

  if (weatherState.hasData) {
    temperatureText = String(weatherState.temperatureF) + "\xB0";
    conditionText = weatherState.condition;
    primaryLine = "High " + String(weatherState.highF) +
                  "   Low " + String(weatherState.lowF) +
                  "   Feels " + String(weatherState.feelsLikeF);
    secondaryLine = "Wind " + String(weatherState.windMph) +
                    " mph   Rain " + String(weatherState.precipitationPercent) +
                    "%   " + (weatherState.isDay ? "Sunset " : "Sunrise ") +
                    (weatherState.isDay ? weatherState.sunset : weatherState.sunrise);
    updatedLine = weatherState.stale ? "Offline - cached forecast" : weatherState.updated;
  }

  if (force || !hasRenderedWeatherScreen || temperatureText != lastRenderedWeatherTemp) {
    drawTextInRect(kWeatherTempRect, temperatureText, &FreeSansBold24pt7b, TextAlign::Left);
  }
  if (force || !hasRenderedWeatherScreen || conditionText != lastRenderedWeatherCondition) {
    drawTextInRect(kWeatherConditionRect, conditionText, &FreeSans12pt7b, TextAlign::Left, kForegroundColor);
  }
  if (force || !hasRenderedWeatherScreen || primaryLine != lastRenderedWeatherPrimary) {
    drawTextInRect(kWeatherDetailPrimaryRect, primaryLine, &FreeSans9pt7b, TextAlign::Left, kMutedTextColor);
  }
  if (force || !hasRenderedWeatherScreen || secondaryLine != lastRenderedWeatherSecondary) {
    drawTextInRect(kWeatherDetailSecondaryRect, secondaryLine, &FreeSans9pt7b, TextAlign::Left, kMutedTextColor);
  }
  if (force || !hasRenderedWeatherScreen || updatedLine != lastRenderedWeatherUpdated) {
    drawTextInRect(kWeatherUpdatedRect, updatedLine, &FreeSans9pt7b, TextAlign::Left, kMutedTextColor);
  }

  renderKeyboardToast(force);

  lastRenderedWeatherLocation = weatherState.location;
  lastRenderedWeatherTemp = temperatureText;
  lastRenderedWeatherCondition = conditionText;
  lastRenderedWeatherPrimary = primaryLine;
  lastRenderedWeatherSecondary = secondaryLine;
  lastRenderedWeatherUpdated = updatedLine;
  lastRenderedWeatherHasData = weatherState.hasData;
  lastRenderedWeatherStale = weatherState.stale;
  lastRenderedWeatherIsDay = weatherState.isDay;
  lastRenderedWeatherCode = weatherState.weatherCode;
  hasRenderedWeatherScreen = true;
}

void renderCurrentScreen(bool force)
{
  uint32_t now = millis();
  if (!force && now - lastRenderAtMs < screenRenderInterval(currentScreen)) {
    return;
  }

  if (powerButtonHoldActive || powerButtonShutdownTriggered) {
    lastRenderAtMs = now;
    renderPowerHoldScreen(force || lastRenderedPowerHoldTenths < 0);
    screenDirty = false;
    return;
  }

  bool screenChanged = currentScreen != lastRenderedScreen;
  if (screenChanged) {
    if (currentScreen == ScreenId::Motion) {
      beginMotionScreen();
    }
    force = true;
  }

  lastRenderAtMs = now;

  switch (currentScreen) {
    case ScreenId::Watchface:
      renderWatchface(force);
      break;
    case ScreenId::Motion:
      renderMotionScreen(force);
      break;
    case ScreenId::Weather:
      renderWeatherScreen(force);
      break;
    default:
      break;
  }

  lastRenderedScreen = currentScreen;
  screenDirty = false;
}
} // namespace

void setup()
{
  Serial.begin(115200);
  delay(250);

  applyTimeZone();

  if (!gfx->begin()) {
    Serial.println("Display initialization failed");
    while (true) {
      delay(1000);
    }
  }

  gfx->setBrightness(kActiveBrightness);
  gfx->fillScreen(kBackgroundColor);

  Wire.begin(IIC_SDA, IIC_SCL);
  pinMode(TP_INT, INPUT_PULLUP);
  pinMode(IMU_IRQ, INPUT_PULLUP);

  initExpander();
  initPower();
  initRtc();
  initTouch();
  initImu();
  initKeyboardBle();

  configuredNetworkCount = countConfiguredNetworks();
  weatherState.condition = "Waiting for weather";
  weatherState.updated = "Waiting for Wi-Fi";
  nextWeatherRefreshAtMs = millis() + 1500;
  lastActivityAtMs = millis();
  renderCurrentScreen(true);
  beginNextWifiCycle();
}

void loop()
{
  updateWiFi();
  updateWeather();
  updateKeyboard();
  persistTimeToRtc();
  updateImuState();
  updateTouchInput();
  handleMotionTouch();
  updateTopButton();
  updatePowerKey();
  updatePowerButtonHold();
  updateIdleState();

  if (networkIsOnline()) {
    ArduinoOTA.handle();
  }

  if (otaScreenActive) {
    renderOtaScreen();

    if (!otaInProgress && otaScreenDismissAtMs != 0 &&
        static_cast<int32_t>(millis() - otaScreenDismissAtMs) >= 0) {
      otaScreenActive = false;
      otaScreenDismissAtMs = 0;
      lastRenderedScreen = ScreenId::Count;
      hasRenderedWatchface = false;
      renderCurrentScreen(true);
    }
  } else if (screenDirty || millis() - lastRenderAtMs >= screenRenderInterval(currentScreen)) {
    renderCurrentScreen();
  }

  delay(10);
}
