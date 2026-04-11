#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "Arduino_DriveBus_Library.h"
#include "pin_config.h"
#include <Wire.h>
#include "HWCDC.h"

HWCDC USBSerial;
// SensorPCF85063 rtc;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS /* CS */, LCD_SCLK /* SCK */, LCD_SDIO0 /* SDIO0 */, LCD_SDIO1 /* SDIO1 */,
  LCD_SDIO2 /* SDIO2 */, LCD_SDIO3 /* SDIO3 */);

Arduino_SH8601 *gfx = new Arduino_SH8601(
    bus, GFX_NOT_DEFINED /* RST */, 0 /* rotation */, LCD_WIDTH /* width */, LCD_HEIGHT /* height */);

#define BACKGROUND RGB565_BLACK
#define MARK_COLOR RGB565_WHITE
#define SUBMARK_COLOR RGB565_DARKGREY  // RGB565_LIGHTGREY
#define HOUR_COLOR RGB565_WHITE
#define MINUTE_COLOR RGB565_BLUE  // RGB565_LIGHTGREY
#define SECOND_COLOR RGB565_RED

#define SIXTIETH 0.016666667
#define TWELFTH 0.08333333
#define SIXTIETH_RADIAN 0.10471976
#define TWELFTH_RADIAN 0.52359878
#define RIGHT_ANGLE_RADIAN 1.5707963

static uint8_t conv2d(const char *p)
{
  uint8_t v = 0;
  return (10 * (*p - '0')) + (*++p - '0');
}

static int16_t w, h, center, cx, cy;
static int16_t hHandLen, mHandLen, sHandLen, markLen;
static float sdeg, mdeg, hdeg;
static int16_t osx = 0, osy = 0, omx = 0, omy = 0, ohx = 0, ohy = 0; // Saved H, M, S x & y coords
static int16_t nsx, nsy, nmx, nmy, nhx, nhy;                         // H, M, S x & y coords
static int16_t xMin, yMin, xMax, yMax;                               // RGB565_REDraw range
static int16_t hh, mm, ss;
static unsigned long targetTime; // next action time

// Previous hand endpoints for erase
static int16_t psx = -1, psy = -1, pmx = -1, pmy = -1, phx = -1, phy = -1;

// Blend fg over bg in RGB565 by alpha (0..255)
static inline uint16_t blend565(uint16_t bg, uint16_t fg, uint8_t a)
{
  uint16_t br = (bg >> 11) & 0x1F;
  uint16_t bgn = (bg >> 5) & 0x3F;
  uint16_t bb = bg & 0x1F;
  uint16_t fr = (fg >> 11) & 0x1F;
  uint16_t fgn = (fg >> 5) & 0x3F;
  uint16_t fb = fg & 0x1F;
  uint16_t r = (br * (255 - a) + fr * a) / 255;
  uint16_t g = (bgn * (255 - a) + fgn * a) / 255;
  uint16_t b = (bb * (255 - a) + fb * a) / 255;
  return (r << 11) | (g << 5) | b;
}

static inline void wu_plot(int16_t x, int16_t y, uint16_t color, uint8_t alpha)
{
  if (alpha == 0) return;
  uint16_t c = (color == BACKGROUND) ? BACKGROUND : blend565(BACKGROUND, color, alpha);
  gfx->writePixel(x, y, c);
}

// Xiaolin Wu antialiased line (single pixel thick)
static void draw_line_aa_1(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
  bool steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) { int16_t t = x0; x0 = y0; y0 = t; t = x1; x1 = y1; y1 = t; }
  if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }

  float dx = x1 - x0;
  float dy = y1 - y0;
  float gradient = (dx == 0.0f) ? 1.0f : dy / dx;
  float intery = y0 + gradient;

  // Endpoints (solid)
  if (steep) {
    wu_plot(y0, x0, color, 255);
    wu_plot(y1, x1, color, 255);
  } else {
    wu_plot(x0, y0, color, 255);
    wu_plot(x1, y1, color, 255);
  }

  for (int16_t x = x0 + 1; x < x1; x++) {
    int16_t iy = (int16_t)intery;
    float frac = intery - iy;
    uint8_t a1 = (uint8_t)((1.0f - frac) * 255);
    uint8_t a2 = (uint8_t)(frac * 255);
    if (steep) {
      wu_plot(iy, x, color, a1);
      wu_plot(iy + 1, x, color, a2);
    } else {
      wu_plot(x, iy, color, a1);
      wu_plot(x, iy + 1, color, a2);
    }
    intery += gradient;
  }
}

// 2px-thick antialiased line: draw twice, offset by 1px perpendicular.
static void draw_line_aa(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
  float dx = x1 - x0;
  float dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.5f) {
    draw_line_aa_1(x0, y0, x1, y1, color);
    return;
  }
  // Perpendicular unit vector
  int16_t ox = (int16_t)roundf(-dy / len);
  int16_t oy = (int16_t)roundf(dx / len);
  draw_line_aa_1(x0, y0, x1, y1, color);
  draw_line_aa_1(x0 + ox, y0 + oy, x1 + ox, y1 + oy, color);
}

// Like draw_line_aa but starts at distance `skip` from (x0,y0) along the line.
// Used to erase old hands without touching the center hub.
static void draw_line_aa_skip(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color, int16_t skip)
{
  float dx = x1 - x0;
  float dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy);
  if (len <= skip) return;
  float t = skip / len;
  int16_t sx = (int16_t)(x0 + dx * t);
  int16_t sy = (int16_t)(y0 + dy * t);
  draw_line_aa(sx, sy, x1, y1, color);
}

void setup(void)
{
  USBSerial.begin(115200);
  USBSerial.println("Arduino_GFX Clock example");

  // Init Display
  if (!gfx->begin())
  {
    USBSerial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(BACKGROUND);

  gfx->setBrightness(255);

  // init LCD constant
  w = gfx->width();
  h = gfx->height();
  cx = w / 2;
  cy = h / 2;
  center = (w < h) ? (w / 2) : (h / 2);
  hHandLen = center * 3 / 8;
  mHandLen = center * 2 / 3;
  sHandLen = center * 5 / 6;
  markLen = sHandLen / 6;

  // Draw 60 clock marks
  draw_round_clock_mark(
      // draw_square_clock_mark(
      center - markLen, center,
      center - (markLen * 2 / 3), center,
      center - (markLen / 2), center);

  hh = conv2d(__TIME__);
  mm = conv2d(__TIME__ + 3);
  ss = conv2d(__TIME__ + 6);

  targetTime = ((millis() / 1000) + 1) * 1000;
}

void loop()
{
  unsigned long cur_millis = millis();
  if (cur_millis >= targetTime)
  {
    targetTime += 1000;
    ss++; // Advance second
    if (ss == 60)
    {
      ss = 0;
      mm++; // Advance minute
      if (mm > 59)
      {
        mm = 0;
        hh++; // Advance hour
        if (hh > 23)
        {
          hh = 0;
        }
      }
    }
  }

  // Pre-compute hand degrees, x & y coords for a fast screen update
  sdeg = SIXTIETH_RADIAN * ((0.001 * (cur_millis % 1000)) + ss); // 0-59 (includes millis)
  nsx = cos(sdeg - RIGHT_ANGLE_RADIAN) * sHandLen + cx;
  nsy = sin(sdeg - RIGHT_ANGLE_RADIAN) * sHandLen + cy;
  if ((nsx != osx) || (nsy != osy))
  {
    mdeg = (SIXTIETH * sdeg) + (SIXTIETH_RADIAN * mm); // 0-59 (includes seconds)
    hdeg = (TWELFTH * mdeg) + (TWELFTH_RADIAN * hh);   // 0-11 (includes minutes)
    mdeg -= RIGHT_ANGLE_RADIAN;
    hdeg -= RIGHT_ANGLE_RADIAN;
    nmx = cos(mdeg) * mHandLen + cx;
    nmy = sin(mdeg) * mHandLen + cy;
    nhx = cos(hdeg) * hHandLen + cx;
    nhy = sin(hdeg) * hHandLen + cy;

    // Redraw hands with antialiasing — draw new first, then erase old,
    // then redraw everything on top so there's never a blank frame.
    gfx->startWrite();
    draw_line_aa(cx, cy, nhx, nhy, HOUR_COLOR);
    draw_line_aa(cx, cy, nmx, nmy, MINUTE_COLOR);
    draw_line_aa(cx, cy, nsx, nsy, SECOND_COLOR);
    // Erase previous hands, skipping the center hub where all hands overlap
    int16_t skipR = hHandLen + 2;
    if (psx >= 0 && (psx != nsx || psy != nsy)) {
      draw_line_aa_skip(cx, cy, psx, psy, BACKGROUND, skipR);
    }
    if (pmx >= 0 && (pmx != nmx || pmy != nmy)) {
      draw_line_aa_skip(cx, cy, pmx, pmy, BACKGROUND, skipR);
    }
    if (phx >= 0 && (phx != nhx || phy != nhy)) {
      draw_line_aa(cx, cy, phx, phy, BACKGROUND);
    }
    // Redraw marks (cheap) and hands on top to repair any damage
    draw_round_clock_mark(
        center - markLen, center,
        center - (markLen * 2 / 3), center,
        center - (markLen / 2), center);
    draw_line_aa(cx, cy, nhx, nhy, HOUR_COLOR);
    draw_line_aa(cx, cy, nmx, nmy, MINUTE_COLOR);
    draw_line_aa(cx, cy, nsx, nsy, SECOND_COLOR);
    gfx->endWrite();
    phx = nhx; phy = nhy;
    pmx = nmx; pmy = nmy;
    psx = nsx; psy = nsy;

    ohx = nhx;
    ohy = nhy;
    omx = nmx;
    omy = nmy;
    osx = nsx;
    osy = nsy;

    delay(1);
  }
}

void draw_round_clock_mark(int16_t innerR1, int16_t outerR1, int16_t innerR2, int16_t outerR2, int16_t innerR3, int16_t outerR3)
{
  float x, y;
  int16_t x0, x1, y0, y1, innerR, outerR;
  uint16_t c;

  for (uint8_t i = 0; i < 60; i++)
  {
    if ((i % 15) == 0)
    {
      innerR = innerR1;
      outerR = outerR1;
      c = MARK_COLOR;
    }
    else if ((i % 5) == 0)
    {
      innerR = innerR2;
      outerR = outerR2;
      c = MARK_COLOR;
    }
    else
    {
      innerR = innerR3;
      outerR = outerR3;
      c = SUBMARK_COLOR;
    }

    mdeg = (SIXTIETH_RADIAN * i) - RIGHT_ANGLE_RADIAN;
    x = cos(mdeg);
    y = sin(mdeg);
    x0 = x * outerR + cx;
    y0 = y * outerR + cy;
    x1 = x * innerR + cx;
    y1 = y * innerR + cy;

    draw_line_aa(x0, y0, x1, y1, c);
  }
}

void draw_square_clock_mark(int16_t innerR1, int16_t outerR1, int16_t innerR2, int16_t outerR2, int16_t innerR3, int16_t outerR3)
{
  float x, y;
  int16_t x0, x1, y0, y1, innerR, outerR;
  uint16_t c;

  for (uint8_t i = 0; i < 60; i++)
  {
    if ((i % 15) == 0)
    {
      innerR = innerR1;
      outerR = outerR1;
      c = MARK_COLOR;
    }
    else if ((i % 5) == 0)
    {
      innerR = innerR2;
      outerR = outerR2;
      c = MARK_COLOR;
    }
    else
    {
      innerR = innerR3;
      outerR = outerR3;
      c = SUBMARK_COLOR;
    }

    if ((i >= 53) || (i < 8))
    {
      x = tan(SIXTIETH_RADIAN * i);
      x0 = center + (x * outerR);
      y0 = center + (1 - outerR);
      x1 = center + (x * innerR);
      y1 = center + (1 - innerR);
    }
    else if (i < 23)
    {
      y = tan((SIXTIETH_RADIAN * i) - RIGHT_ANGLE_RADIAN);
      x0 = center + (outerR);
      y0 = center + (y * outerR);
      x1 = center + (innerR);
      y1 = center + (y * innerR);
    }
    else if (i < 38)
    {
      x = tan(SIXTIETH_RADIAN * i);
      x0 = center - (x * outerR);
      y0 = center + (outerR);
      x1 = center - (x * innerR);
      y1 = center + (innerR);
    }
    else if (i < 53)
    {
      y = tan((SIXTIETH_RADIAN * i) - RIGHT_ANGLE_RADIAN);
      x0 = center + (1 - outerR);
      y0 = center - (y * outerR);
      x1 = center + (1 - innerR);
      y1 = center - (y * innerR);
    }
    gfx->drawLine(x0, y0, x1, y1, c);
  }
}

