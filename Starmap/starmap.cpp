#include "ota_config.h"
#include "screen_constants.h"
#include "screen_manager.h"
#ifdef SCREEN_STARMAP
#include "screen_callbacks.h"

extern lv_obj_t *screenRoots[];
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
void styleLine(lv_obj_t *line, lv_color_t color, int width);

namespace
{
const ScreenModule kModule = {
    ScreenId::Starmap,
    "Starmap",
    waveformBuildStarmapScreen,
    waveformRefreshStarmapScreen,
    waveformEnterStarmapScreen,
    waveformLeaveStarmapScreen,
    waveformTickStarmapScreen,
    waveformStarmapScreenRoot,
    nullptr,
};

struct ChartStar
{
  float nx;
  float ny;
  uint8_t mag;
};

struct ChartLine
{
  uint8_t a;
  uint8_t b;
};

constexpr ChartStar kChartStars[] = {
    // Ursa Major (Big Dipper) 0-6
    {-0.62f, -0.42f, 1}, {-0.60f, -0.30f, 3}, {-0.46f, -0.30f, 3}, {-0.48f, -0.44f, 3},
    {-0.32f, -0.52f, 2}, {-0.18f, -0.56f, 2}, {-0.05f, -0.62f, 2},
    // Cassiopeia 7-11
    { 0.18f, -0.66f, 3}, { 0.30f, -0.54f, 2}, { 0.42f, -0.64f, 3}, { 0.55f, -0.52f, 2}, { 0.68f, -0.58f, 3},
    // Cygnus 12-16
    { 0.00f, -0.40f, 1}, {-0.05f, -0.18f, 4}, {-0.22f, -0.10f, 3}, { 0.12f, -0.10f, 3}, {-0.02f,  0.08f, 2},
    // Orion 17-23
    { 0.05f,  0.22f, 1}, {-0.22f,  0.22f, 3}, {-0.12f,  0.40f, 2}, { 0.00f,  0.42f, 2},
    { 0.10f,  0.44f, 2}, { 0.15f,  0.60f, 3}, {-0.18f,  0.60f, 1},
    // Leo 24-29
    { 0.65f,  0.20f, 1}, { 0.52f,  0.08f, 3}, { 0.50f, -0.08f, 4}, { 0.60f, -0.18f, 3},
    { 0.72f, -0.08f, 3}, { 0.80f,  0.30f, 2},
    // Lyra 30-33
    {-0.70f, -0.08f, 1}, {-0.62f,  0.00f, 3}, {-0.58f,  0.10f, 3}, {-0.70f,  0.12f, 3},
    // Scorpius 34-39
    {-0.38f,  0.42f, 1}, {-0.28f,  0.38f, 3}, {-0.20f,  0.42f, 3}, {-0.15f,  0.55f, 4},
    {-0.08f,  0.68f, 3}, {-0.18f,  0.78f, 3},
    // Taurus / Aldebaran 40-43
    { 0.40f,  0.58f, 1}, { 0.28f,  0.68f, 3}, { 0.55f,  0.50f, 3}, { 0.45f,  0.75f, 3},
};

constexpr ChartLine kChartLines[] = {
    // Big Dipper
    {0,1},{1,2},{2,3},{3,0},{3,4},{4,5},{5,6},
    // Cassiopeia
    {7,8},{8,9},{9,10},{10,11},
    // Cygnus
    {12,13},{13,14},{13,15},{13,16},
    // Orion
    {18,17},{18,19},{17,21},{19,20},{20,21},{19,23},{21,22},{22,23},
    // Leo
    {24,25},{25,26},{26,27},{27,28},{28,25},{24,29},
    // Lyra
    {30,31},{31,32},{32,33},{33,30},
    // Scorpius
    {34,35},{35,36},{36,37},{37,38},{38,39},
    // Taurus
    {41,40},{40,42},{40,43},
};

constexpr size_t kChartStarCount = sizeof(kChartStars) / sizeof(kChartStars[0]);
constexpr size_t kChartLineCount = sizeof(kChartLines) / sizeof(kChartLines[0]);

// Scatter background stars in unit disc coordinates.
constexpr ChartStar kScatterStars[] = {
    {-0.90f, -0.05f, 5}, {-0.82f,  0.28f, 5}, {-0.72f, -0.40f, 5}, {-0.55f, -0.72f, 4},
    {-0.40f, -0.12f, 5}, {-0.35f, -0.78f, 5}, {-0.22f,  0.20f, 5}, {-0.10f, -0.86f, 4},
    {-0.08f,  0.52f, 5}, { 0.02f, -0.72f, 5}, { 0.05f, -0.02f, 5}, { 0.14f,  0.15f, 5},
    { 0.22f, -0.38f, 5}, { 0.28f, -0.82f, 5}, { 0.30f,  0.28f, 5}, { 0.38f,  0.00f, 5},
    { 0.46f, -0.36f, 5}, { 0.52f,  0.28f, 5}, { 0.58f, -0.40f, 5}, { 0.62f, -0.72f, 5},
    { 0.68f,  0.08f, 5}, { 0.75f,  0.42f, 5}, { 0.82f, -0.30f, 5}, { 0.86f,  0.10f, 5},
    { 0.88f, -0.12f, 5}, {-0.88f,  0.18f, 5}, {-0.78f,  0.42f, 5}, {-0.68f,  0.58f, 5},
    {-0.48f,  0.18f, 5}, {-0.42f,  0.68f, 5}, {-0.28f,  0.82f, 5}, {-0.02f,  0.82f, 5},
    { 0.20f,  0.52f, 4}, { 0.42f,  0.40f, 5}, { 0.52f,  0.62f, 5}, { 0.08f,  0.72f, 5},
    {-0.92f, -0.22f, 5}, {-0.12f,  0.32f, 5}, { 0.12f, -0.58f, 5}, { 0.38f,  0.20f, 5},
    {-0.62f, -0.62f, 5}, {-0.30f,  0.62f, 5}, { 0.18f, -0.18f, 5}, { 0.32f,  0.58f, 5},
    {-0.05f,  0.20f, 5}, {-0.52f, -0.05f, 5}, {-0.18f, -0.45f, 5}, { 0.00f, -0.22f, 5},
    { 0.64f,  0.54f, 5}, {-0.58f,  0.40f, 5},
};

constexpr size_t kScatterStarCount = sizeof(kScatterStars) / sizeof(kScatterStars[0]);

struct StarmapUi
{
  lv_obj_t *screen = nullptr;
  lv_obj_t *titleLabel = nullptr;
  lv_obj_t *panel = nullptr;
  lv_obj_t *nebula = nullptr;
  lv_obj_t *chartDots[kChartStarCount] = {};
  lv_obj_t *sparkleH[kChartStarCount] = {};
  lv_obj_t *sparkleV[kChartStarCount] = {};
  lv_obj_t *scatterDots[kScatterStarCount] = {};
  lv_obj_t *constellationLines[kChartLineCount] = {};
};

StarmapUi ui;
lv_point_precise_t kLinePoints[kChartLineCount][2] = {};
lv_point_precise_t kSparkleHPoints[kChartStarCount][2] = {};
lv_point_precise_t kSparkleVPoints[kChartStarCount][2] = {};
bool built = false;

int dotSizeForMagnitude(uint8_t mag)
{
  switch (mag) {
    case 1: return 5;
    case 2: return 4;
    case 3: return 3;
    case 4: return 2;
    default: return 1;
  }
}

lv_color_t starColorForMagnitude(uint8_t mag)
{
  if (mag <= 1) return lvColor(255, 255, 255);
  if (mag <= 2) return lvColor(230, 240, 255);
  if (mag <= 3) return lvColor(198, 214, 238);
  return lvColor(150, 170, 198);
}

lv_obj_t *makeDot(lv_obj_t *parent, int size, lv_color_t color)
{
  lv_obj_t *dot = lv_obj_create(parent);
  lv_obj_remove_style_all(dot);
  lv_obj_set_size(dot, size, size);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, color, 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_set_style_pad_all(dot, 0, 0);
  lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
  return dot;
}

void setLinePoints2(lv_point_precise_t points[2], float x1, float y1, float x2, float y2)
{
  points[0].x = x1;
  points[0].y = y1;
  points[1].x = x2;
  points[1].y = y2;
}

lv_obj_t *buildStarmapScreen()
{
  ui.screen = lv_obj_create(nullptr);
  applyRootStyle(ui.screen);

  ui.titleLabel = lv_label_create(ui.screen);
  lv_obj_set_width(ui.titleLabel, LCD_WIDTH - 40);
  lv_obj_set_style_text_font(ui.titleLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(ui.titleLabel, lvColor(232, 238, 250), 0);
  lv_obj_set_style_text_align(ui.titleLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(ui.titleLabel, "Starmap");
  lv_obj_align(ui.titleLabel, LV_ALIGN_TOP_MID, 0, kStarmapTitleY);

  ui.panel = lv_obj_create(ui.screen);
  lv_obj_remove_style_all(ui.panel);
  lv_obj_set_size(ui.panel, kStarmapPanelSize, kStarmapPanelSize);
  lv_obj_align(ui.panel, LV_ALIGN_TOP_MID, 0, kStarmapPanelY);
  lv_obj_set_style_radius(ui.panel, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(ui.panel, lvColor(10, 14, 40), 0);
  lv_obj_set_style_bg_opa(ui.panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ui.panel, 2, 0);
  lv_obj_set_style_border_color(ui.panel, lvColor(210, 220, 236), 0);
  lv_obj_set_style_border_opa(ui.panel, LV_OPA_70, 0);
  lv_obj_set_style_pad_all(ui.panel, 0, 0);
  lv_obj_clear_flag(ui.panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(ui.panel, LV_OBJ_FLAG_CLICKABLE);

  // Nebula glow — a soft cyan blob slightly off center.
  ui.nebula = lv_obj_create(ui.panel);
  lv_obj_remove_style_all(ui.nebula);
  lv_obj_set_size(ui.nebula, kStarmapPanelSize * 0.55f, kStarmapPanelSize * 0.55f);
  lv_obj_set_style_radius(ui.nebula, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(ui.nebula, lvColor(72, 140, 210), 0);
  lv_obj_set_style_bg_opa(ui.nebula, LV_OPA_20, 0);
  lv_obj_set_style_border_width(ui.nebula, 0, 0);
  lv_obj_clear_flag(ui.nebula, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(ui.nebula, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(ui.nebula, LV_ALIGN_CENTER, kStarmapPanelSize * 0.08f, kStarmapPanelSize * 0.06f);

  const int center = kStarmapPanelSize / 2;
  const int radius = (kStarmapPanelSize / 2) - 6;

  // Scatter background stars.
  for (size_t i = 0; i < kScatterStarCount; ++i) {
    int size = dotSizeForMagnitude(kScatterStars[i].mag);
    lv_color_t color = lvColor(170, 186, 214);
    ui.scatterDots[i] = makeDot(ui.panel, size, color);
    lv_obj_set_style_bg_opa(ui.scatterDots[i], LV_OPA_60, 0);
    int px = center + static_cast<int>(kScatterStars[i].nx * radius);
    int py = center + static_cast<int>(kScatterStars[i].ny * radius);
    lv_obj_set_pos(ui.scatterDots[i], px - size / 2, py - size / 2);
  }

  // Constellation lines.
  for (size_t i = 0; i < kChartLineCount; ++i) {
    const ChartStar &a = kChartStars[kChartLines[i].a];
    const ChartStar &b = kChartStars[kChartLines[i].b];
    float ax = center + a.nx * radius;
    float ay = center + a.ny * radius;
    float bx = center + b.nx * radius;
    float by = center + b.ny * radius;
    setLinePoints2(kLinePoints[i], ax, ay, bx, by);

    ui.constellationLines[i] = lv_line_create(ui.panel);
    lv_obj_set_size(ui.constellationLines[i], kStarmapPanelSize, kStarmapPanelSize);
    styleLine(ui.constellationLines[i], lvColor(186, 204, 232), 1);
    lv_obj_set_style_line_opa(ui.constellationLines[i], LV_OPA_50, 0);
    lv_line_set_points(ui.constellationLines[i], kLinePoints[i], 2);
  }

  // Chart stars with optional 4-point sparkle for the brightest ones.
  for (size_t i = 0; i < kChartStarCount; ++i) {
    const ChartStar &s = kChartStars[i];
    int size = dotSizeForMagnitude(s.mag);
    lv_color_t color = starColorForMagnitude(s.mag);
    ui.chartDots[i] = makeDot(ui.panel, size, color);
    int px = center + static_cast<int>(s.nx * radius);
    int py = center + static_cast<int>(s.ny * radius);
    lv_obj_set_pos(ui.chartDots[i], px - size / 2, py - size / 2);

    if (s.mag <= 1) {
      int arm = 8;
      setLinePoints2(kSparkleHPoints[i], px - arm, py, px + arm, py);
      setLinePoints2(kSparkleVPoints[i], px, py - arm, px, py + arm);

      ui.sparkleH[i] = lv_line_create(ui.panel);
      lv_obj_set_size(ui.sparkleH[i], kStarmapPanelSize, kStarmapPanelSize);
      styleLine(ui.sparkleH[i], lvColor(255, 255, 255), 1);
      lv_obj_set_style_line_opa(ui.sparkleH[i], LV_OPA_80, 0);
      lv_line_set_points(ui.sparkleH[i], kSparkleHPoints[i], 2);

      ui.sparkleV[i] = lv_line_create(ui.panel);
      lv_obj_set_size(ui.sparkleV[i], kStarmapPanelSize, kStarmapPanelSize);
      styleLine(ui.sparkleV[i], lvColor(255, 255, 255), 1);
      lv_obj_set_style_line_opa(ui.sparkleV[i], LV_OPA_80, 0);
      lv_line_set_points(ui.sparkleV[i], kSparkleVPoints[i], 2);
    }
  }

  built = true;
  return ui.screen;
}
} // namespace

const ScreenModule &starmapScreenModule()
{
  return kModule;
}

lv_obj_t *waveformStarmapScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Starmap)];
}

bool waveformBuildStarmapScreen()
{
  if (!waveformStarmapScreenRoot()) {
    screenRoots[static_cast<size_t>(ScreenId::Starmap)] = buildStarmapScreen();
  }
  return built && waveformStarmapScreenRoot() && ui.panel;
}

bool waveformRefreshStarmapScreen()
{
  return built && waveformStarmapScreenRoot() && ui.panel;
}

void waveformEnterStarmapScreen()
{
}

void waveformLeaveStarmapScreen()
{
}

void waveformTickStarmapScreen(uint32_t nowMs)
{
  (void)nowMs;
}

#endif // SCREEN_STARMAP
