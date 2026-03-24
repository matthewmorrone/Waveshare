#include "config/screen_constants.h"
#include "core/screen_manager.h"
#include "screens/screen_callbacks.h"
#include <cmath>
#include <time.h>

extern lv_obj_t *screenRoots[];
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
void noteActivity();

namespace
{
const ScreenModule kModule = {
    ScreenId::Moon,
    "Moon",
    waveformBuildMoonScreen,
    waveformRefreshMoonScreen,
    waveformEnterMoonScreen,
    waveformLeaveMoonScreen,
    waveformTickMoonScreen,
    waveformMoonScreenRoot,
};
} // namespace

const ScreenModule &moonScreenModule()
{
  return kModule;
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr int   kMoonR       = 160;
static constexpr int   kMoonD       = kMoonR * 2;
static constexpr float kSynodicDays = 29.53058770f;

static lv_obj_t    *moonScreen  = nullptr;
static lv_obj_t    *moonCanvas  = nullptr;
static lv_obj_t    *phaseLabel  = nullptr;
static lv_draw_buf_t *moonBuf   = nullptr;
static bool          moonBuilt  = false;

// ---------------------------------------------------------------------------
// Phase calculation
// ---------------------------------------------------------------------------

static float moonAgeInDays()
{
  time_t now = time(nullptr);
  const double jd        = 2440587.5 + (double)now / 86400.0;
  const double daysSince = jd - 2451549.2597;
  double age = fmod(daysSince, (double)kSynodicDays);
  if (age < 0) age += kSynodicDays;
  return (float)age;
}

static const char *phaseName(float age)
{
  float f = age / kSynodicDays;
  if (f < 0.033f || f >= 0.967f) return "New Moon";
  if (f < 0.133f)                 return "Waxing Crescent";
  if (f < 0.183f)                 return "First Quarter";
  if (f < 0.383f)                 return "Waxing Gibbous";
  if (f < 0.533f)                 return "Full Moon";
  if (f < 0.633f)                 return "Waning Gibbous";
  if (f < 0.683f)                 return "Last Quarter";
  if (f < 0.883f)                 return "Waning Crescent";
  return "New Moon";
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

struct Crater { float cx, cy, r, dark; };
static constexpr Crater kCraters[] = {
  { 0.05f,  -0.55f, 0.06f, -0.18f },  // Tycho
  { -0.30f,  0.20f, 0.07f, -0.15f },  // Copernicus
  { -0.35f,  0.35f, 0.22f, -0.25f },  // Mare Imbrium
  {  0.20f,  0.35f, 0.14f, -0.22f },  // Mare Serenitatis
  {  0.30f,  0.15f, 0.13f, -0.20f },  // Mare Tranquillitatis
  {  0.55f,  0.30f, 0.09f, -0.23f },  // Mare Crisium
  { -0.15f, -0.25f, 0.12f, -0.18f },  // Mare Nubium
  { -0.25f,  0.60f, 0.06f, -0.20f },  // Plato
};
static constexpr size_t kCraterCount = sizeof(kCraters) / sizeof(kCraters[0]);

static void renderMoon(float ageDays)
{
  if (!moonBuf || !moonBuf->data) return;

  const float R        = (float)kMoonR;
  const float R2       = R * R;
  const float phase    = (ageDays / kSynodicDays) * 2.0f * (float)M_PI;
  const float cosPhase = cosf(phase);
  const bool  waxing   = ageDays < (kSynodicDays * 0.5f);

  uint16_t *pixels = (uint16_t *)moonBuf->data;
  const uint32_t stride = moonBuf->header.stride / 2;  // stride in pixels

  for (int py = 0; py < kMoonD; ++py) {
    for (int px = 0; px < kMoonD; ++px) {
      float dx = (float)px - R + 0.5f;
      float dy = (float)py - R + 0.5f;
      float d2 = dx * dx + dy * dy;

      if (d2 > R2) {
        pixels[py * stride + px] = 0x0000;
        continue;
      }

      float dist = sqrtf(d2) / R;
      float base = 0.72f * (1.0f - dist * dist * 0.35f);

      float nx = dx / R, ny = dy / R;
      for (size_t i = 0; i < kCraterCount; ++i) {
        float cdx = nx - kCraters[i].cx;
        float cdy = ny - kCraters[i].cy;
        float cr  = kCraters[i].r;
        if (cdx * cdx + cdy * cdy < cr * cr) {
          float t = sqrtf(cdx * cdx + cdy * cdy) / cr;
          base += kCraters[i].dark * (1.0f - t * t * 0.5f);
        }
      }
      if (base < 0.0f) base = 0.0f;
      if (base > 1.0f) base = 1.0f;

      float termX    = cosPhase * sqrtf(R2 - dy * dy);
      float edgeDist = (waxing ? (dx - termX) : (termX - dx)) / R;
      float shadow;
      if (edgeDist >  0.015f) shadow = 0.0f;
      else if (edgeDist < -0.015f) shadow = 1.0f;
      else shadow = 0.5f - edgeDist / 0.03f * 0.5f;

      uint8_t v = (uint8_t)((base * (1.0f - shadow * 0.94f)) * 255.0f);
      pixels[py * stride + px] = ((uint16_t)(v >> 3) << 11) |
                                  ((uint16_t)(v >> 2) << 5)  |
                                  (v >> 3);
    }
  }
}

// ---------------------------------------------------------------------------
// Build / lifecycle
// ---------------------------------------------------------------------------

static void buildMoonScreen()
{
  moonBuf = lv_draw_buf_create(kMoonD, kMoonD, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
  if (!moonBuf) return;

  renderMoon(moonAgeInDays());

  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);

  moonCanvas = lv_canvas_create(screen);
  lv_canvas_set_draw_buf(moonCanvas, moonBuf);
  lv_obj_align(moonCanvas, LV_ALIGN_CENTER, 0, -14);

  phaseLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(phaseLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(phaseLabel, lvColor(160, 170, 185), 0);
  lv_label_set_text(phaseLabel, phaseName(moonAgeInDays()));
  lv_obj_align(phaseLabel, LV_ALIGN_BOTTOM_MID, 0, -18);

  screenRoots[static_cast<size_t>(ScreenId::Moon)] = screen;
  moonScreen = screen;
  moonBuilt  = true;
}

lv_obj_t *waveformMoonScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Moon)];
}

bool waveformBuildMoonScreen()
{
  if (!waveformMoonScreenRoot()) buildMoonScreen();
  return moonBuilt && waveformMoonScreenRoot() && moonCanvas;
}

bool waveformRefreshMoonScreen()
{
  return moonBuilt && moonCanvas;
}

void waveformEnterMoonScreen()
{
  if (!moonBuilt || !moonCanvas || !phaseLabel || !moonBuf) return;
  float age = moonAgeInDays();
  renderMoon(age);
  lv_obj_invalidate(moonCanvas);
  lv_label_set_text(phaseLabel, phaseName(age));
}

void waveformLeaveMoonScreen() {}
void waveformTickMoonScreen(uint32_t) {}
