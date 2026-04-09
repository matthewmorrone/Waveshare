#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <time.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>
#include <esp_sntp.h>
#include "config/ota_config.h"

extern const lv_font_t montserrat_bold_128;

// Pin config
#define LCD_CS 12
#define LCD_SCLK 11
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7

// LVGL
static const uint32_t screenWidth = 368;
static const uint32_t screenHeight = 448;

// Display
Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_SH8601 *gfx = new Arduino_SH8601(bus, GFX_NOT_DEFINED, 0, screenWidth, screenHeight);
static const uint32_t lvglBufferRows = 40;
static lv_display_t *display;
static uint8_t *lvBuffer = nullptr;

// UI Elements
static lv_obj_t *timeLabel, *dateLabel, *timezoneLabel;
static lv_obj_t *wifiIcon, *btIcon, *batteryTrack, *batteryFill;

// Screen constants (from WaveformScreens)
static const int kBatteryTrackWidth = 266;
static const int kBatteryTrackHeight = 8;
static const int kBatteryTrackY = 358;
static const int kWatchTimeY = 108;
static const int kWatchDateY = 292;
static const int kWatchTimezoneY = 330;

void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap) {
  uint16_t width = static_cast<uint16_t>(area->x2 - area->x1 + 1);
  uint16_t height = static_cast<uint16_t>(area->y2 - area->y1 + 1);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(pxMap), width, height);
  lv_display_flush_ready(disp);
}

void setupDisplay() {
  gfx->begin();
  gfx->fillScreen(0x0000);

  lv_init();
  lv_tick_set_cb(millis);

  display = lv_display_create(screenWidth, screenHeight);

  size_t bufferPixelCount = screenWidth * lvglBufferRows;
  lvBuffer = static_cast<uint8_t *>(heap_caps_malloc(bufferPixelCount * sizeof(lv_color16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (!lvBuffer) {
    Serial.println("LVGL buffer allocation failed");
    while (true) delay(1000);
  }

  lv_display_set_buffers(display, lvBuffer, nullptr, bufferPixelCount * sizeof(lv_color16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, disp_flush);

  // Build watch screen
  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

  // Icon row (top right)
  lv_obj_t *iconRow = lv_obj_create(screen);
  lv_obj_set_size(iconRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(iconRow, 0, 0);
  lv_obj_set_style_border_opa(iconRow, 0, 0);
  lv_obj_set_layout(iconRow, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(iconRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(iconRow, 14, 0);
  lv_obj_align(iconRow, LV_ALIGN_TOP_RIGHT, -20, 18);

  btIcon = lv_label_create(iconRow);
  lv_obj_set_style_text_font(btIcon, &lv_font_montserrat_20, 0);
  lv_label_set_text(btIcon, LV_SYMBOL_BLUETOOTH);
  lv_obj_set_style_text_color(btIcon, lv_color_white(), 0);

  wifiIcon = lv_label_create(iconRow);
  lv_obj_set_style_text_font(wifiIcon, &lv_font_montserrat_20, 0);
  lv_label_set_text(wifiIcon, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_color(wifiIcon, lv_color_white(), 0);

  // Battery track
  batteryTrack = lv_obj_create(screen);
  lv_obj_set_size(batteryTrack, kBatteryTrackWidth, kBatteryTrackHeight);
  lv_obj_align(batteryTrack, LV_ALIGN_TOP_MID, 0, kBatteryTrackY);
  lv_obj_set_style_radius(batteryTrack, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(batteryTrack, lv_color_hex(0x1E1E24), 0);
  lv_obj_set_style_border_width(batteryTrack, 1, 0);
  lv_obj_set_style_border_color(batteryTrack, lv_color_hex(0x2E3640), 0);
  lv_obj_set_style_pad_all(batteryTrack, 0, 0);

  batteryFill = lv_obj_create(batteryTrack);
  lv_obj_set_size(batteryFill, kBatteryTrackWidth, kBatteryTrackHeight);
  lv_obj_set_style_radius(batteryFill, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(batteryFill, 0, 0);
  lv_obj_set_style_pad_all(batteryFill, 0, 0);
  lv_obj_set_style_bg_color(batteryFill, lv_color_hex(0x00FF00), 0);
  lv_obj_align(batteryFill, LV_ALIGN_LEFT_MID, 0, 0);

  // Time label (using large custom font)
  timeLabel = lv_label_create(screen);
  lv_obj_set_width(timeLabel, LV_SIZE_CONTENT);
  lv_label_set_long_mode(timeLabel, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(timeLabel, &montserrat_bold_128, 0);
  lv_obj_set_style_text_color(timeLabel, lv_color_white(), 0);
  lv_label_set_text(timeLabel, "--:--");
  lv_obj_align(timeLabel, LV_ALIGN_TOP_MID, 0, kWatchTimeY);

  // Date label
  dateLabel = lv_label_create(screen);
  lv_obj_set_width(dateLabel, screenWidth - 36);
  lv_obj_set_style_text_font(dateLabel, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(dateLabel, lv_color_white(), 0);
  lv_obj_set_style_text_align(dateLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(dateLabel, "Waiting");
  lv_obj_align(dateLabel, LV_ALIGN_TOP_MID, 0, kWatchDateY);

  // Timezone label
  timezoneLabel = lv_label_create(screen);
  lv_obj_set_width(timezoneLabel, screenWidth - 36);
  lv_obj_set_style_text_font(timezoneLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(timezoneLabel, lv_color_hex(0x949CA8), 0);
  lv_obj_set_style_text_align(timezoneLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(timezoneLabel, "TIME UNAVAILABLE");
  lv_obj_align(timezoneLabel, LV_ALIGN_TOP_MID, 0, kWatchTimezoneY);

  Serial.println("Watch screen built");
}

void updateDisplay() {
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);

  static uint32_t lastLog = 0;
  if (millis() - lastLog >= 10000) {
    lastLog = millis();
    Serial.printf("Time: epoch=%lld, year=%d, mon=%d, mday=%d, hour=%d, min=%d, sec=%d\n",
                  (long long)now, timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                  timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    Serial.printf("TZ env: %s\n", getenv("TZ") ? getenv("TZ") : "NOT SET");
  }

  // Update time
  char timeBuf[16];
  strftime(timeBuf, sizeof(timeBuf), "%H:%M", timeinfo);
  lv_label_set_text(timeLabel, timeBuf);

  // Update date
  char dateBuf[64];
  strftime(dateBuf, sizeof(dateBuf), "%a, %b %d", timeinfo);
  lv_label_set_text(dateLabel, dateBuf);

  // Update timezone
  char tzBuf[32];
  strftime(tzBuf, sizeof(tzBuf), "%Z", timeinfo);
  lv_label_set_text(timezoneLabel, tzBuf);

  // Update WiFi icon
  lv_obj_set_style_text_color(wifiIcon, WiFi.isConnected() ? lv_color_white() : lv_color_hex(0x485260), 0);
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);

  // Scan to prove WiFi radio works
  Serial.println("Scanning for networks...");
  int networks = WiFi.scanNetworks();
  Serial.printf("Found %d networks:\n", networks);
  for (int i = 0; i < networks && i < 10; i++) {
    Serial.printf("  [%d] %s (RSSI: %ld)\n", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  }
  Serial.println();

  const char *ssids[] = {WIFI_SSID_PRIMARY, WIFI_SSID_SECONDARY, HOTSPOT_SSID};
  const char *passes[] = {WIFI_PASSWORD_PRIMARY, WIFI_PASSWORD_SECONDARY, HOTSPOT_PASSWORD};

  for (int i = 0; i < 3; i++) {
    if (strlen(ssids[i]) == 0) {
      Serial.printf("Skipping empty SSID at index %d\n", i);
      continue;
    }
    Serial.printf("[%d] Attempting %s...\n", i, ssids[i]);
    WiFi.begin(ssids[i], passes[i]);

    uint32_t timeout = millis() + 15000;
    while (WiFi.status() != WL_CONNECTED && millis() < timeout) {
      delay(500);
      Serial.printf("  Status: %d\n", WiFi.status());
    }

    if (WiFi.isConnected()) {
      Serial.printf("✓ Connected to %s! IP: %s\n", ssids[i], WiFi.localIP().toString().c_str());

      // Set DNS servers
      WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), IPAddress(8, 8, 8, 8), IPAddress(8, 8, 4, 4));
      Serial.println("DNS configured to 8.8.8.8, 8.8.4.4");

      // Test API call
      Serial.println("Testing HTTP GET to worldtimeapi.org...");
      HTTPClient http;
      http.begin("http://worldtimeapi.org/api/timezone/America/New_York");
      int httpCode = http.GET();
      Serial.printf("HTTP Response code: %d\n", httpCode);

      if (httpCode == 200) {
        String payload = http.getString();
        Serial.println("API Response (first 200 chars):");
        Serial.println(payload.substring(0, 200));

        // Extract unix_timestamp from JSON
        int pos = payload.indexOf("\"unixtime\":");
        if (pos != -1) {
          int endPos = payload.indexOf(",", pos);
          String timeStr = payload.substring(pos + 11, endPos);
          time_t apiTime = timeStr.toInt();
          Serial.printf("API unix timestamp: %lld\n", (long long)apiTime);

          // Set system time from API
          timeval tv = {apiTime, 0};
          settimeofday(&tv, nullptr);
          Serial.println("✓ System time set from API");
        }
      }
      http.end();

      Serial.printf("Calling configTzTime with TZ=%s\n", TIMEZONE_POSIX);
      configTzTime(TIMEZONE_POSIX, NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);
      Serial.println("✓ configTzTime completed");

      delay(1000);
      time_t now = time(nullptr);
      Serial.printf("Time after sync: epoch=%lld\n", (long long)now);
      return;
    } else {
      Serial.printf("✗ Failed to connect to %s (final status: %d)\n", ssids[i], WiFi.status());
    }
  }
  Serial.println("WiFi connection failed - all networks exhausted");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\nMinimal Waveform Watch");

  setupDisplay();
  setupWiFi();

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.begin();
}

void loop() {
  if (WiFi.isConnected()) {
    ArduinoOTA.handle();
  }

  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    updateDisplay();
    lv_timer_handler();
  }

  delay(50);
}
