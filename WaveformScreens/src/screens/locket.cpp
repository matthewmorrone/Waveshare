#include "core/screen_manager.h"

#ifdef SCREEN_LOCKET

#include "config/pin_config.h"
#include "config/screen_constants.h"
#include "screens/screen_callbacks.h"
#include "drivers/es8311.h"
#include <ESP_I2S.h>
#include <SD_MMC.h>

#include <cmath>
#include <vector>
#include <esp_heap_caps.h>

extern lv_obj_t *screenRoots[];
extern I2SClass  audioI2s;
extern bool      sdMounted;
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
void noteActivity();
void locketHandleOrbTap();

// ---------------------------------------------------------------------------
// WAV playback from SD card
// ---------------------------------------------------------------------------

static constexpr size_t   kWavChunkBytes   = 4096;
static constexpr uint32_t kWavDefaultRate  = 16000;
static constexpr const char *kWavDir       = "/sdcard/assets";

static File              gWavFile;
static bool              gSongPlaying   = false;
static bool              gSongI2sReady  = false;
static es8311_handle_t   gSongCodec     = nullptr;
static uint32_t          gWavSampleRate = kWavDefaultRate;
static uint8_t           gWavBuf[kWavChunkBytes];

// Scan kWavDir for .wav files, pick one at random, return full path.
// Returns empty string if none found or SD not mounted.
static String pickRandomWav()
{
  if (!sdMounted) return "";
  File dir = SD_MMC.open(kWavDir);
  if (!dir || !dir.isDirectory()) return "";

  std::vector<String> names;
  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String name = entry.name();
      name.toLowerCase();
      if (name.endsWith(".wav")) {
        names.push_back(String(kWavDir) + "/" + entry.name());
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();

  if (names.empty()) return "";
  return names[esp_random() % names.size()];
}

// Parse a minimal WAV header to extract sample rate.
// Returns true if the file looks like a valid PCM WAV; seeks past the header.
static bool parseWavHeader(File &f, uint32_t &outSampleRate)
{
  uint8_t hdr[44];
  if (f.read(hdr, 44) != 44) return false;
  // RIFF....WAVEfmt
  if (hdr[0]!='R'||hdr[1]!='I'||hdr[2]!='F'||hdr[3]!='F') return false;
  if (hdr[8]!='W'||hdr[9]!='A'||hdr[10]!='V'||hdr[11]!='E') return false;
  outSampleRate = (uint32_t)hdr[24]
                | ((uint32_t)hdr[25] << 8)
                | ((uint32_t)hdr[26] << 16)
                | ((uint32_t)hdr[27] << 24);
  if (outSampleRate == 0) outSampleRate = kWavDefaultRate;

  // Seek to "data" chunk — scan up to 256 extra bytes for non-standard headers
  // hdr[36..39] might already be "data" for a standard 44-byte header
  if (hdr[36]=='d' && hdr[37]=='a' && hdr[38]=='t' && hdr[39]=='a') return true;

  // Extended header — search forward
  uint8_t buf[4];
  for (int i = 0; i < 256; i++) {
    if (f.read(buf, 1) != 1) return false;
    if (buf[0]=='d') {
      if (f.read(buf+1, 3) != 3) return false;
      if (buf[1]=='a' && buf[2]=='t' && buf[3]=='a') {
        f.read(buf, 4); // skip data chunk size
        return true;
      }
    }
  }
  return false;
}

static bool initSongI2s(uint32_t sampleRate)
{
  audioI2s.end();
  audioI2s.setPins(AUDIO_I2S_BCLK, AUDIO_I2S_WS, AUDIO_I2S_DOUT, AUDIO_I2S_DIN, AUDIO_I2S_MCLK);
  audioI2s.setTimeout(100);
  return audioI2s.begin(I2S_MODE_STD, sampleRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
}

static bool initSongCodec(uint32_t sampleRate)
{
  if (gSongCodec) {
    es8311_delete(gSongCodec);
    gSongCodec = nullptr;
  }
  gSongCodec = es8311_create(0, ES8311_ADDRRES_0);
  if (!gSongCodec) return false;
  const es8311_clock_config_t clk = {
      .mclk_inverted      = false,
      .sclk_inverted      = false,
      .mclk_from_mclk_pin = true,
      .mclk_frequency     = (int)(sampleRate * 256U),
      .sample_frequency   = (int)sampleRate,
  };
  esp_err_t err = es8311_init(gSongCodec, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
  if (err == ESP_OK) err = es8311_sample_frequency_config(gSongCodec, clk.mclk_frequency, clk.sample_frequency);
  if (err == ESP_OK) err = es8311_microphone_config(gSongCodec, false);
  if (err == ESP_OK) err = es8311_voice_volume_set(gSongCodec, 70, nullptr);
  if (err == ESP_OK) err = es8311_voice_mute(gSongCodec, false);
  if (err != ESP_OK) {
    es8311_delete(gSongCodec);
    gSongCodec = nullptr;
    return false;
  }
  return true;
}

static void teardownSong()
{
  gSongPlaying  = false;
  gSongI2sReady = false;
  if (gWavFile) gWavFile.close();
  audioI2s.end();
  if (gSongCodec) {
    es8311_delete(gSongCodec);
    gSongCodec = nullptr;
  }
}

static void tickSong(uint32_t /*nowMs*/)
{
  if (!gSongPlaying || !gSongI2sReady) return;
  if (!gWavFile) { teardownSong(); return; }

  int bytesRead = gWavFile.read(gWavBuf, kWavChunkBytes);
  if (bytesRead <= 0) {
    teardownSong();
    return;
  }
  audioI2s.write(gWavBuf, (size_t)bytesRead);
}

namespace
{
constexpr uint32_t kFrameIntervalMs = 40;
constexpr int kMoonRadius = 78;
constexpr int kMoonDiameter = kMoonRadius * 2;
constexpr float kCloudLoopMs = 22000.0f;
constexpr float kMoonSpinMs = 9000.0f;

struct StarPoint
{
  int16_t x;
  int16_t y;
  uint8_t radius;
  uint8_t twinkle;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

struct CloudCircleSpec
{
  int16_t dx;
  int16_t dy;
  int16_t diameter;
  lv_opa_t opa;
};

struct CloudLane
{
  float y;
  float scale;
  float speed;
  float start;
};

struct LocketUi
{
  lv_obj_t *screen = nullptr;
  lv_obj_t *moonCanvas = nullptr;
  lv_obj_t *statusLabel = nullptr;
};

constexpr StarPoint kStars[] = {
    {24, 44, 1, 18, 255, 255, 255},
    {56, 92, 2, 26, 220, 238, 255},
    {98, 58, 1, 14, 255, 250, 236},
    {142, 128, 1, 22, 210, 232, 255},
    {184, 72, 2, 28, 255, 255, 255},
    {218, 108, 1, 16, 214, 232, 255},
    {254, 52, 1, 20, 255, 247, 232},
    {302, 94, 2, 24, 230, 242, 255},
    {332, 48, 1, 18, 255, 255, 255},
    {44, 168, 1, 14, 205, 228, 255},
    {86, 206, 2, 30, 255, 255, 255},
    {146, 182, 1, 18, 220, 236, 255},
    {212, 198, 1, 22, 255, 250, 238},
    {278, 156, 2, 26, 230, 242, 255},
    {326, 188, 1, 18, 255, 255, 255},
};

constexpr CloudCircleSpec kCloudShape[] = {
    {-54, 18, 56, 115},
    {-18, 0, 74, 140},
    {34, 12, 62, 122},
    {68, 26, 44, 92},
};

constexpr CloudLane kClouds[] = {
    {292.0f, 1.15f, 1.0f, 0.00f},
    {320.0f, 0.94f, 1.3f, 0.28f},
    {346.0f, 1.28f, 0.8f, 0.56f},
    {370.0f, 1.05f, 1.1f, 0.78f},
};

const ScreenModule kModule = {
    ScreenId::Locket,
    "Locket",
    waveformBuildLocketScreen,
    waveformRefreshLocketScreen,
    waveformEnterLocketScreen,
    waveformLeaveLocketScreen,
    waveformTickLocketScreen,
    waveformLocketScreenRoot,
    waveformDestroyLocketScreen,
};

LocketUi gUi;
lv_draw_buf_t *gMoonBuf = nullptr;
bool gBuilt = false;
uint32_t gLastFrameAtMs = 0;
lv_obj_t *gCloudCircles[sizeof(kClouds) / sizeof(kClouds[0])][sizeof(kCloudShape) / sizeof(kCloudShape[0])] = {};

uint16_t blend565(uint16_t a, uint16_t b, uint8_t amount)
{
  const uint8_t inv = 255U - amount;
  const uint8_t ar = static_cast<uint8_t>((a >> 11) & 0x1F);
  const uint8_t ag = static_cast<uint8_t>((a >> 5) & 0x3F);
  const uint8_t ab = static_cast<uint8_t>(a & 0x1F);
  const uint8_t br = static_cast<uint8_t>((b >> 11) & 0x1F);
  const uint8_t bg = static_cast<uint8_t>((b >> 5) & 0x3F);
  const uint8_t bb = static_cast<uint8_t>(b & 0x1F);

  const uint8_t rr = static_cast<uint8_t>((ar * inv + br * amount) / 255U);
  const uint8_t rg = static_cast<uint8_t>((ag * inv + bg * amount) / 255U);
  const uint8_t rb = static_cast<uint8_t>((ab * inv + bb * amount) / 255U);
  return static_cast<uint16_t>((rr << 11) | (rg << 5) | rb);
}

void destroyLocketScreen()
{
  teardownSong();
  if (gUi.screen) {
    lv_obj_delete(gUi.screen);
  }
  gUi = {};
  if (gMoonBuf) {
    lv_draw_buf_destroy(gMoonBuf);
    gMoonBuf = nullptr;
  }
  screenRoots[static_cast<size_t>(ScreenId::Locket)] = nullptr;
  memset(gCloudCircles, 0, sizeof(gCloudCircles));
  gBuilt = false;
  gLastFrameAtMs = 0;
}

void renderMoon(float phaseAngle)
{
  if (!gMoonBuf) {
    return;
  }

  uint16_t *pixels = reinterpret_cast<uint16_t *>(gMoonBuf->data);
  const uint32_t stride = gMoonBuf->header.stride / 2U;
  const float radius = static_cast<float>(kMoonRadius);
  const float moonR = radius * 0.95f;
  const float cutR = moonR * 0.76f;
  const float cutOffset = moonR * 0.46f;
  const float cutX = cosf(phaseAngle) * cutOffset;
  const float cutY = sinf(phaseAngle) * cutOffset;

  for (int py = 0; py < kMoonDiameter; ++py) {
    for (int px = 0; px < kMoonDiameter; ++px) {
      const float dx = (static_cast<float>(px) + 0.5f) - radius;
      const float dy = (static_cast<float>(py) + 0.5f) - radius;
      const float outer = sqrtf((dx * dx) + (dy * dy));
      if (outer > moonR) {
        pixels[py * stride + px] = 0;
        continue;
      }

      const float cdx = dx - cutX;
      const float cdy = dy - cutY;
      const float inner = sqrtf((cdx * cdx) + (cdy * cdy));
      if (inner < cutR) {
        pixels[py * stride + px] = 0;
        continue;
      }

      const float t = outer / moonR;
      const uint16_t base = ((31U << 11) | (61U << 5) | 28U);
      const uint16_t warm = ((30U << 11) | (60U << 5) | 20U);
      pixels[py * stride + px] = blend565(base, warm, static_cast<uint8_t>(t * 150.0f));
    }
  }

  lv_obj_invalidate(gUi.moonCanvas);
}

void updateClouds(uint32_t nowMs)
{
  constexpr float travel = static_cast<float>(LCD_WIDTH + 240);
  for (size_t laneIndex = 0; laneIndex < sizeof(kClouds) / sizeof(kClouds[0]); ++laneIndex) {
    const CloudLane &lane = kClouds[laneIndex];
    float t = fmodf((static_cast<float>(nowMs) / (kCloudLoopMs / lane.speed)) + lane.start, 1.0f);
    float anchorX = static_cast<float>(LCD_WIDTH + 120) - (t * travel);

    for (size_t circleIndex = 0; circleIndex < sizeof(kCloudShape) / sizeof(kCloudShape[0]); ++circleIndex) {
      lv_obj_t *obj = gCloudCircles[laneIndex][circleIndex];
      if (!obj) {
        continue;
      }

      const CloudCircleSpec &circle = kCloudShape[circleIndex];
      int size = static_cast<int>(circle.diameter * lane.scale);
      int x = static_cast<int>(anchorX + (circle.dx * lane.scale));
      int y = static_cast<int>(lane.y + (circle.dy * lane.scale));
      lv_obj_set_size(obj, size, size);
      lv_obj_set_pos(obj, x, y);
    }
  }
}

void buildLocketScreen()
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);
  lv_obj_set_style_bg_color(screen, lvColor(6, 18, 76), 0);
  lv_obj_set_style_bg_grad_color(screen, lvColor(144, 210, 255), 0);
  lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_add_flag(screen, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(screen, [](lv_event_t *) { locketHandleOrbTap(); }, LV_EVENT_CLICKED, nullptr);

  for (size_t i = 0; i < sizeof(kStars) / sizeof(kStars[0]); ++i) {
    const StarPoint &star = kStars[i];
    lv_obj_t *obj = lv_obj_create(screen);
    lv_obj_set_size(obj, star.radius * 2, star.radius * 2);
    lv_obj_set_pos(obj, star.x, star.y);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(obj, lvColor(star.red, star.green, star.blue), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  }

  gMoonBuf = lv_draw_buf_create(kMoonDiameter, kMoonDiameter, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
  gUi.moonCanvas = lv_canvas_create(screen);
  lv_canvas_set_draw_buf(gUi.moonCanvas, gMoonBuf);
  lv_obj_set_size(gUi.moonCanvas, kMoonDiameter, kMoonDiameter);
  lv_obj_set_style_bg_opa(gUi.moonCanvas, LV_OPA_TRANSP, 0);
  lv_obj_set_style_blend_mode(gUi.moonCanvas, LV_BLEND_MODE_ADDITIVE, 0);
  lv_obj_set_style_transform_pivot_x(gUi.moonCanvas, kMoonRadius, 0);
  lv_obj_set_style_transform_pivot_y(gUi.moonCanvas, kMoonRadius, 0);
  lv_obj_set_pos(gUi.moonCanvas, (LCD_WIDTH - kMoonDiameter) / 2, 118);

  for (size_t laneIndex = 0; laneIndex < sizeof(kClouds) / sizeof(kClouds[0]); ++laneIndex) {
    for (size_t circleIndex = 0; circleIndex < sizeof(kCloudShape) / sizeof(kCloudShape[0]); ++circleIndex) {
      const CloudCircleSpec &circle = kCloudShape[circleIndex];
      lv_obj_t *obj = lv_obj_create(screen);
      lv_obj_set_size(obj, circle.diameter, circle.diameter);
      lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(obj, lvColor(240, 246, 255), 0);
      lv_obj_set_style_bg_opa(obj, circle.opa, 0);
      lv_obj_set_style_border_width(obj, 0, 0);
      lv_obj_set_style_pad_all(obj, 0, 0);
      lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
      gCloudCircles[laneIndex][circleIndex] = obj;
    }
  }

  static const struct GlowRing { int radius; uint8_t red; uint8_t green; uint8_t blue; lv_opa_t opa; } kRings[] = {
      {92, 255, 180, 72, LV_OPA_10},
      {74, 255, 138, 60, 46},
      {58, 255, 116, 54, 72},
  };
  for (const GlowRing &ring : kRings) {
    lv_obj_t *glow = lv_obj_create(screen);
    lv_obj_set_size(glow, ring.radius * 2, ring.radius * 2);
    lv_obj_set_style_radius(glow, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(glow, lvColor(ring.red, ring.green, ring.blue), 0);
    lv_obj_set_style_bg_opa(glow, ring.opa, 0);
    lv_obj_set_style_border_width(glow, 0, 0);
    lv_obj_set_style_pad_all(glow, 0, 0);
    lv_obj_clear_flag(glow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(glow, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(glow, LV_ALIGN_CENTER, 0, 38);
  }

  lv_obj_t *rim = lv_obj_create(screen);
  lv_obj_set_size(rim, 96, 96);
  lv_obj_set_style_radius(rim, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(rim, lvColor(255, 194, 82), 0);
  lv_obj_set_style_bg_opa(rim, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(rim, 0, 0);
  lv_obj_set_style_pad_all(rim, 0, 0);
  lv_obj_clear_flag(rim, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(rim, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(rim, LV_ALIGN_CENTER, 0, 38);

  lv_obj_t *orb = lv_obj_create(screen);
  lv_obj_set_size(orb, 82, 82);
  lv_obj_set_style_radius(orb, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(orb, lvColor(220, 56, 72), 0);
  lv_obj_set_style_bg_grad_color(orb, lvColor(146, 28, 38), 0);
  lv_obj_set_style_bg_grad_dir(orb, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(orb, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(orb, 0, 0);
  lv_obj_set_style_pad_all(orb, 0, 0);
  lv_obj_clear_flag(orb, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(orb, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(orb, LV_ALIGN_CENTER, 0, 38);

  lv_obj_t *highlight = lv_obj_create(screen);
  lv_obj_set_size(highlight, 28, 28);
  lv_obj_set_style_radius(highlight, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(highlight, lvColor(255, 238, 214), 0);
  lv_obj_set_style_bg_opa(highlight, LV_OPA_70, 0);
  lv_obj_set_style_border_width(highlight, 0, 0);
  lv_obj_set_style_pad_all(highlight, 0, 0);
  lv_obj_clear_flag(highlight, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(highlight, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(highlight, LV_ALIGN_CENTER, -12, 24);

  lv_obj_t *sparkle = lv_obj_create(screen);
  lv_obj_set_size(sparkle, 10, 10);
  lv_obj_set_style_radius(sparkle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(sparkle, lvColor(255, 250, 236), 0);
  lv_obj_set_style_bg_opa(sparkle, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(sparkle, 0, 0);
  lv_obj_set_style_pad_all(sparkle, 0, 0);
  lv_obj_clear_flag(sparkle, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(sparkle, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(sparkle, LV_ALIGN_CENTER, -22, 16);

  lv_obj_t *eyebrow = lv_label_create(screen);
  lv_obj_set_style_text_font(eyebrow, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(eyebrow, lvColor(236, 244, 255), 0);
  lv_label_set_text(eyebrow, "LOCKET");
  lv_obj_align(eyebrow, LV_ALIGN_TOP_LEFT, 20, 18);

  gUi.statusLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(gUi.statusLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(gUi.statusLabel, lvColor(225, 240, 255), 0);
  lv_label_set_text(gUi.statusLabel, "Sky charm");
  lv_obj_align(gUi.statusLabel, LV_ALIGN_TOP_LEFT, 20, 40);

  gUi.screen = screen;
  screenRoots[static_cast<size_t>(ScreenId::Locket)] = screen;
  gBuilt = true;

  renderMoon(-1.15f);
  updateClouds(0);
}
} // namespace

const ScreenModule &locketScreenModule()
{
  return kModule;
}

lv_obj_t *waveformLocketScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Locket)];
}

bool waveformBuildLocketScreen()
{
  if (!waveformLocketScreenRoot()) {
    buildLocketScreen();
  }
  return gBuilt && waveformLocketScreenRoot() && gUi.screen && gUi.moonCanvas && gMoonBuf;
}

bool waveformRefreshLocketScreen()
{
  return gBuilt && waveformLocketScreenRoot() && gUi.moonCanvas && gMoonBuf;
}

void waveformEnterLocketScreen()
{
  gLastFrameAtMs = 0;
}

void waveformLeaveLocketScreen()
{
  teardownSong();
}

void waveformTickLocketScreen(uint32_t nowMs)
{
  tickSong(nowMs);

  if (!gBuilt || !gUi.moonCanvas || !gMoonBuf) {
    return;
  }
  if (gLastFrameAtMs != 0 && (nowMs - gLastFrameAtMs) < kFrameIntervalMs) {
    return;
  }
  gLastFrameAtMs = nowMs;

  const float moonPhase = fmodf(static_cast<float>(nowMs) / kMoonSpinMs, 1.0f) * 2.0f * static_cast<float>(M_PI);
  renderMoon(-moonPhase);
  updateClouds(nowMs);

  for (size_t i = 0; i < sizeof(kStars) / sizeof(kStars[0]); ++i) {
    lv_obj_t *star = lv_obj_get_child(gUi.screen, static_cast<int32_t>(i));
    if (!star) {
      continue;
    }
    lv_obj_set_style_bg_opa(star,
                            static_cast<lv_opa_t>(160 + ((static_cast<int>((nowMs / 70U) + kStars[i].twinkle) % 60) * 1)),
                            0);
  }
}

void locketHandleOrbTap()
{
  teardownSong();

  String path = pickRandomWav();
  if (path.isEmpty()) {
    Serial.printf("Locket: no WAV files found in %s\n", kWavDir);
    return;
  }

  gWavFile = SD_MMC.open(path.c_str(), FILE_READ);
  if (!gWavFile) {
    Serial.printf("Locket: failed to open %s\n", path.c_str());
    return;
  }

  gWavSampleRate = kWavDefaultRate;
  if (!parseWavHeader(gWavFile, gWavSampleRate)) {
    // Not a standard WAV — rewind and play raw PCM at default rate
    gWavFile.seek(0);
    gWavSampleRate = kWavDefaultRate;
  }

  Serial.printf("Locket: playing %s @ %lu Hz\n", path.c_str(), (unsigned long)gWavSampleRate);

  gSongI2sReady = initSongI2s(gWavSampleRate) && initSongCodec(gWavSampleRate);
  gSongPlaying  = gSongI2sReady;
  if (!gSongPlaying && gWavFile) gWavFile.close();
  noteActivity();
}

void waveformDestroyLocketScreen()
{
  destroyLocketScreen();
}

#endif // SCREEN_LOCKET
