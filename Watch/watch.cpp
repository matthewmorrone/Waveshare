#include "screen_constants.h"
#include "screen_manager.h"

extern const lv_font_t montserrat_bold_128;
#include "ble_manager.h"
#include "wifi_manager.h"
#include "screen_callbacks.h"
#include "settings_state.h"
#include "time_utils.h"
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
String timeText();
String dateText();
String timezoneText();
void enableTapBubbling(lv_obj_t *obj);
bool showScreenById(ScreenId id);
extern bool rtcReady;
extern SensorPCF85063 rtc;

extern lv_obj_t *watchTimeLabel;
extern lv_obj_t *watchTimeRow;
extern lv_obj_t *watchTimeGlyphs[5];
extern lv_obj_t *watchDateLabel;
extern lv_obj_t *watchTimezoneLabel;
extern lv_obj_t *watchBatteryTrack;
extern lv_obj_t *watchBatteryFill;
extern lv_obj_t *watchWifiIcon;
extern lv_obj_t *watchBluetoothIcon;
extern lv_obj_t *watchUsbIcon;

namespace
{
lv_obj_t *watchWifiConnectedGlyph = nullptr;
lv_obj_t *watchWifiOutlineGlyph = nullptr;
bool watchBuilt = false;

const ScreenModule kModule = {
    ScreenId::Watch,
    "Watch",
    waveformBuildWatchScreen,
    waveformRefreshWatchScreen,
    waveformEnterWatchScreen,
    waveformLeaveWatchScreen,
    waveformTickWatchScreen,
    waveformWatchScreenRoot,
};

void drawWifiArc(lv_layer_t *layer,
                 int centerX,
                 int centerY,
                 int radius,
                 int width,
                 lv_color_t color,
                 lv_opa_t opa)
{
  lv_draw_arc_dsc_t arc;
  lv_draw_arc_dsc_init(&arc);
  arc.color = color;
  arc.opa = opa;
  arc.center.x = centerX;
  arc.center.y = centerY;
  arc.radius = radius;
  arc.width = width;
  arc.start_angle = 225;
  arc.end_angle = 315;
  arc.rounded = 1;
  lv_draw_arc(layer, &arc);
}

void drawWifiDot(lv_layer_t *layer, int centerX, int centerY, int diameter, lv_color_t color, lv_opa_t opa)
{
  lv_draw_rect_dsc_t rect;
  lv_draw_rect_dsc_init(&rect);
  rect.bg_color = color;
  rect.bg_opa = opa;
  rect.radius = LV_RADIUS_CIRCLE;
  rect.border_width = 0;

  int radius = diameter / 2;
  lv_area_t area = {
      static_cast<lv_coord_t>(centerX - radius),
      static_cast<lv_coord_t>(centerY - radius),
      static_cast<lv_coord_t>(centerX + radius),
      static_cast<lv_coord_t>(centerY + radius),
  };
  lv_draw_rect(layer, &rect, &area);
}

void drawWifiSlash(lv_layer_t *layer, const lv_area_t &coords, lv_color_t color, lv_opa_t opa)
{
  lv_draw_line_dsc_t line;
  lv_draw_line_dsc_init(&line);
  line.color = color;
  line.opa = opa;
  line.width = 2;
  line.round_start = 1;
  line.round_end = 1;
  line.p1.x = coords.x1 + 4;
  line.p1.y = coords.y2 - 2;
  line.p2.x = coords.x2 - 3;
  line.p2.y = coords.y1 + 3;
  lv_draw_line(layer, &line);
}

void watchWifiOutlineDrawEvent(lv_event_t *e)
{
  lv_obj_t *obj = static_cast<lv_obj_t *>(lv_event_get_target(e));
  lv_layer_t *layer = lv_event_get_layer(e);
  if (!obj || !layer) {
    return;
  }

  lv_area_t coords;
  lv_obj_get_coords(obj, &coords);

  int centerX = (coords.x1 + coords.x2) / 2;
  int centerY = coords.y2 - 1;
  lv_color_t color = lvColor(72, 82, 96);
  lv_opa_t opa = LV_OPA_80;

  drawWifiArc(layer, centerX, centerY, 4, 1, color, opa);
  drawWifiArc(layer, centerX, centerY, 8, 1, color, opa);
  drawWifiArc(layer, centerX, centerY, 12, 1, color, opa);
  drawWifiSlash(layer, coords, color, LV_OPA_COVER);
}
} // namespace

const ScreenModule &watchScreenModule()
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
  if (!waveform::rtcTimeLooksValid(rtcTime)) {
    Serial.println("RTC time is not valid yet");
    return;
  }

  if (waveform::setSystemTimeFromTm(rtcTime)) {
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
  waveform::ensureSystemTimeAtLeastBuildTimestamp();
}

void buildWatchScreen()
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

  watchWifiIcon = lv_obj_create(iconRow);
  lv_obj_remove_style_all(watchWifiIcon);
  lv_obj_set_size(watchWifiIcon, 24, 20);
  lv_obj_clear_flag(watchWifiIcon, LV_OBJ_FLAG_SCROLLABLE);

  watchWifiConnectedGlyph = lv_label_create(watchWifiIcon);
  lv_obj_set_style_text_font(watchWifiConnectedGlyph, &lv_font_montserrat_20, 0);
  lv_label_set_text(watchWifiConnectedGlyph, LV_SYMBOL_WIFI);
  lv_obj_center(watchWifiConnectedGlyph);

  watchWifiOutlineGlyph = lv_obj_create(watchWifiIcon);
  lv_obj_remove_style_all(watchWifiOutlineGlyph);
  lv_obj_set_size(watchWifiOutlineGlyph, 24, 20);
  lv_obj_center(watchWifiOutlineGlyph);
  lv_obj_clear_flag(watchWifiOutlineGlyph, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(watchWifiOutlineGlyph, watchWifiOutlineDrawEvent, LV_EVENT_DRAW_MAIN, nullptr);

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
  lv_obj_set_width(watchTimeLabel, LV_SIZE_CONTENT);
  lv_label_set_long_mode(watchTimeLabel, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(watchTimeLabel, &montserrat_bold_128, 0);
  lv_obj_set_style_text_color(watchTimeLabel, lvColor(255, 255, 255), 0);
  lv_label_set_text(watchTimeLabel, "--:--");
  lv_obj_align(watchTimeLabel, LV_ALIGN_TOP_MID, 0, kWatchTimeY);

  watchDateLabel = lv_label_create(screen);
  lv_obj_set_width(watchDateLabel, LCD_WIDTH - 36);
  lv_obj_set_style_text_font(watchDateLabel, &lv_font_montserrat_28, 0);
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

  screenRoots[static_cast<size_t>(ScreenId::Watch)] = screen;
  watchBuilt = true;
}

void refreshWatch()
{
  if (!watchBuilt) {
    return;
  }

  lv_label_set_text(watchTimeLabel, timeText().c_str());
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

  bool wifiEnabled = settingsState().wifiEnabled;
  if (watchWifiConnectedGlyph && watchWifiOutlineGlyph) {
    if (wifiEnabled) {
      lv_obj_clear_flag(watchWifiConnectedGlyph, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(watchWifiOutlineGlyph, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_text_color(watchWifiConnectedGlyph,
                                  networkIsOnline() ? lvColor(255, 255, 255) : lvColor(72, 82, 96),
                                  0);
    } else {
      lv_obj_add_flag(watchWifiConnectedGlyph, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(watchWifiOutlineGlyph, LV_OBJ_FLAG_HIDDEN);
      lv_obj_invalidate(watchWifiOutlineGlyph);
    }
  }
  lv_obj_set_style_text_color(watchBluetoothIcon,
                              settingsState().bleEnabled ? lvColor(255, 255, 255) : lvColor(72, 82, 96),
                              0);
  lv_obj_set_style_text_color(watchUsbIcon,
                              usbIsConnected() ? (batteryIsCharging() ? lvColor(0, 120, 255) : lvColor(255, 255, 255))
                                               : lvColor(72, 82, 96),
                              0);

}

lv_obj_t *waveformWatchScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Watch)];
}

bool waveformBuildWatchScreen()
{
  if (!waveformWatchScreenRoot()) {
    buildWatchScreen();
  }

  return watchBuilt && waveformWatchScreenRoot() && watchTimeLabel && watchDateLabel &&
         watchTimezoneLabel;
}

bool waveformRefreshWatchScreen()
{
  if (!watchBuilt || !watchTimeLabel || !watchDateLabel || !watchTimezoneLabel) {
    return false;
  }

  refreshWatch();
  return true;
}

void waveformEnterWatchScreen()
{
}

void waveformLeaveWatchScreen()
{
}

void waveformTickWatchScreen(uint32_t nowMs)
{
  (void)nowMs;
  refreshWatch();
}
