#include <Adafruit_XCA9554.h>
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <lvgl.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>

#include "ota_config.h"
#include "pin_config.h"
#include "screen_manager.h"
#include "screen_callbacks.h"
#include "geo_state.h"

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
GeoState geoState;

namespace
{
constexpr uint8_t kActiveBrightness = 255;
constexpr uint16_t kBackgroundColor = 0x0000;
constexpr uint32_t kLvglBufferRows = 40;
constexpr uint32_t kWifiConnectTimeoutMs = 12000;
constexpr uint32_t kWifiRetryIntervalMs = 30000;
constexpr uint32_t kMemoryReportIntervalMs = 10000;
constexpr time_t kMinValidEpoch = 1704067200;

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
  return time(nullptr) >= kMinValidEpoch;
}

String weatherUpdatedLabel()
{
  time_t now = time(nullptr);
  if (now <= 0) {
    return "Updated";
  }

  struct tm localTime = {};
  localtime_r(&now, &localTime);
  char buffer[32];
  strftime(buffer, sizeof(buffer), "Updated %H:%M", &localTime);
  return String(buffer);
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

int monthFromBuildString(const char *month)
{
  if (strcmp(month, "Jan") == 0) return 0;
  if (strcmp(month, "Feb") == 0) return 1;
  if (strcmp(month, "Mar") == 0) return 2;
  if (strcmp(month, "Apr") == 0) return 3;
  if (strcmp(month, "May") == 0) return 4;
  if (strcmp(month, "Jun") == 0) return 5;
  if (strcmp(month, "Jul") == 0) return 6;
  if (strcmp(month, "Aug") == 0) return 7;
  if (strcmp(month, "Sep") == 0) return 8;
  if (strcmp(month, "Oct") == 0) return 9;
  if (strcmp(month, "Nov") == 0) return 10;
  if (strcmp(month, "Dec") == 0) return 11;
  return 0;
}

void seedTimeFromBuildIfNeeded()
{
  if (hasValidTime()) {
    return;
  }

  char month[4] = {};
  int day = 1;
  int year = 2026;
  int hour = 0;
  int minute = 0;
  int second = 0;

  if (sscanf(__DATE__, "%3s %d %d", month, &day, &year) != 3) {
    return;
  }
  if (sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second) != 3) {
    return;
  }

  struct tm buildTm = {};
  buildTm.tm_year = year - 1900;
  buildTm.tm_mon = monthFromBuildString(month);
  buildTm.tm_mday = day;
  buildTm.tm_hour = hour;
  buildTm.tm_min = minute;
  buildTm.tm_sec = second;
  buildTm.tm_isdst = -1;

  time_t buildEpoch = mktime(&buildTm);
  if (buildEpoch <= 0) {
    return;
  }

  timeval tv = {};
  tv.tv_sec = buildEpoch;
  settimeofday(&tv, nullptr);
  Serial.printf("Seeded time from build timestamp: %ld\n", static_cast<long>(buildEpoch));
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
  Serial.printf("Stars boot. reset_reason=%d\n", static_cast<int>(esp_reset_reason()));

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

  if (!waveformBuildStarsScreen()) {
    Serial.println("Stars screen build failed");
    while (true) {
      delay(1000);
    }
  }

  lv_screen_load(screenRoots[static_cast<size_t>(ScreenId::Stars)]);
  waveformEnterStarsScreen();
  waveformRefreshStarsScreen();
  lv_refr_now(display);
  lastRefreshAtMs = millis();
}

void loop()
{
  updateWifi();

  uint32_t now = millis();
  if (now - lastMemoryReportAtMs >= kMemoryReportIntervalMs) {
    lastMemoryReportAtMs = now;
    Serial.printf("Memory: free_heap=%u min_free_heap=%u largest_block=%u\n",
                  ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(),
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  }

  if (now - lastRefreshAtMs >= 250) {
    waveformRefreshStarsScreen();
    lastRefreshAtMs = now;
  }

  waveformTickStarsScreen(now);
  lv_timer_handler();
  delay(5);
}
