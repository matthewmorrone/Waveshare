#include "standalone_runtime.h"

#include <Arduino.h>
#include <Arduino_DriveBus_Library.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <memory>
#include <sys/time.h>
#include <time.h>

#include "ota_config.h"
#include "ota_module.h"
#include "pin_config.h"
#include "screen_constants.h"
#include "settings_state.h"
#include "storage.h"
#include "wifi_manager.h"
#include "ble_manager.h"

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

XPowersAXP2101 power;
Adafruit_XCA9554 expander;
SensorPCF85063 rtc;
std::shared_ptr<Arduino_IIC_DriveBus> touchBus = std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);
std::unique_ptr<Arduino_IIC> touchController(
    new Arduino_FT3x68(touchBus, FT3168_DEVICE_ADDRESS, DRIVEBUS_DEFAULT_VALUE, TP_INT, nullptr));
Preferences preferences;

lv_display_t *display = nullptr;
lv_indev_t *touchInput = nullptr;
lv_obj_t *screenRoots[24] = {};

bool rtcReady = false;
bool expanderReady = false;
bool powerReady = false;
bool sdMounted = false;

size_t currentScreenIndex = 0;
ConnectivityState connectivityState = ConnectivityState::Offline;
uint32_t nextWifiRetryAtMs = 0;
uint32_t lastAutoCycleAtMs = 0;
bool autoCycleEnabled = false;

namespace
{
constexpr uint8_t kActiveBrightness = 255;
constexpr uint16_t kBackgroundColor = 0x0000;
constexpr uint32_t kLvglBufferRows = 40;
constexpr time_t kMinValidEpoch = 1704067200;
constexpr uint32_t kTimeApiFallbackDelayMs = 3000;
constexpr const char *kTimeApiFallbackUrl = "http://worldtimeapi.org/api/timezone/America/New_York";
constexpr const char *kPreferencesNamespace = "waveform";

uint8_t *lvBuffer = nullptr;
bool touchReady = false;
bool touchPressed = false;
bool ntpConfigured = false;
bool timeApiFallbackAttempted = false;
int16_t touchLastX = LCD_WIDTH / 2;
int16_t touchLastY = LCD_HEIGHT / 2;
uint32_t wifiConnectedAtMs = 0;
uint32_t lastRefreshAtMs = 0;
uint32_t lastTickAtMs = 0;
char configuredTimezonePosix[16] = {};
int pendingTimezoneOffset = 0;
StandaloneRuntimeConfig gConfig{};

void handleTouchInterrupt()
{
  if (touchController) {
    touchController->IIC_Interrupt_Flag = true;
  }
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
  timeval tv = {.tv_sec = epoch, .tv_usec = 0};
  return settimeofday(&tv, nullptr) == 0;
}

bool setSystemTimeFromEpoch(time_t epoch)
{
  if (epoch < kMinValidEpoch) {
    return false;
  }
  timeval tv = {.tv_sec = epoch, .tv_usec = 0};
  return settimeofday(&tv, nullptr) == 0;
}

bool hasValidTime()
{
  return time(nullptr) >= kMinValidEpoch;
}

bool syncTimeViaApiFallback()
{
  HTTPClient http;
  if (!http.begin(kTimeApiFallbackUrl)) {
    return false;
  }

  bool success = false;
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    int pos = payload.indexOf("\"unixtime\":");
    if (pos != -1) {
      int endPos = payload.indexOf(",", pos);
      String timeStr = payload.substring(pos + 11, endPos);
      success = setSystemTimeFromEpoch(static_cast<time_t>(timeStr.toInt()));
    }
  }
  http.end();
  return success;
}

void loadTimeFromRtc()
{
  if (!rtcReady) {
    return;
  }

  struct tm rtcTime = {};
  rtc.getDateTime(&rtcTime);
  if (!rtcTimeLooksValid(rtcTime)) {
    return;
  }

  setSystemTimeFromTm(rtcTime);
}

void handleWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
  LV_UNUSED(info);
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      connectivityState = ConnectivityState::Online;
      wifiConnectedAtMs = millis();
      timeApiFallbackAttempted = false;
      if (gConfig.enableOta) {
        otaModuleStart();
      }
      if (!ntpConfigured) {
        configTzTime(configuredTimezonePosix, NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);
        ntpConfigured = true;
      }
      wifiManagerRequestScan();
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      connectivityState = ConnectivityState::Offline;
      ntpConfigured = false;
      wifiConnectedAtMs = 0;
      timeApiFallbackAttempted = false;
      if (gConfig.enableOta) {
        otaModuleReset();
      }
      break;
    default:
      break;
  }
}

void updateWiFi()
{
  if (!gConfig.enableWifi || !settingsState().wifiEnabled) {
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true, true);
      WiFi.mode(WIFI_OFF);
    }
    connectivityState = ConnectivityState::Offline;
    ntpConfigured = false;
    return;
  }

  if (networkIsOnline()) {
    connectivityState = ConnectivityState::Online;
    if (!ntpConfigured) {
      configTzTime(configuredTimezonePosix, NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);
      ntpConfigured = true;
    }
    if (!hasValidTime() &&
        !timeApiFallbackAttempted &&
        wifiConnectedAtMs != 0 &&
        (millis() - wifiConnectedAtMs) >= kTimeApiFallbackDelayMs) {
      timeApiFallbackAttempted = true;
      syncTimeViaApiFallback();
    }
    return;
  }

  if (WiFi.getMode() == WIFI_OFF) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setHostname(OTA_HOSTNAME);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(1, 1, 1, 1), IPAddress(8, 8, 8, 8));
    WiFi.begin(WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY);
    connectivityState = ConnectivityState::Connecting;
    nextWifiRetryAtMs = millis() + 15000;
    return;
  }

  wl_status_t status = WiFi.status();
  if ((status == WL_IDLE_STATUS || status == WL_DISCONNECTED || status == WL_NO_SSID_AVAIL || status == WL_CONNECT_FAILED) &&
      static_cast<int32_t>(millis() - nextWifiRetryAtMs) >= 0) {
    WiFi.disconnect(true, true);
    delay(40);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(1, 1, 1, 1), IPAddress(8, 8, 8, 8));
    WiFi.begin(WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY);
    connectivityState = ConnectivityState::Connecting;
    nextWifiRetryAtMs = millis() + 15000;
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
    int fingers = static_cast<int>(touchController->IIC_Read_Device_Value(
        touchController->Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER));
    touchPressed = fingers > 0;
    if (touchPressed) {
      touchLastX = static_cast<int16_t>(touchController->IIC_Read_Device_Value(
          touchController->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X));
      touchLastY = static_cast<int16_t>(touchController->IIC_Read_Device_Value(
          touchController->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y));
    }
    touchController->IIC_Interrupt_Flag = false;
  }

  data->point.x = touchLastX;
  data->point.y = touchLastY;
  data->state = touchPressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
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
} // namespace

void noteActivity()
{
}

int batteryPercentValue()
{
  if (!powerReady || !power.isBatteryConnect()) {
    return -1;
  }
  int percent = power.getBatteryPercent();
  return percent < 0 ? -1 : constrain(percent, 0, 100);
}

bool batteryIsAvailable()
{
  return powerReady && power.isBatteryConnect();
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
  if (percent >= 100 || power.isCharging()) {
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

lv_color_t batteryIndicatorColor(int percent, bool charging)
{
  if (charging) {
    return lv_color_make(0, 120, 255);
  }
  if (percent < 0) {
    return lv_color_make(88, 88, 96);
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
  return lv_color_make(red, green, 0);
}

void setDisplayBrightness(uint8_t brightness)
{
  gfx->setBrightness(brightness ? brightness : kActiveBrightness);
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

void applyConfiguredTimezone()
{
  int offset = settingsState().utcOffsetHours;
  int hours = abs(offset);
  snprintf(configuredTimezonePosix,
           sizeof(configuredTimezonePosix),
           "UTC%c%d",
           offset <= 0 ? '+' : '-',
           hours);
  setenv("TZ", configuredTimezonePosix, 1);
  tzset();
}

void setOfflineMode(const char *reason)
{
  LV_UNUSED(reason);
  connectivityState = ConnectivityState::Offline;
  ntpConfigured = false;
  timeApiFallbackAttempted = false;
}

void timezonePickerSetInitial(int utcOffset)
{
  pendingTimezoneOffset = utcOffset;
}

void showTimezonePicker()
{
  int next = pendingTimezoneOffset + 1;
  if (next > 14) {
    next = -12;
  }
  pendingTimezoneOffset = next;
  settingsState().utcOffsetHours = next;
  settingsSave(preferences);
  applyConfiguredTimezone();
}

void hideTimezonePicker()
{
}

void timezonePickerHandleSwipe(int deltaX)
{
  LV_UNUSED(deltaX);
}

bool showScreenById(ScreenId id)
{
  if (!screenManagerEnsureBuilt(id)) {
    return false;
  }

  screenManagerLeave(static_cast<ScreenId>(currentScreenIndex));
  currentScreenIndex = static_cast<size_t>(id);
  lv_screen_load(screenManagerRoot(id));
  screenManagerEnter(id);
  screenManagerRefresh(id);
  return true;
}

void standaloneRuntimeSetup(const StandaloneRuntimeConfig &config)
{
  gConfig = config;

  Serial.begin(115200);
  delay(250);

  preferences.begin(kPreferencesNamespace, false);
  settingsLoad(preferences);
  autoCycleEnabled = settingsState().autoCycleEnabled;
  applyConfiguredTimezone();

  const char *const knownNetworks[] = {WIFI_SSID_PRIMARY};
  wifiManagerConfigureKnownNetworks(knownNetworks, 1);

  Wire.begin(IIC_SDA, IIC_SCL);
  pinMode(TP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TP_INT), handleTouchInterrupt, FALLING);

  initExpander();
  initPower();
  initTouch();
  initRtc();
  if (gConfig.enableSd) {
    initSdCard();
  }

  WiFi.onEvent(handleWiFiEvent);

  if (!gfx->begin()) {
    Serial.println("Display initialization failed");
    while (true) {
      delay(1000);
    }
  }

  setDisplayBrightness(settingsState().brightness);
  gfx->fillScreen(kBackgroundColor);
  gfx->displayOn();

  setupLvgl();
  if (gConfig.enableOta) {
    otaModuleBuildOverlay();
  }

  if (!screenManagerEnsureBuilt(gConfig.initialScreen)) {
    Serial.println("Initial screen build failed");
    while (true) {
      delay(1000);
    }
  }

  currentScreenIndex = static_cast<size_t>(gConfig.initialScreen);
  lv_screen_load(screenManagerRoot(gConfig.initialScreen));
  screenManagerEnter(gConfig.initialScreen);
  screenManagerRefresh(gConfig.initialScreen);
  lv_refr_now(display);

  lastRefreshAtMs = millis();
  lastTickAtMs = lastRefreshAtMs;

  if (gConfig.enableWifi && settingsState().wifiEnabled) {
    nextWifiRetryAtMs = millis();
    updateWiFi();
  }
}

void standaloneRuntimeLoop()
{
  if (gConfig.enableWifi) {
    updateWiFi();
    wifiManagerUpdateScan(millis(), settingsState().wifiEnabled, true);
  }
  if (gConfig.enableBle) {
    bleManagerUpdate(millis(), settingsState().bleEnabled, true);
  }
  if (gConfig.enableOta) {
    otaModuleHandle();
    otaModuleUpdate();
  }

  uint32_t now = millis();
  if (now - lastRefreshAtMs >= gConfig.refreshIntervalMs) {
    screenManagerRefresh(static_cast<ScreenId>(currentScreenIndex));
    lastRefreshAtMs = now;
  }
  if (now - lastTickAtMs >= gConfig.tickIntervalMs) {
    screenManagerTick(static_cast<ScreenId>(currentScreenIndex), now);
    lastTickAtMs = now;
  }

  lv_timer_handler();
  delay(5);
}
