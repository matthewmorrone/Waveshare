#include "config/screen_constants.h"
#include "core/screen_manager.h"
#include "modules/wifi_manager.h"
#include "screens/screen_callbacks.h"
#include <SensorPCF85063.hpp>
#include <WiFi.h>
#include <Wire.h>

extern lv_obj_t *screenRoots[];
extern ConnectivityState connectivityState;
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
lv_color_t batteryIndicatorColor(int percent, bool charging);
bool usbIsConnected();
bool batteryIsCharging();
int batteryPercentValue();
String firmwareUpdatedText();
String timeText();
String dateText();
String timezoneText();
void enableTapBubbling(lv_obj_t *obj);
// Symbols from main.cpp
extern bool rtcReady;
extern SensorPCF85063 rtc;
bool rtcTimeLooksValid(const struct tm &timeInfo);
bool setSystemTimeFromTm(const struct tm &timeInfo);

bool watchfaceBuilt = false;

extern lv_obj_t *watchTimeLabel;
extern lv_obj_t *watchTimeRow;
extern lv_obj_t *watchTimeGlyphs[5];
extern lv_obj_t *watchDateLabel;
extern lv_obj_t *watchTimezoneLabel;
extern lv_obj_t *watchFirmwareLabel;
extern lv_obj_t *watchBatteryTrack;
extern lv_obj_t *watchBatteryFill;
extern lv_obj_t *watchWifiIcon;
extern lv_obj_t *watchBluetoothIcon;
extern lv_obj_t *watchUsbIcon;
extern lv_obj_t *watchStatusLabel;

namespace
{
const ScreenModule kModule = {
    ScreenId::Watchface,
    "Watchface",
    waveformBuildWatchfaceScreen,
    waveformRefreshWatchfaceScreen,
    waveformEnterWatchfaceScreen,
    waveformLeaveWatchfaceScreen,
    waveformTickWatchfaceScreen,
    waveformWatchfaceScreenRoot,
};
} // namespace

const ScreenModule &watchfaceScreenModule()
{
  return kModule;
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

  watchTimeLabel = lv_label_create(screen);
  lv_obj_set_width(watchTimeLabel, LCD_WIDTH - 24);
  lv_obj_set_style_text_font(watchTimeLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(watchTimeLabel, lvColor(255, 255, 255), 0);
  lv_obj_set_style_text_align(watchTimeLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(watchTimeLabel, "--:--");
  lv_obj_align(watchTimeLabel, LV_ALIGN_TOP_MID, 0, kWatchTimeY);

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

  watchFirmwareLabel = lv_label_create(screen);
  lv_obj_set_width(watchFirmwareLabel, LCD_WIDTH - 36);
  lv_obj_set_style_text_font(watchFirmwareLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(watchFirmwareLabel, lvColor(120, 132, 148), 0);
  lv_obj_set_style_text_align(watchFirmwareLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(watchFirmwareLabel, firmwareUpdatedText().c_str());
  lv_obj_align(watchFirmwareLabel, LV_ALIGN_TOP_MID, 0, 332);

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

void refreshWatchface()
{
  if (!watchfaceBuilt) {
    return;
  }

  lv_label_set_text(watchTimeLabel, timeText().c_str());
  lv_label_set_text(watchDateLabel, dateText().c_str());
  lv_label_set_text(watchTimezoneLabel, timezoneText().c_str());
  lv_label_set_text(watchFirmwareLabel, firmwareUpdatedText().c_str());

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

  lv_label_set_text(watchStatusLabel, status.c_str());
}

lv_obj_t *waveformWatchfaceScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Watchface)];
}

bool waveformBuildWatchfaceScreen()
{
  if (!waveformWatchfaceScreenRoot()) {
    buildWatchfaceScreen();
  }

  return watchfaceBuilt && waveformWatchfaceScreenRoot() && watchTimeLabel && watchDateLabel &&
         watchTimezoneLabel && watchStatusLabel;
}

bool waveformRefreshWatchfaceScreen()
{
  if (!watchfaceBuilt || !watchTimeLabel || !watchDateLabel || !watchTimezoneLabel || !watchStatusLabel) {
    return false;
  }

  refreshWatchface();
  return true;
}

void waveformEnterWatchfaceScreen()
{
}

void waveformLeaveWatchfaceScreen()
{
}

void waveformTickWatchfaceScreen(uint32_t nowMs)
{
  (void)nowMs;
  refreshWatchface();
}
