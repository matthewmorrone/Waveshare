#include <Adafruit_XCA9554.h>
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <lvgl.h>
#include <sys/time.h>
#include <time.h>

#include "ota_config.h"
#include "pin_config.h"
#include "screen_manager.h"
#include "screen_callbacks.h"
#include "time_utils.h"
void updatePlanets();

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

Adafruit_XCA9554 expander;

lv_display_t *display = nullptr;
lv_indev_t *touchInput = nullptr;
lv_obj_t *screenRoots[24] = {};
int currentScreenIndex = static_cast<int>(ScreenId::Planets);

namespace
{
constexpr uint8_t kActiveBrightness = 255;
constexpr uint16_t kBackgroundColor = 0x0000;
constexpr uint32_t kLvglBufferRows = 40;
constexpr uint32_t kWifiConnectTimeoutMs = 12000;
constexpr uint32_t kWifiRetryIntervalMs = 30000;
constexpr uint32_t kMemoryReportIntervalMs = 10000;
uint8_t *lvBuffer = nullptr;
bool expanderReady = false;
bool wifiAttemptInProgress = false;
bool wifiWasOnline = false;
size_t currentCredentialIndex = 0;
size_t nextCredentialIndex = 0;
uint32_t wifiStartedAtMs = 0;
uint32_t nextWifiRetryAtMs = 0;
uint32_t lastRefreshAtMs = 0;
uint32_t lastMemoryReportAtMs = 0;

struct WiFiCredential
{
  const char *ssid;
  const char *password;
};

constexpr WiFiCredential kWiFiCredentials[] = {
    {WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY},
    {WIFI_SSID_SECONDARY, WIFI_PASSWORD_SECONDARY},
    {HOTSPOT_SSID, HOTSPOT_PASSWORD},
};

void flushDisplay(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap)
{
  LV_UNUSED(disp);
  uint16_t width = static_cast<uint16_t>(area->x2 - area->x1 + 1);
  uint16_t height = static_cast<uint16_t>(area->y2 - area->y1 + 1);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(pxMap), width, height);
  lv_display_flush_ready(display);
}
} // namespace

lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b)
{
  return lv_color_make(r, g, b);
}

void applyRootStyle(lv_obj_t *obj)
{
  lv_obj_set_size(obj, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_style_bg_color(obj, lvColor(0, 0, 0), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_outline_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

bool otaUpdateInProgress()
{
  return false;
}

bool lightSleepActive()
{
  return false;
}

bool hasValidTime()
{
  return waveform::effectiveNow() >= waveform::minimumReasonableEpoch();
}

String weatherUpdatedLabel()
{
  return String("Updated ") + waveform::formatCurrentLocal("%H:%M", "--:--");
}

float compressedOrbitRadius(float distanceAu)
{
  return sqrtf(fmaxf(distanceAu, 0.0f));
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

void setupLvgl()
{
  lv_init();
  lv_tick_set_cb(millis);

  display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
  size_t bufferPixelCount = LCD_WIDTH * kLvglBufferRows;
  lvBuffer = static_cast<uint8_t *>(malloc(bufferPixelCount * sizeof(lv_color16_t)));
  if (!lvBuffer) {
    Serial.println("LVGL buffer allocation failed");
    while (true) {
      delay(1000);
    }
  }

  lv_display_set_buffers(display, lvBuffer, nullptr, bufferPixelCount * sizeof(lv_color16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flushDisplay);
}

void seedTimeFromBuildIfNeeded()
{
  if (hasValidTime()) {
    return;
  }

  time_t buildEpoch = waveform::buildTimestampEpoch();
  if (buildEpoch <= 0) {
    return;
  }

  if (waveform::ensureSystemTimeAtLeastBuildTimestamp()) {
    Serial.printf("Seeded time from build timestamp: %ld\n", static_cast<long>(buildEpoch));
  }
}

void startWifi()
{
  for (size_t index = 0; index < (sizeof(kWiFiCredentials) / sizeof(kWiFiCredentials[0])); ++index) {
    if (!kWiFiCredentials[index].ssid || kWiFiCredentials[index].ssid[0] == '\0') {
      continue;
    }

    currentCredentialIndex = index;
    nextCredentialIndex = index + 1;
    wifiAttemptInProgress = true;
    wifiStartedAtMs = millis();

    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    if (WiFi.getMode() != WIFI_OFF) {
      WiFi.disconnect(true, true);
    }
    delay(40);
    WiFi.mode(WIFI_OFF);
    delay(40);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setHostname(OTA_HOSTNAME);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(1, 1, 1, 1), IPAddress(8, 8, 8, 8));
    Serial.printf("Connecting to Wi-Fi SSID \"%s\"...\n", kWiFiCredentials[index].ssid);
    WiFi.begin(kWiFiCredentials[index].ssid, kWiFiCredentials[index].password);
    configTzTime(TIMEZONE_POSIX, NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);
    return;
  }
}

void updateWifi()
{
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiWasOnline) {
      wifiWasOnline = true;
      Serial.printf("Wi-Fi connected to \"%s\": %s\n",
                    WiFi.SSID().c_str(),
                    WiFi.localIP().toString().c_str());
    }
    wifiAttemptInProgress = false;
    return;
  }

  if (wifiWasOnline) {
    wifiWasOnline = false;
    Serial.println("Wi-Fi connection lost");
  }

  if (!wifiAttemptInProgress) {
    if (static_cast<int32_t>(millis() - nextWifiRetryAtMs) < 0) {
      return;
    }
    startWifi();
    return;
  }

  if (millis() - wifiStartedAtMs >= kWifiConnectTimeoutMs) {
    while (nextCredentialIndex < (sizeof(kWiFiCredentials) / sizeof(kWiFiCredentials[0]))) {
      if (!kWiFiCredentials[nextCredentialIndex].ssid || kWiFiCredentials[nextCredentialIndex].ssid[0] == '\0') {
        ++nextCredentialIndex;
        continue;
      }

      currentCredentialIndex = nextCredentialIndex++;
      wifiStartedAtMs = millis();

      if (WiFi.getMode() != WIFI_OFF) {
        WiFi.disconnect(true, true);
      }
      delay(40);
      WiFi.mode(WIFI_OFF);
      delay(40);
      WiFi.mode(WIFI_STA);
      WiFi.setSleep(false);
      WiFi.setHostname(OTA_HOSTNAME);
      WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(1, 1, 1, 1), IPAddress(8, 8, 8, 8));
      Serial.printf("Connecting to Wi-Fi SSID \"%s\"...\n", kWiFiCredentials[currentCredentialIndex].ssid);
      WiFi.begin(kWiFiCredentials[currentCredentialIndex].ssid, kWiFiCredentials[currentCredentialIndex].password);
      return;
    }

    WiFi.disconnect(true, true);
    wifiAttemptInProgress = false;
    nextWifiRetryAtMs = millis() + kWifiRetryIntervalMs;
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  uint32_t serialWaitStartedAt = millis();
  while (!Serial && millis() - serialWaitStartedAt < 2000) {
    delay(10);
  }
  delay(250);
  Serial.printf("Planets boot. reset_reason=%d\n", static_cast<int>(esp_reset_reason()));

  Wire.begin(IIC_SDA, IIC_SCL);
  initExpander();

  if (!gfx->begin()) {
    Serial.println("Display initialization failed");
    while (true) {
      delay(1000);
    }
  }

  gfx->setBrightness(kActiveBrightness);
  gfx->fillScreen(kBackgroundColor);
  gfx->displayOn();

  setupLvgl();
  startWifi();
  seedTimeFromBuildIfNeeded();
  Serial.printf("Planets time valid after seed: %s epoch=%ld\n",
                hasValidTime() ? "yes" : "no",
                static_cast<long>(time(nullptr)));

  if (!waveformBuildPlanetsScreen()) {
    Serial.println("Planets screen build failed");
    while (true) {
      delay(1000);
    }
  }

  lv_screen_load(screenRoots[static_cast<size_t>(ScreenId::Planets)]);
  waveformEnterPlanetsScreen();
  updatePlanets();
  waveformRefreshPlanetsScreen();
  lv_refr_now(display);
  lastRefreshAtMs = millis();
}

void loop()
{
  updateWifi();
  updatePlanets();

  uint32_t now = millis();
  if (now - lastMemoryReportAtMs >= kMemoryReportIntervalMs) {
    lastMemoryReportAtMs = now;
    Serial.printf("Memory: free_heap=%u min_free_heap=%u largest_block=%u\n",
                  ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(),
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  }

  if (now - lastRefreshAtMs >= 250) {
    waveformRefreshPlanetsScreen();
    lastRefreshAtMs = now;
  }

  waveformTickPlanetsScreen(now);
  lv_timer_handler();
  delay(5);
}
