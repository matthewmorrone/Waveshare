#include "config/screen_constants.h"
#include "core/screen_manager.h"

extern const lv_font_t montserrat_bold_128;
#include "modules/ble_manager.h"
#include "modules/wifi_manager.h"
#include "screens/screen_callbacks.h"
#include "state/settings_state.h"
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
void waveformDestroyCalendarScreen();
// Symbols from main.cpp
extern bool rtcReady;
extern SensorPCF85063 rtc;
extern size_t currentScreenIndex;
bool rtcTimeLooksValid(const struct tm &timeInfo);
bool setSystemTimeFromTm(const struct tm &timeInfo);

bool watchBuilt = false;
bool calendarBuilt = false;

static lv_obj_t *calendarScreen = nullptr;

static const char *kDayHeaders[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

static void populateCalendar(lv_obj_t *container)
{
  if (!container) return;

  lv_obj_clean(container);

  time_t now = time(nullptr);
  struct tm t = {};
  localtime_r(&now, &t);
  int today     = t.tm_mday;
  int todayMon  = t.tm_mon;
  int todayYear = t.tm_year;

  struct tm first = {};
  first.tm_year = t.tm_year;
  first.tm_mon  = t.tm_mon;
  first.tm_mday = 1;
  mktime(&first);
  int startWday = first.tm_wday;

  struct tm nextMonth = first;
  nextMonth.tm_mon++;
  mktime(&nextMonth);
  int daysInMonth = (int)((mktime(&nextMonth) - mktime(&first)) / 86400);

  // How many rows the grid needs
  int numRows = ((startWday + daysInMonth - 1) / 7) + 1;

  constexpr int kGridLeft  = 16;
  constexpr int kCellW     = (LCD_WIDTH - kGridLeft * 2) / 7;
  constexpr int kCellH     = 48;
  constexpr int kTitleH    = 28;   // title text height
  constexpr int kDayHdrH   = 22;   // day-of-week row height
  constexpr int kTitleGap  = 10;   // space between title and day headers

  int gridH      = kDayHdrH + numRows * kCellH;
  int totalH     = kTitleH + kTitleGap + gridH;
  int topY       = (LCD_HEIGHT - totalH) / 2;

  // Month/year header
  char header[32];
  const char *months[] = {"January","February","March","April","May","June",
                           "July","August","September","October","November","December"};
  snprintf(header, sizeof(header), "%s %d", months[t.tm_mon], t.tm_year + 1900);

  lv_obj_t *titleLabel = lv_label_create(container);
  lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(titleLabel, lvColor(220, 220, 220), 0);
  lv_label_set_text(titleLabel, header);
  lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, topY);

  int gridTop = topY + kTitleH + kTitleGap;

  // Day-of-week headers
  for (int col = 0; col < 7; ++col) {
    lv_obj_t *lbl = lv_label_create(container);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lvColor(96, 108, 124), 0);
    lv_label_set_text(lbl, kDayHeaders[col]);
    lv_obj_set_pos(lbl, kGridLeft + col * kCellW + kCellW / 2 - 8, gridTop);
  }

  int dayTop = gridTop + kDayHdrH;

  // Day cells
  int row = 0, col = startWday;
  for (int day = 1; day <= daysInMonth; ++day) {
    int x = kGridLeft + col * kCellW;
    int y = dayTop + row * kCellH;

    bool isToday = (day == today && t.tm_mon == todayMon && t.tm_year == todayYear);

    if (isToday) {
      lv_obj_t *circle = lv_obj_create(container);
      lv_obj_remove_style_all(circle);
      lv_obj_set_size(circle, 36, 36);
      lv_obj_set_style_bg_color(circle, lvColor(30, 100, 200), 0);
      lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_pos(circle, x + kCellW / 2 - 18, y + kCellH / 2 - 18);
    }

    char dayBuf[4];
    snprintf(dayBuf, sizeof(dayBuf), "%d", day);
    lv_obj_t *lbl = lv_label_create(container);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl,
      isToday ? lvColor(255, 255, 255) : lvColor(200, 200, 200), 0);
    lv_label_set_text(lbl, dayBuf);
    lv_obj_set_pos(lbl, x + kCellW / 2 - (day < 10 ? 5 : 10), y + kCellH / 2 - 10);

    ++col;
    if (col == 7) { col = 0; ++row; }
  }
}

void watchShowCalendar()
{
  showScreenById(ScreenId::Calendar);
}

void watchDismissCalendar()
{
  if (static_cast<ScreenId>(currentScreenIndex) == ScreenId::Calendar) {
    showScreenById(ScreenId::Watch);
  }
}

void watchDismissCalendarSilent()
{
  watchDismissCalendar();
}

bool watchCalendarIsVisible()
{
  return static_cast<ScreenId>(currentScreenIndex) == ScreenId::Calendar;
}

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

const ScreenModule kCalendarModule = {
    ScreenId::Calendar,
    "Calendar",
    waveformBuildCalendarScreen,
    waveformRefreshCalendarScreen,
    waveformEnterCalendarScreen,
    waveformLeaveCalendarScreen,
    waveformTickCalendarScreen,
    waveformCalendarScreenRoot,
    waveformDestroyCalendarScreen,
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

const ScreenModule &calendarScreenModule()
{
  return kCalendarModule;
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

void buildCalendarScreen()
{
  calendarScreen = lv_obj_create(nullptr);
  applyRootStyle(calendarScreen);
  populateCalendar(calendarScreen);
  screenRoots[static_cast<size_t>(ScreenId::Calendar)] = calendarScreen;
  calendarBuilt = true;
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

lv_obj_t *waveformCalendarScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Calendar)];
}

bool waveformBuildCalendarScreen()
{
  if (!waveformCalendarScreenRoot()) {
    buildCalendarScreen();
  }

  return calendarBuilt && waveformCalendarScreenRoot();
}

bool waveformRefreshCalendarScreen()
{
  if (!calendarBuilt || !calendarScreen) {
    return false;
  }

  populateCalendar(calendarScreen);
  return true;
}

void waveformEnterCalendarScreen()
{
  if (calendarScreen) {
    populateCalendar(calendarScreen);
  }
}

void waveformLeaveCalendarScreen()
{
}

void waveformTickCalendarScreen(uint32_t nowMs)
{
  (void)nowMs;
}

void waveformDestroyCalendarScreen()
{
  if (calendarScreen) {
    lv_obj_delete(calendarScreen);
    calendarScreen = nullptr;
  }

  screenRoots[static_cast<size_t>(ScreenId::Calendar)] = nullptr;
  calendarBuilt = false;
}
