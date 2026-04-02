#include "config/screen_constants.h"
#include "core/screen_manager.h"
#include "screens/screen_callbacks.h"

#ifdef SCREEN_ORB
#include <cmath>
#include <esp_heap_caps.h>

extern lv_obj_t *screenRoots[];
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
static void destroyOrbScreen();

namespace
{
const ScreenModule kModule = {
    ScreenId::Orb,
    "Orb",
    waveformBuildOrbScreen,
    waveformRefreshOrbScreen,
    waveformEnterOrbScreen,
    waveformLeaveOrbScreen,
    waveformTickOrbScreen,
    waveformOrbScreenRoot,
    destroyOrbScreen,
};
} // namespace

const ScreenModule &orbScreenModule() { return kModule; }

// ---------------------------------------------------------------------------
// Layout constants
// Reference images: crescent is ~65% of frame width, thick C-shape.
// Display is 368×448. Orbit radius ~120px. Crescent radius ~110px.
// ---------------------------------------------------------------------------

static constexpr int   kW        = LCD_WIDTH;    // 368
static constexpr int   kH        = LCD_HEIGHT;   // 448
static constexpr int   kCX       = kW / 2;       // 184
static constexpr int   kCY       = kH / 2;       // 224

// Red orb
static constexpr int   kOrbR     = 38;
static constexpr int   kBorderW  = 5;

// Crescent canvas: 120×120 px, RGB565 = 28.8KB — fits in fragmented heap
static constexpr int   kCresR    = 60;           // moon disc radius — 120×120×2 = 28.8KB
static constexpr int   kCresD    = kCresR * 2;   // 120

// Orbit: radius at which crescent centre travels
static constexpr float kOrbitR   = 110.0f;

// Periods
static constexpr float kOrbitMs  = 9000.0f;   // one full orbit
static constexpr float kSpinMs   = 4500.0f;   // crescent self-rotation (ω₂ = 2×ω₁)
static constexpr float kCloudMs  = 22000.0f;  // cloud crossing time
static constexpr uint32_t kOrbFrameIntervalMs = 50;

// Cloud puff geometry (circles relative to anchor x)
struct CloudCircle { int16_t dx, dy, d; uint8_t opa; };
static constexpr CloudCircle kCloud[] = {
  {   0,  30,  90, 58}, {  55,  10, 105, 62}, { 115,  25,  88, 56},
  { 160,  35,  75, 50}, {  28,  52, 110, 46}, {  90,  58,  95, 44},
  { 145,  55,  78, 40}, {   8,  72, 130, 34}, { 105,  75, 108, 32},
  { 195,  42,  68, 42},
};
static constexpr size_t kCloudCount = sizeof(kCloud) / sizeof(kCloud[0]);
static constexpr int    kCloudSpan  = 265;

// Stars
struct Star { int16_t x, y; uint8_t r, v; };
static constexpr Star kStars[] = {
  { 40,210,2,230},{ 90,310,1,200},{160,390,1,185},{290,260,2,220},
  { 30,380,1,195},{310,180,1,210},{330,380,1,175},{ 70,420,2,205},
  {250,355,1,190},{340, 80,1,215},
};
static constexpr size_t kStarCount = sizeof(kStars)/sizeof(kStars[0]);

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static lv_obj_t      *orbScreen      = nullptr;
static lv_obj_t      *crescentCanvas = nullptr;
static lv_draw_buf_t  crescentBuf    = {};
static void          *crescentData   = nullptr;
static lv_obj_t      *cloudObjs[kCloudCount] = {};
static bool           orbBuilt       = false;
static uint32_t       orbLastFrameAtMs = 0;
static int            orbLastCloudAnchor = INT32_MIN;
static int            orbLastCrescentX = INT32_MIN;
static int            orbLastCrescentY = INT32_MIN;
static int16_t        orbLastRotation = INT16_MIN;

static void clearOrbState()
{
  orbScreen = nullptr;
  crescentCanvas = nullptr;
  crescentBuf = {};
  for (size_t i = 0; i < kCloudCount; ++i) {
    cloudObjs[i] = nullptr;
  }
  orbBuilt = false;
  orbLastFrameAtMs = 0;
  orbLastCloudAnchor = INT32_MIN;
  orbLastCrescentX = INT32_MIN;
  orbLastCrescentY = INT32_MIN;
  orbLastRotation = INT16_MIN;
}

// ---------------------------------------------------------------------------
// Render crescent: thick C-shape on black background
// moonR = outer radius, maskR = inner cutout radius, maskOX = cutout offset
// Result: a proper crescent with arm width ~35% of moonR
// ---------------------------------------------------------------------------

static void renderCrescent()
{
  if (!crescentData) return;
  uint16_t *px = (uint16_t *)crescentData;

  const float R      = (float)kCresR;
  const float moonR  = R * 0.94f;
  const float maskR  = moonR * 0.68f;   // inner cutout — larger = thinner crescent
  const float maskOX = moonR * 0.50f;   // positive = cutout offset right = C opens left; negate to flip

  for (int y = 0; y < kCresD; ++y) {
    for (int x = 0; x < kCresD; ++x) {
      float dx = (float)x - R;
      float dy = (float)y - R;
      float dist = sqrtf(dx*dx + dy*dy);

      if (dist > moonR) { px[y*kCresD+x] = 0; continue; }

      float mdx = dx - maskOX;
      if (sqrtf(mdx*mdx + dy*dy) < maskR) { px[y*kCresD+x] = 0; continue; }

      // Warm white-yellow, slightly darker toward edge
      float t   = dist / moonR;
      uint8_t r = (uint8_t)(248 - t * 18);
      uint8_t g = (uint8_t)(245 - t * 15);
      uint8_t b = (uint8_t)(210 - t * 20);
      px[y*kCresD+x] = ((uint16_t)(r>>3)<<11)|((uint16_t)(g>>2)<<5)|(b>>3);
    }
  }
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

static void buildOrbScreen()
{
  uint32_t bytes = (uint32_t)kCresD * kCresD * 2;
  if (orbScreen) {
    return;
  }

  crescentData = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
  if (!crescentData) {
    clearOrbState();
    return;
  }

  lv_draw_buf_init(&crescentBuf, kCresD, kCresD, LV_COLOR_FORMAT_RGB565,
                   kCresD * 2, crescentData, bytes);
  renderCrescent();

  lv_obj_t *screen = lv_obj_create(nullptr);
  if (!screen) {
    destroyOrbScreen();
    return;
  }
  applyRootStyle(screen);

  // Sky gradient: deep indigo top → dusky teal bottom
  lv_obj_set_style_bg_color(screen, lvColor(20, 16, 50), 0);
  lv_obj_set_style_bg_grad_color(screen, lvColor(48, 62, 98), 0);
  lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  // Stars
  for (size_t i = 0; i < kStarCount; ++i) {
    lv_obj_t *s = lv_obj_create(screen);
    if (!s) {
      destroyOrbScreen();
      return;
    }
    int d = kStars[i].r * 2;
    lv_obj_set_size(s, d, d);
    lv_obj_set_pos(s, kStars[i].x - kStars[i].r, kStars[i].y - kStars[i].r);
    lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, 0);
    uint8_t v = kStars[i].v;
    lv_obj_set_style_bg_color(s, lvColor(v, v, v), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_CLICKABLE);
  }

  // Cloud circles
  for (size_t i = 0; i < kCloudCount; ++i) {
    lv_obj_t *c = lv_obj_create(screen);
    if (!c) {
      destroyOrbScreen();
      return;
    }
    lv_obj_set_size(c, kCloud[i].d, kCloud[i].d);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(c, lvColor(200, 210, 228), 0);
    lv_obj_set_style_bg_opa(c, kCloud[i].opa, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(c, kW + kCloud[i].dx, kCloud[i].dy);
    cloudObjs[i] = c;
  }

  // Allow crescent to move outside screen bounds without clipping
  lv_obj_add_flag(screen, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  // Crescent canvas (drawn behind orb, on top of sky)
  crescentCanvas = lv_canvas_create(screen);
  if (!crescentCanvas) {
    destroyOrbScreen();
    return;
  }
  lv_canvas_set_draw_buf(crescentCanvas, &crescentBuf);
  lv_obj_set_size(crescentCanvas, kCresD, kCresD);
  lv_obj_set_style_bg_opa(crescentCanvas, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(crescentCanvas, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(crescentCanvas, LV_OBJ_FLAG_CLICKABLE);
  // Black pixels in canvas blend against sky — use recolor to make black transparent
  lv_obj_set_style_blend_mode(crescentCanvas, LV_BLEND_MODE_ADDITIVE, 0);
  // Set pivot to centre of canvas for rotation
  lv_obj_set_style_transform_pivot_x(crescentCanvas, kCresR, 0);
  lv_obj_set_style_transform_pivot_y(crescentCanvas, kCresR, 0);
  lv_obj_set_pos(crescentCanvas, kCX + (int)kOrbitR - kCresR, kCY - kCresR);

  // Glow rings around orb
  static const struct { int r; uint8_t rr,gg,bb,opa; } kGlows[] = {
    {kOrbR+40, 200,50,65, 14},
    {kOrbR+24, 215,48,62, 30},
    {kOrbR+12, 225,46,60, 55},
    {kOrbR+ 5, 232,44,58, 85},
  };
  for (auto &g : kGlows) {
    lv_obj_t *gl = lv_obj_create(screen);
    if (!gl) {
      destroyOrbScreen();
      return;
    }
    lv_obj_set_size(gl, g.r*2, g.r*2);
    lv_obj_set_style_radius(gl, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(gl, lvColor(g.rr,g.gg,g.bb), 0);
    lv_obj_set_style_bg_opa(gl, g.opa, 0);
    lv_obj_set_style_border_width(gl, 0, 0);
    lv_obj_set_style_pad_all(gl, 0, 0);
    lv_obj_clear_flag(gl, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(gl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(gl, LV_ALIGN_CENTER, 0, 0);
  }

  // Gold border
  {
    lv_obj_t *g = lv_obj_create(screen);
    if (!g) {
      destroyOrbScreen();
      return;
    }
    int d = (kOrbR + kBorderW) * 2;
    lv_obj_set_size(g, d, d);
    lv_obj_set_style_radius(g, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g, lvColor(200, 162, 48), 0);
    lv_obj_set_style_bg_opa(g, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g, 0, 0);
    lv_obj_set_style_pad_all(g, 0, 0);
    lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(g, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(g, LV_ALIGN_CENTER, 0, 0);
  }

  // Red orb
  {
    lv_obj_t *o = lv_obj_create(screen);
    if (!o) {
      destroyOrbScreen();
      return;
    }
    int d = kOrbR * 2;
    lv_obj_set_size(o, d, d);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, lvColor(218, 48, 68), 0);
    lv_obj_set_style_bg_grad_color(o, lvColor(168, 28, 42), 0);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(o, LV_ALIGN_CENTER, 0, 0);
  }

  screenRoots[static_cast<size_t>(ScreenId::Orb)] = screen;
  orbScreen = screen;
  orbBuilt  = true;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

lv_obj_t *waveformOrbScreenRoot() { return screenRoots[static_cast<size_t>(ScreenId::Orb)]; }

bool waveformBuildOrbScreen()
{
  if (!waveformOrbScreenRoot()) buildOrbScreen();
  return orbBuilt && waveformOrbScreenRoot();
}

bool waveformRefreshOrbScreen()
{
  return orbBuilt && waveformOrbScreenRoot() && crescentCanvas && crescentData;
}
void waveformEnterOrbScreen()   {}
void waveformLeaveOrbScreen()   {}

void waveformTickOrbScreen(uint32_t nowMs)
{
  if (!orbBuilt || !crescentCanvas) return;
  if (orbLastFrameAtMs != 0 && (nowMs - orbLastFrameAtMs) < kOrbFrameIntervalMs) return;
  orbLastFrameAtMs = nowMs;

  // ── Cloud: single puff drifting right→left ──────────────────────────────
  float panFrac = fmodf((float)nowMs / kCloudMs, 1.0f);
  int anchor = kW - (int)(panFrac * (float)(kW + kCloudSpan));
  if (anchor != orbLastCloudAnchor) {
    orbLastCloudAnchor = anchor;
    for (size_t i = 0; i < kCloudCount; ++i) {
      if (cloudObjs[i]) {
        lv_obj_set_pos(cloudObjs[i], anchor + kCloud[i].dx, kCloud[i].dy);
      }
    }
  }

  // ── Orbital position (ω₁) ───────────────────────────────────────────────
  float θ1 = (fmodf((float)nowMs / kOrbitMs, 1.0f)) * 2.0f * (float)M_PI;
  int ox = kCX + (int)(cosf(θ1) * kOrbitR);
  int oy = kCY + (int)(sinf(θ1) * kOrbitR);
  int crescentX = ox - kCresR;
  int crescentY = oy - kCresR;
  if (crescentX != orbLastCrescentX || crescentY != orbLastCrescentY) {
    orbLastCrescentX = crescentX;
    orbLastCrescentY = crescentY;
    lv_obj_set_pos(crescentCanvas, crescentX, crescentY);
  }

  // ── Self-rotation (ω₂): independent spin rate ───────────────────────────
  float θ2      = (fmodf((float)nowMs / kSpinMs, 1.0f)) * 360.0f;
  int16_t deg10 = (int16_t)((int32_t)(θ2 * 10.0f) % 3600);
  if (deg10 != orbLastRotation) {
    orbLastRotation = deg10;
    lv_obj_set_style_transform_rotation(crescentCanvas, deg10, 0);
  }
}

static void destroyOrbScreen()
{
  if (orbScreen) {
    lv_obj_delete(orbScreen);
  }

  screenRoots[static_cast<size_t>(ScreenId::Orb)] = nullptr;

  if (crescentData) {
    heap_caps_free(crescentData);
    crescentData = nullptr;
  }

  clearOrbState();
}

#endif // SCREEN_ORB
