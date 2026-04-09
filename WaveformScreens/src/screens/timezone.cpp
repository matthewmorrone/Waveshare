#include "state/settings_state.h"
#include <Preferences.h>
#include "config/pin_config.h"
#include "screens/screen_callbacks.h"
#include <lvgl.h>

extern Preferences preferences;
void applyConfiguredTimezone();

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b)
{
  return lv_color_make(r, g, b);
}

void applyRootStyle(lv_obj_t *obj)
{
  lv_obj_set_size(obj, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_style_bg_color(obj, lvColor(6, 10, 16), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_outline_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

// ---------------------------------------------------------------------------
// Timezone data
// ---------------------------------------------------------------------------

static const struct
{
  const char *name;
  const char *abbr;
  int         utcOffset;
} kTimezones[] = {
  {"Pacific/Apia",           "SMST", -11},
  {"Pacific/Honolulu",       "HST",  -10},
  {"America/Anchorage",      "AKST",  -9},
  {"America/Los_Angeles",    "PST",   -8},
  {"America/Denver",         "MST",   -7},
  {"America/Chicago",        "CST",   -6},
  {"America/New_York",       "EST",   -5},
  {"America/Halifax",        "AST",   -4},
  {"America/Sao_Paulo",      "BRT",   -3},
  {"Atlantic/South_Georgia", "GST",   -2},
  {"Atlantic/Azores",        "AZOT",  -1},
  {"UTC",                    "UTC",    0},
  {"Europe/London",          "GMT",    0},
  {"Europe/Paris",           "CET",   +1},
  {"Europe/Athens",          "EET",   +2},
  {"Africa/Nairobi",         "EAT",   +3},
  {"Asia/Dubai",             "GST",   +4},
  {"Asia/Karachi",           "PKT",   +5},
  {"Asia/Dhaka",             "BST",   +6},
  {"Asia/Bangkok",           "ICT",   +7},
  {"Asia/Shanghai",          "CST",   +8},
  {"Asia/Tokyo",             "JST",   +9},
  {"Australia/Sydney",       "AEST", +10},
  {"Pacific/Auckland",       "NZST", +11},
};
static constexpr size_t kTimezoneCount = sizeof(kTimezones) / sizeof(kTimezones[0]);

// bandIndex = utcOffset + 12  (UTC-12=0 … UTC+11=23)
static inline int offsetToBandIndex(int utcOffset)
{
  int idx = utcOffset + 12;
  if (idx < 0) idx = 0;
  if (idx >= (int)kTimezoneCount) idx = (int)kTimezoneCount - 1;
  return idx;
}

// ---------------------------------------------------------------------------
// Canvas geometry
// ---------------------------------------------------------------------------

static constexpr int   kMapW   = 330;
static constexpr int   kMapH   = 200;
static constexpr float kBandW  = (float)kMapW / 24.0f;   // ≈13.75 px

// ---------------------------------------------------------------------------
// Continent polygon data (canvas coords, 330×200)
// ---------------------------------------------------------------------------

struct Poly { lv_point_precise_t pts[12]; int count; };

static const Poly kContinents[] = {
  // North America
  { {{75,15},{95,10},{130,12},{155,20},{165,35},{160,55},{148,72},{130,78},{110,75},{88,65},{75,50},{70,32}}, 12 },
  // South America
  { {{95,102},{118,98},{128,108},{130,125},{122,148},{112,168},{100,170},{90,155},{85,135},{88,115},{0,0},{0,0}}, 10 },
  // Europe
  { {{158,22},{175,18},{192,20},{200,30},{198,45},{188,52},{172,50},{160,42},{156,30},{0,0},{0,0},{0,0}}, 9 },
  // Africa
  { {{158,68},{178,62},{200,65},{212,80},{215,100},{210,125},{200,148},{188,158},{175,155},{162,140},{155,115},{155,88}}, 12 },
  // Asia
  { {{200,15},{225,10},{258,10},{280,15},{295,25},{290,45},{275,58},{255,65},{230,68},{208,62},{198,48},{196,30}}, 12 },
  // India
  { {{222,68},{235,65},{242,78},{240,95},{232,108},{222,105},{216,90},{218,75},{0,0},{0,0},{0,0},{0,0}}, 8 },
  // SE Asia
  { {{250,65},{268,62},{278,70},{276,82},{265,88},{252,82},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}}, 6 },
  // Australia
  { {{245,112},{268,108},{285,115},{292,130},{288,148},{272,158},{252,155},{240,143},{238,128},{0,0},{0,0},{0,0}}, 9 },
};
static constexpr size_t kContinentCount = sizeof(kContinents) / sizeof(kContinents[0]);

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static lv_obj_t      *gScreen       = nullptr;
static lv_obj_t      *gCanvas       = nullptr;
static lv_draw_buf_t *gMapBuf       = nullptr;
static lv_obj_t      *gEyebrow      = nullptr;
static lv_obj_t      *gHeadline     = nullptr;
static lv_obj_t      *gSubtitle     = nullptr;
static lv_obj_t      *gPrevScreen   = nullptr;
static bool           gBuilt        = false;
static int            gSelectedIdx  = 11;  // default: UTC (index 11)

// ---------------------------------------------------------------------------
// Map rendering
// ---------------------------------------------------------------------------

static void drawLine(lv_layer_t *layer, int x1, int y1, int x2, int y2, lv_color_t color, int width, lv_opa_t opa)
{
  lv_draw_line_dsc_t dsc;
  lv_draw_line_dsc_init(&dsc);
  dsc.color = color;
  dsc.width = width;
  dsc.opa   = opa;
  dsc.p1.x  = x1; dsc.p1.y = y1;
  dsc.p2.x  = x2; dsc.p2.y = y2;
  lv_draw_line(layer, &dsc);
}

static void drawRect(lv_layer_t *layer, int x, int y, int w, int h, lv_color_t color, lv_opa_t opa)
{
  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.bg_color     = color;
  dsc.bg_opa       = opa;
  dsc.border_width = 0;
  dsc.radius       = 0;
  lv_area_t area = {(lv_coord_t)x, (lv_coord_t)y, (lv_coord_t)(x + w - 1), (lv_coord_t)(y + h - 1)};
  lv_draw_rect(layer, &dsc, &area);
}

static void drawPoly(lv_layer_t *layer, const lv_point_precise_t *pts, int count, lv_color_t color)
{
  lv_draw_triangle_dsc_t dsc;
  lv_draw_triangle_dsc_init(&dsc);
  dsc.color = color;
  dsc.opa   = LV_OPA_COVER;
  // Fan-triangulate the polygon from pts[0]
  for (int i = 1; i + 1 < count; ++i) {
    dsc.p[0] = pts[0];
    dsc.p[1] = pts[i];
    dsc.p[2] = pts[i + 1];
    lv_draw_triangle(layer, &dsc);
  }
}

static void drawMap(int activeBandIdx)
{
  if (!gMapBuf || !gCanvas) return;

  lv_canvas_fill_bg(gCanvas, lvColor(8, 17, 30), LV_OPA_COVER);

  lv_layer_t layer;
  lv_canvas_init_layer(gCanvas, &layer);

  // --- Timezone grid lines ---
  for (int i = 0; i <= 24; ++i) {
    int x = (int)(i * kBandW);
    if (x >= kMapW) x = kMapW - 1;
    drawLine(&layer, x, 0, x, kMapH - 1, lvColor(20, 35, 52), 1, LV_OPA_COVER);
  }

  // --- Continent fills ---
  for (size_t c = 0; c < kContinentCount; ++c) {
    drawPoly(&layer, kContinents[c].pts, kContinents[c].count, lvColor(30, 58, 88));
  }

  // --- Active band highlight ---
  {
    int bx = (int)(activeBandIdx * kBandW);
    int bw = (int)((activeBandIdx + 1) * kBandW) - bx;
    drawRect(&layer, bx, 0, bw, kMapH, lvColor(139, 92, 246), LV_OPA_20);
    drawLine(&layer, bx,      0, bx,      kMapH - 1, lvColor(167, 139, 250), 2, LV_OPA_COVER);
    drawLine(&layer, bx + bw, 0, bx + bw, kMapH - 1, lvColor(167, 139, 250), 2, LV_OPA_COVER);
  }

  lv_canvas_finish_layer(gCanvas, &layer);
  lv_obj_invalidate(gCanvas);
}

// ---------------------------------------------------------------------------
// Label update
// ---------------------------------------------------------------------------

static void updateLabels()
{
  if (gSelectedIdx < 0 || gSelectedIdx >= (int)kTimezoneCount) return;
  if (!gHeadline || !gSubtitle) return;

  const char *name   = kTimezones[gSelectedIdx].name;
  const char *abbr   = kTimezones[gSelectedIdx].abbr;
  int         offset = kTimezones[gSelectedIdx].utcOffset;

  lv_label_set_text(gHeadline, name);

  char utcBuf[32];
  if (offset == 0) {
    snprintf(utcBuf, sizeof(utcBuf), "UTC+0  %s", abbr);
  } else if (offset > 0) {
    snprintf(utcBuf, sizeof(utcBuf), "UTC+%d  %s", offset, abbr);
  } else {
    snprintf(utcBuf, sizeof(utcBuf), "UTC%d  %s", offset, abbr);
  }
  lv_label_set_text(gSubtitle, utcBuf);
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

static void buildTimezonePicker()
{
  // Allocate canvas buffer
  gMapBuf = lv_draw_buf_create(kMapW, kMapH, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
  if (!gMapBuf) return;

  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);

  // Eyebrow
  gEyebrow = lv_label_create(screen);
  lv_obj_set_style_text_font(gEyebrow, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(gEyebrow, lvColor(110, 126, 148), 0);
  lv_label_set_text(gEyebrow, "TIMEZONE");
  lv_obj_align(gEyebrow, LV_ALIGN_TOP_LEFT, 20, 18);

  // Headline — timezone name
  gHeadline = lv_label_create(screen);
  lv_obj_set_style_text_font(gHeadline, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(gHeadline, lvColor(244, 247, 252), 0);
  lv_obj_set_width(gHeadline, LCD_WIDTH - 40);
  lv_label_set_long_mode(gHeadline, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_align(gHeadline, LV_ALIGN_TOP_LEFT, 20, 42);

  // Subtitle — UTC offset + abbreviation
  gSubtitle = lv_label_create(screen);
  lv_obj_set_style_text_font(gSubtitle, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(gSubtitle, lvColor(167, 139, 250), 0);
  lv_obj_align(gSubtitle, LV_ALIGN_TOP_LEFT, 20, 78);

  // Canvas (map)
  gCanvas = lv_canvas_create(screen);
  lv_canvas_set_draw_buf(gCanvas, gMapBuf);
  lv_obj_set_size(gCanvas, kMapW, kMapH);
  lv_obj_align(gCanvas, LV_ALIGN_TOP_MID, 0, 100);

  // Confirm button
  lv_obj_t *btn = lv_button_create(screen);
  lv_obj_set_size(btn, LCD_WIDTH - 40, 52);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -28);
  lv_obj_set_style_bg_color(btn, lvColor(124, 58, 237), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(btn, 16, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_shadow_width(btn, 20, 0);
  lv_obj_set_style_shadow_color(btn, lvColor(124, 58, 237), 0);
  lv_obj_set_style_shadow_opa(btn, LV_OPA_30, 0);
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_add_event_cb(btn, [](lv_event_t *) {
    settingsState().utcOffsetHours = kTimezones[gSelectedIdx].utcOffset;
    settingsSave(preferences);
    applyConfiguredTimezone();
    hideTimezonePicker();
  }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *btnLabel = lv_label_create(btn);
  lv_obj_set_style_text_font(btnLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(btnLabel, lvColor(244, 247, 252), 0);
  lv_label_set_text(btnLabel, "Confirm");
  lv_obj_align(btnLabel, LV_ALIGN_CENTER, 0, 0);

  gScreen = screen;
  gBuilt  = true;

  updateLabels();
  drawMap(gSelectedIdx);
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void timezonePickerSetInitial(int utcOffset)
{
  // Find the first timezone entry matching the offset
  for (size_t i = 0; i < kTimezoneCount; ++i) {
    if (kTimezones[i].utcOffset == utcOffset) {
      gSelectedIdx = (int)i;
      return;
    }
  }
  // Fallback: use band math
  gSelectedIdx = offsetToBandIndex(utcOffset);
  if (gSelectedIdx < 0) gSelectedIdx = 0;
  if (gSelectedIdx >= (int)kTimezoneCount) gSelectedIdx = (int)kTimezoneCount - 1;
}

void timezonePickerHandleSwipe(int deltaX)
{
  if (!gBuilt) return;

  // Positive deltaX = swipe right = go to earlier (lower index) timezone
  // Negative deltaX = swipe left  = go to later (higher index) timezone
  static constexpr int kSwipeThreshold = 20;

  if (deltaX > kSwipeThreshold) {
    gSelectedIdx--;
    if (gSelectedIdx < 0) gSelectedIdx = (int)kTimezoneCount - 1;
  } else if (deltaX < -kSwipeThreshold) {
    gSelectedIdx++;
    if (gSelectedIdx >= (int)kTimezoneCount) gSelectedIdx = 0;
  } else {
    return;
  }

  updateLabels();
  drawMap(gSelectedIdx);
}

void showTimezonePicker()
{
  if (!gBuilt) {
    buildTimezonePicker();
  }
  if (!gBuilt || !gScreen) return;

  // Remember what's currently on screen so we can return to it
  gPrevScreen = lv_screen_active();

  updateLabels();
  drawMap(gSelectedIdx);

  lv_screen_load(gScreen);
}

void hideTimezonePicker()
{
  if (gPrevScreen) {
    lv_screen_load(gPrevScreen);
    gPrevScreen = nullptr;
  }
}
