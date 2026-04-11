#include "screen_constants.h"
#include "screen_manager.h"
#include "screen_callbacks.h"
#include "time_utils.h"
#include "pin_config.h"

#include <SensorPCF85063.hpp>
#include <time.h>
#include <Wire.h>

extern lv_obj_t *screenRoots[];
extern size_t currentScreenIndex;
extern SensorPCF85063 rtc;
extern bool rtcReady;
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
bool showScreenById(ScreenId id);
void waveformDestroyCalendarScreen();

namespace
{
bool calendarBuilt = false;
lv_obj_t *calendarScreen = nullptr;

const char *kDayHeaders[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

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

void populateCalendar(lv_obj_t *container)
{
  if (!container) {
    return;
  }

  lv_obj_clean(container);

  time_t now = waveform::effectiveNow();
  struct tm t = {};
  localtime_r(&now, &t);
  int today = t.tm_mday;
  int todayMon = t.tm_mon;
  int todayYear = t.tm_year;

  struct tm first = {};
  first.tm_year = t.tm_year;
  first.tm_mon = t.tm_mon;
  first.tm_mday = 1;
  mktime(&first);
  int startWday = first.tm_wday;

  struct tm nextMonth = first;
  nextMonth.tm_mon++;
  mktime(&nextMonth);
  int daysInMonth = static_cast<int>((mktime(&nextMonth) - mktime(&first)) / 86400);

  int numRows = ((startWday + daysInMonth - 1) / 7) + 1;

  constexpr int kGridLeft = 16;
  constexpr int kCellW = (LCD_WIDTH - kGridLeft * 2) / 7;
  constexpr int kCellH = 48;
  constexpr int kTitleH = 28;
  constexpr int kDayHdrH = 22;
  constexpr int kTitleGap = 10;

  int gridH = kDayHdrH + numRows * kCellH;
  int totalH = kTitleH + kTitleGap + gridH;
  int topY = (LCD_HEIGHT - totalH) / 2;

  lv_obj_t *titleLabel = lv_label_create(container);
  lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(titleLabel, lvColor(220, 220, 220), 0);
  String header = waveform::formatCurrentLocal("%B %Y", "Calendar");
  lv_label_set_text(titleLabel, header.c_str());
  lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, topY);

  int gridTop = topY + kTitleH + kTitleGap;

  for (int col = 0; col < 7; ++col) {
    lv_obj_t *lbl = lv_label_create(container);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lvColor(96, 108, 124), 0);
    lv_label_set_text(lbl, kDayHeaders[col]);
    lv_obj_set_pos(lbl, kGridLeft + col * kCellW + kCellW / 2 - 8, gridTop);
  }

  int dayTop = gridTop + kDayHdrH;
  int row = 0;
  int col = startWday;
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
    lv_obj_set_style_text_color(lbl, isToday ? lvColor(255, 255, 255) : lvColor(200, 200, 200), 0);
    lv_label_set_text(lbl, dayBuf);
    lv_obj_set_pos(lbl, x + kCellW / 2 - (day < 10 ? 5 : 10), y + kCellH / 2 - 10);

    ++col;
    if (col == 7) {
      col = 0;
      ++row;
    }
  }
}

void buildCalendarScreen()
{
  calendarScreen = lv_obj_create(nullptr);
  applyRootStyle(calendarScreen);
  populateCalendar(calendarScreen);
  screenRoots[static_cast<size_t>(ScreenId::Calendar)] = calendarScreen;
  calendarBuilt = true;
}
} // namespace

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

void watchShowCalendar()
{
  showScreenById(ScreenId::Calendar);
}

void watchDismissCalendar()
{
  (void)currentScreenIndex;
}

void watchDismissCalendarSilent()
{
  watchDismissCalendar();
}

bool watchCalendarIsVisible()
{
  return static_cast<ScreenId>(currentScreenIndex) == ScreenId::Calendar;
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
