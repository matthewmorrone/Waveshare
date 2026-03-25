#include "config/ota_config.h"
#include "config/screen_constants.h"
#include "core/screen_manager.h"
#include "modules/weather_module.h"
#include "screens/screen_callbacks.h"
#include "screens/weather_ui.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <math.h>
#include <time.h>

void showNextScreen();
void showPreviousScreen();
void noteActivity();
bool hasValidTime();
void waveformDestroyWeatherScreen();

namespace
{
const ScreenModule kModule = {
    ScreenId::Weather,
    "Weather",
    waveformBuildWeatherScreen,
    waveformRefreshWeatherScreen,
    waveformEnterWeatherScreen,
    waveformLeaveWeatherScreen,
    waveformTickWeatherScreen,
    waveformWeatherScreenRoot,
    waveformDestroyWeatherScreen,
};

constexpr uint16_t kBackgroundColor = 0x0000;
constexpr double kSynodicMonthDays = 29.530588853;
constexpr time_t kReferenceNewMoonUnix = 947182440;
constexpr const char *kDefaultUnavailableCondition = "Weather unavailable";
constexpr const char *kDefaultOfflineWeatherLabel = "Offline - cached weather";

WeatherModuleConfig gConfig = {};
WeatherState gState;
WeatherUi gUi;
lv_obj_t *gRoot = nullptr;
bool gFetchInProgress = false;
bool gDebugOverrideEnabled = false;
WeatherSceneType gDebugScene = WeatherSceneType::Clear;
uint32_t gNextRefreshAtMs = 0;

lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b)
{
  return lv_color_make(r, g, b);
}

bool networkOnline()
{
  return gConfig.networkIsOnline && gConfig.networkIsOnline();
}

bool otaBusy()
{
  return gConfig.otaInProgress && gConfig.otaInProgress();
}

bool lightSleepActive()
{
  return gConfig.lightSleepActive && gConfig.lightSleepActive();
}

String apiUrl()
{
  return gConfig.apiUrl ? gConfig.apiUrl() : String();
}

String updatedLabel()
{
  return gConfig.updatedLabel ? gConfig.updatedLabel() : String("Updated just now");
}

void applyRootStyle(lv_obj_t *obj)
{
  lv_obj_set_style_bg_color(obj, lvColor(0, 0, 0), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_outline_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *createCloud(lv_obj_t *parent, lv_coord_t width, lv_coord_t height)
{
  lv_obj_t *cloud = lv_obj_create(parent);
  lv_obj_set_size(cloud, width, height);
  lv_obj_set_style_radius(cloud, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(cloud, lvColor(228, 236, 246), 0);
  lv_obj_set_style_bg_opa(cloud, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(cloud, 0, 0);
  return cloud;
}

static void setLinePoints(lv_point_precise_t points[2], float x1, float y1, float x2, float y2)
{
  points[0].x = x1;
  points[0].y = y1;
  points[1].x = x2;
  points[1].y = y2;
}

lv_color_t interpolateColor(lv_color_t a, lv_color_t b, float t)
{
  t = fminf(fmaxf(t, 0.0f), 1.0f);
  uint8_t r = static_cast<uint8_t>(a.red + ((b.red - a.red) * t));
  uint8_t g = static_cast<uint8_t>(a.green + ((b.green - a.green) * t));
  uint8_t bl = static_cast<uint8_t>(a.blue + ((b.blue - a.blue) * t));
  return lvColor(r, g, bl);
}

lv_color_t temperatureColor(int tempF)
{
  if (tempF <= 32) {
    return lvColor(86, 154, 255);
  }
  if (tempF <= 60) {
    return interpolateColor(lvColor(86, 154, 255), lvColor(255, 216, 92), (tempF - 32) / 28.0f);
  }
  if (tempF <= 90) {
    return interpolateColor(lvColor(255, 216, 92), lvColor(255, 112, 92), (tempF - 60) / 30.0f);
  }
  return lvColor(255, 86, 86);
}

int countValidHourly(const WeatherState &state)
{
  int count = 0;
  for (const auto &entry : state.hourly) {
    if (entry.valid) {
      ++count;
    }
  }
  return count;
}

int countValidDaily(const WeatherState &state)
{
  int count = 0;
  for (const auto &entry : state.daily) {
    if (entry.valid) {
      ++count;
    }
  }
  return count;
}

void hourlyTemperatureBounds(const WeatherState &state, int &minTemp, int &maxTemp)
{
  minTemp = 999;
  maxTemp = -999;
  for (const auto &entry : state.hourly) {
    if (!entry.valid) {
      continue;
    }
    minTemp = min(minTemp, entry.temperatureF);
    maxTemp = max(maxTemp, entry.temperatureF);
  }
  if (minTemp > maxTemp) {
    minTemp = 30;
    maxTemp = 80;
  }
  if (minTemp == maxTemp) {
    maxTemp = minTemp + 1;
  }
}

void dailyTemperatureBounds(const WeatherState &state, int &minTemp, int &maxTemp)
{
  minTemp = 999;
  maxTemp = -999;
  for (const auto &entry : state.daily) {
    if (!entry.valid) {
      continue;
    }
    minTemp = min(minTemp, entry.lowF);
    maxTemp = max(maxTemp, entry.highF);
  }
  if (minTemp > maxTemp) {
    minTemp = 30;
    maxTemp = 80;
  }
  if (minTemp == maxTemp) {
    maxTemp = minTemp + 1;
  }
}

float currentMoonPhaseFraction()
{
  time_t now = time(nullptr);
  if (now < 946684800) {
    return 0.5f;
  }

  double daysSinceReference = difftime(now, kReferenceNewMoonUnix) / 86400.0;
  double lunation = fmod(daysSinceReference / kSynodicMonthDays, 1.0);
  if (lunation < 0.0) {
    lunation += 1.0;
  }
  return static_cast<float>(lunation);
}

void updateMoonPhaseVisual(WeatherUi &ui, lv_color_t skyColor)
{
  if (!ui.moon || !ui.moonBuf) return;

  constexpr int D = 66;
  constexpr float R = D * 0.5f;
  constexpr int moonX = 42;
  constexpr int moonY = 32;

  float phase = currentMoonPhaseFraction();  // 0=new, 0.5=full, 1=new
  // terminator x-coordinate in normalised disc space: -R..+R
  // positive = right side lit (waxing), negative = left side lit (waning)
  float termX = R * cosf(phase * 2.0f * static_cast<float>(M_PI));
  bool waxing = phase < 0.5f;

  lv_draw_buf_t *buf = ui.moonBuf;
  uint32_t *pixels = reinterpret_cast<uint32_t *>(buf->data);
  uint32_t stride = buf->header.stride / 4;  // stride in pixels (ARGB8888 = 4 bytes)

  for (int py = 0; py < D; ++py) {
    float ny = (py + 0.5f) - R;
    for (int px = 0; px < D; ++px) {
      float nx = (px + 0.5f) - R;
      float dist = sqrtf(nx * nx + ny * ny);
      if (dist >= R) {
        pixels[py * stride + px] = 0x00000000;  // fully transparent outside disc
        continue;
      }

      // Base brightness: limb darkening
      float base = 0.78f * (1.0f - (dist / R) * (dist / R) * 0.25f);

      float shadow;
      float soften = R * 0.12f;
      if (waxing) {
        float edge = nx - termX;
        shadow = edge < -soften ? 1.0f : edge > soften ? 0.0f
               : 0.5f - edge / (2.0f * soften);
      } else {
        float edge = termX - nx;
        shadow = edge < -soften ? 1.0f : edge > soften ? 0.0f
               : 0.5f - edge / (2.0f * soften);
      }

      float bright = base * (1.0f - shadow) + base * shadow * 0.06f;

      uint8_t r = static_cast<uint8_t>(fminf(255.0f, bright * 210.0f));
      uint8_t g = static_cast<uint8_t>(fminf(255.0f, bright * 220.0f));
      uint8_t b = static_cast<uint8_t>(fminf(255.0f, bright * 255.0f));
      // ARGB8888 in memory (little-endian): B G R A
      pixels[py * stride + px] = (0xFFu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
  }

  lv_obj_invalidate(ui.moon);
  lv_obj_set_pos(ui.moon, moonX, moonY);
}
} // namespace

String weatherTimeFragment(const char *isoText)
{
  if (isoText == nullptr || isoText[0] == '\0') {
    return "--:--";
  }

  const char *timeFragment = strrchr(isoText, 'T');
  if (timeFragment == nullptr) {
    return String(isoText);
  }

  ++timeFragment;
  char buffer[6] = {'-', '-', ':', '-', '-', '\0'};
  strncpy(buffer, timeFragment, 5);
  buffer[5] = '\0';
  return String(buffer);
}

String weatherHourFragment(const char *isoText)
{
  String timeValue = weatherTimeFragment(isoText);
  if (timeValue.length() < 2) {
    return "--";
  }
  return timeValue.substring(0, 2);
}

String weatherDayFragment(const char *isoText)
{
  if (isoText == nullptr || isoText[0] == '\0') {
    return "---";
  }

  int year = 0;
  int month = 0;
  int day = 0;
  if (sscanf(isoText, "%d-%d-%d", &year, &month, &day) != 3) {
    return "---";
  }

  struct tm tmValue = {};
  tmValue.tm_year = year - 1900;
  tmValue.tm_mon = month - 1;
  tmValue.tm_mday = day;
  mktime(&tmValue);

  char buffer[8];
  strftime(buffer, sizeof(buffer), "%a", &tmValue);
  return String(buffer);
}

WeatherSceneType weatherSceneTypeForCode(int weatherCode)
{
  switch (weatherCode) {
    case 0:
      return WeatherSceneType::Clear;
    case 1:
    case 2:
      return WeatherSceneType::PartlyCloudy;
    case 3:
      return WeatherSceneType::Cloudy;
    case 45:
    case 48:
      return WeatherSceneType::Fog;
    case 51:
    case 53:
    case 55:
    case 56:
    case 57:
    case 61:
    case 63:
    case 65:
    case 66:
    case 67:
    case 80:
    case 81:
    case 82:
      return WeatherSceneType::Rain;
    case 71:
    case 73:
    case 75:
    case 77:
    case 85:
    case 86:
      return WeatherSceneType::Snow;
    case 95:
    case 96:
    case 99:
      return WeatherSceneType::Storm;
    default:
      return WeatherSceneType::Cloudy;
  }
}

const char *weatherSceneName(WeatherSceneType scene)
{
  switch (scene) {
    case WeatherSceneType::Clear:
      return "Clear";
    case WeatherSceneType::PartlyCloudy:
      return "Partly cloudy";
    case WeatherSceneType::Cloudy:
      return "Cloudy";
    case WeatherSceneType::Rain:
      return "Rain";
    case WeatherSceneType::Snow:
      return "Snow";
    case WeatherSceneType::Storm:
      return "Storm";
    case WeatherSceneType::Fog:
      return "Fog";
    default:
      return "Weather";
  }
}

String weatherConditionFromCode(int weatherCode, bool isDay)
{
  switch (weatherCode) {
    case 0:
      return isDay ? "Clear sky" : "Clear night";
    case 1:
      return isDay ? "Mostly sunny" : "Mostly clear";
    case 2:
      return "Partly cloudy";
    case 3:
      return "Overcast";
    case 45:
    case 48:
      return "Fog";
    case 51:
    case 53:
    case 55:
      return "Drizzle";
    case 56:
    case 57:
      return "Freezing drizzle";
    case 61:
    case 63:
    case 65:
      return "Rain";
    case 66:
    case 67:
      return "Freezing rain";
    case 71:
    case 73:
    case 75:
      return "Snow";
    case 77:
      return "Snow grains";
    case 80:
    case 81:
    case 82:
      return "Rain showers";
    case 85:
    case 86:
      return "Snow showers";
    case 95:
      return "Thunderstorm";
    case 96:
    case 99:
      return "Thunder + hail";
    default:
      return "Weather";
  }
}

lv_obj_t *buildCurrentWeatherScreen(WeatherUi &ui)
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);
  ui.currentScreen = screen;

  ui.hero = lv_obj_create(screen);
  lv_obj_set_size(ui.hero, LCD_WIDTH, 224);
  lv_obj_align(ui.hero, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_border_width(ui.hero, 0, 0);
  lv_obj_set_style_pad_all(ui.hero, 0, 0);
  lv_obj_set_style_radius(ui.hero, 0, 0);
  lv_obj_set_style_bg_color(ui.hero, lvColor(24, 34, 52), 0);
  lv_obj_set_style_bg_opa(ui.hero, LV_OPA_COVER, 0);

  for (size_t i = 0; i < kWeatherSunRayCount; ++i) {
    ui.sunRays[i] = lv_line_create(ui.hero);
    lv_obj_set_style_line_width(ui.sunRays[i], 3, 0);
    lv_obj_set_style_line_rounded(ui.sunRays[i], true, 0);
    lv_obj_set_style_line_color(ui.sunRays[i], lvColor(255, 214, 104), 0);
    lv_obj_set_style_line_opa(ui.sunRays[i], LV_OPA_70, 0);
    lv_line_set_points(ui.sunRays[i], ui.sunRayPoints[i], 2);
  }

  ui.sun = lv_obj_create(ui.hero);
  lv_obj_set_size(ui.sun, 74, 74);
  lv_obj_set_style_radius(ui.sun, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(ui.sun, lvColor(255, 196, 72), 0);
  lv_obj_set_style_border_width(ui.sun, 0, 0);

  constexpr int kWeatherMoonD = 66;
  ui.moonBuf = lv_draw_buf_create(kWeatherMoonD, kWeatherMoonD, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
  ui.moon = lv_canvas_create(ui.hero);
  lv_canvas_set_draw_buf(ui.moon, ui.moonBuf);
  lv_obj_set_size(ui.moon, kWeatherMoonD, kWeatherMoonD);
  lv_obj_set_style_border_width(ui.moon, 0, 0);
  lv_obj_set_style_pad_all(ui.moon, 0, 0);
  lv_obj_set_style_bg_opa(ui.moon, LV_OPA_TRANSP, 0);
  ui.moonShadow = nullptr;

  ui.clouds[0] = createCloud(ui.hero, 126, 44);
  ui.clouds[1] = createCloud(ui.hero, 106, 38);
  ui.clouds[2] = createCloud(ui.hero, 144, 48);

  for (size_t i = 0; i < kWeatherParticleCount; ++i) {
    ui.particles[i] = lv_obj_create(ui.hero);
    lv_obj_set_size(ui.particles[i], 4, 18);
    lv_obj_set_style_radius(ui.particles[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ui.particles[i], 0, 0);
    lv_obj_set_style_bg_color(ui.particles[i], lvColor(144, 194, 255), 0);
  }

  ui.bolt = lv_label_create(ui.hero);
  lv_obj_set_style_text_font(ui.bolt, &lv_font_montserrat_48, 0);
  lv_label_set_text(ui.bolt, LV_SYMBOL_CHARGE);
  lv_obj_set_style_text_color(ui.bolt, lvColor(255, 220, 84), 0);

  for (int i = 0; i < 3; ++i) {
    ui.fogBars[i] = lv_obj_create(ui.hero);
    lv_obj_set_size(ui.fogBars[i], 220, 8);
    lv_obj_set_style_radius(ui.fogBars[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ui.fogBars[i], lvColor(214, 222, 232), 0);
    lv_obj_set_style_bg_opa(ui.fogBars[i], LV_OPA_70, 0);
    lv_obj_set_style_border_width(ui.fogBars[i], 0, 0);
  }

  ui.cityLabel = lv_label_create(screen);
  lv_obj_set_width(ui.cityLabel, LCD_WIDTH - 48);
  lv_obj_set_style_text_font(ui.cityLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ui.cityLabel, lvColor(134, 146, 162), 0);
  lv_obj_set_style_text_align(ui.cityLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(ui.cityLabel, "Weather");
  lv_obj_align(ui.cityLabel, LV_ALIGN_TOP_MID, 0, 228);

  ui.tempLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(ui.tempLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(ui.tempLabel, lvColor(208, 214, 226), 0);
  lv_label_set_text(ui.tempLabel, "--");
  lv_obj_align(ui.tempLabel, LV_ALIGN_TOP_LEFT, 24, 248);

  ui.tempMeta = lv_obj_create(screen);
  lv_obj_set_size(ui.tempMeta, 176, 110);
  lv_obj_align_to(ui.tempMeta, ui.tempLabel, LV_ALIGN_OUT_RIGHT_TOP, 54, 6);
  lv_obj_set_style_bg_opa(ui.tempMeta, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ui.tempMeta, 0, 0);
  lv_obj_set_style_outline_width(ui.tempMeta, 0, 0);
  lv_obj_set_style_pad_all(ui.tempMeta, 0, 0);
  lv_obj_clear_flag(ui.tempMeta, LV_OBJ_FLAG_SCROLLABLE);

  ui.rangeLine = lv_label_create(ui.tempMeta);
  lv_obj_set_style_text_font(ui.rangeLine, &lv_font_montserrat_16, 0);
  lv_label_set_recolor(ui.rangeLine, true);
  lv_obj_set_style_text_color(ui.rangeLine, lvColor(255, 255, 255), 0);
  lv_label_set_text(ui.rangeLine, "#ff6868 --# / #6cb4ff --# / feels --");
  lv_obj_align(ui.rangeLine, LV_ALIGN_TOP_LEFT, 0, 0);

  ui.sunEventIcon = lv_obj_create(ui.tempMeta);
  lv_obj_set_size(ui.sunEventIcon, 28, 20);
  lv_obj_align(ui.sunEventIcon, LV_ALIGN_TOP_LEFT, 0, 22);
  lv_obj_set_style_bg_opa(ui.sunEventIcon, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ui.sunEventIcon, 0, 0);
  lv_obj_set_style_outline_width(ui.sunEventIcon, 0, 0);
  lv_obj_set_style_pad_all(ui.sunEventIcon, 0, 0);
  lv_obj_clear_flag(ui.sunEventIcon, LV_OBJ_FLAG_SCROLLABLE);

  ui.sunEventHorizon = lv_obj_create(ui.sunEventIcon);
  lv_obj_set_size(ui.sunEventHorizon, 22, 2);
  lv_obj_align(ui.sunEventHorizon, LV_ALIGN_BOTTOM_MID, 0, -4);
  lv_obj_set_style_bg_color(ui.sunEventHorizon, lvColor(208, 214, 226), 0);
  lv_obj_set_style_bg_opa(ui.sunEventHorizon, LV_OPA_80, 0);
  lv_obj_set_style_border_width(ui.sunEventHorizon, 0, 0);
  lv_obj_set_style_radius(ui.sunEventHorizon, LV_RADIUS_CIRCLE, 0);

  ui.sunEventSun = lv_obj_create(ui.sunEventIcon);
  lv_obj_set_size(ui.sunEventSun, 8, 8);
  lv_obj_set_style_radius(ui.sunEventSun, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(ui.sunEventSun, lvColor(255, 196, 72), 0);
  lv_obj_set_style_border_width(ui.sunEventSun, 0, 0);
  lv_obj_align(ui.sunEventSun, LV_ALIGN_BOTTOM_MID, 0, -5);

  ui.sunEventArrow = lv_label_create(ui.sunEventIcon);
  lv_obj_set_style_text_font(ui.sunEventArrow, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(ui.sunEventArrow, lvColor(255, 214, 104), 0);
  lv_label_set_text(ui.sunEventArrow, LV_SYMBOL_UP);
  lv_obj_align(ui.sunEventArrow, LV_ALIGN_TOP_RIGHT, 0, -1);

  ui.sunEventTimeLabel = lv_label_create(ui.tempMeta);
  lv_obj_set_width(ui.sunEventTimeLabel, 132);
  lv_obj_set_style_text_font(ui.sunEventTimeLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(ui.sunEventTimeLabel, lvColor(208, 214, 226), 0);
  lv_label_set_text(ui.sunEventTimeLabel, "--:--");
  lv_obj_align_to(ui.sunEventTimeLabel, ui.sunEventIcon, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

  ui.conditionLabel = lv_label_create(screen);
  lv_obj_set_width(ui.conditionLabel, LCD_WIDTH - 48);
  lv_obj_set_style_text_font(ui.conditionLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(ui.conditionLabel, lvColor(208, 214, 226), 0);
  lv_label_set_long_mode(ui.conditionLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(ui.conditionLabel, "Waiting for weather");
  lv_obj_align(ui.conditionLabel, LV_ALIGN_TOP_LEFT, 24, 324);

  ui.primaryLabel = lv_label_create(screen);
  lv_obj_set_width(ui.primaryLabel, LCD_WIDTH - 48);
  lv_obj_set_style_text_font(ui.primaryLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(ui.primaryLabel, lvColor(208, 214, 226), 0);
  lv_label_set_long_mode(ui.primaryLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(ui.primaryLabel, "");
  lv_obj_align(ui.primaryLabel, LV_ALIGN_TOP_LEFT, 24, 354);

  ui.updatedValueLabel = lv_label_create(screen);
  lv_obj_set_width(ui.updatedValueLabel, LCD_WIDTH - 48);
  lv_obj_set_style_text_font(ui.updatedValueLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ui.updatedValueLabel, lvColor(116, 126, 140), 0);
  lv_label_set_long_mode(ui.updatedValueLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(ui.updatedValueLabel, "Waiting for Wi-Fi");
  lv_obj_align(ui.updatedValueLabel, LV_ALIGN_BOTTOM_LEFT, 24, -42);

  ui.flashOverlay = lv_obj_create(ui.hero);
  lv_obj_set_size(ui.flashOverlay, LCD_WIDTH, 224);
  lv_obj_set_style_bg_color(ui.flashOverlay, lvColor(255, 255, 255), 0);
  lv_obj_set_style_bg_opa(ui.flashOverlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ui.flashOverlay, 0, 0);
  lv_obj_set_style_outline_width(ui.flashOverlay, 0, 0);
  lv_obj_set_style_pad_all(ui.flashOverlay, 0, 0);
  lv_obj_clear_flag(ui.flashOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(ui.flashOverlay, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_add_flag(ui.flashOverlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(ui.flashOverlay);

  return screen;
}

lv_obj_t *buildHourlyForecastScreen(WeatherUi &ui)
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);
  ui.hourlyScreen = screen;

  const int startY = 22;
  const int rowHeight = 30;
  const int leftX = 18;
  const int colWidth = 176;

  for (size_t i = 0; i < kHourlyForecastCount; ++i) {
    int col = static_cast<int>(i / 12);
    int row = static_cast<int>(i % 12);
    int x = leftX + (col * colWidth);
    int y = startY + (row * rowHeight);

    ui.hourlyHourLabels[i] = lv_label_create(screen);
    lv_obj_set_width(ui.hourlyHourLabels[i], 28);
    lv_obj_set_style_text_font(ui.hourlyHourLabels[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui.hourlyHourLabels[i], lvColor(196, 204, 216), 0);
    lv_obj_set_style_text_align(ui.hourlyHourLabels[i], LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(ui.hourlyHourLabels[i], "--");
    lv_obj_set_pos(ui.hourlyHourLabels[i], x, y + 3);

    ui.hourlyTracks[i] = lv_obj_create(screen);
    lv_obj_set_size(ui.hourlyTracks[i], 120, 10);
    lv_obj_set_pos(ui.hourlyTracks[i], x + 34, y + 8);
    lv_obj_set_style_radius(ui.hourlyTracks[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ui.hourlyTracks[i], lvColor(22, 28, 36), 0);
    lv_obj_set_style_border_width(ui.hourlyTracks[i], 0, 0);
    lv_obj_set_style_pad_all(ui.hourlyTracks[i], 0, 0);

    ui.hourlyFills[i] = lv_obj_create(ui.hourlyTracks[i]);
    lv_obj_set_size(ui.hourlyFills[i], 8, 10);
    lv_obj_align(ui.hourlyFills[i], LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_radius(ui.hourlyFills[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ui.hourlyFills[i], 0, 0);
    lv_obj_set_style_pad_all(ui.hourlyFills[i], 0, 0);
  }

  return screen;
}

lv_obj_t *buildDailyForecastScreen(WeatherUi &ui)
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);
  ui.dailyScreen = screen;

  const int startY = 34;
  const int rowHeight = 38;

  for (size_t i = 0; i < kDailyForecastCount; ++i) {
    int y = startY + (static_cast<int>(i) * rowHeight);

    ui.dailyDayLabels[i] = lv_label_create(screen);
    lv_obj_set_width(ui.dailyDayLabels[i], 44);
    lv_obj_set_style_text_font(ui.dailyDayLabels[i], &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ui.dailyDayLabels[i], lvColor(208, 214, 226), 0);
    lv_obj_set_style_text_align(ui.dailyDayLabels[i], LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(ui.dailyDayLabels[i], "---");
    lv_obj_set_pos(ui.dailyDayLabels[i], 22, y);

    ui.dailyTracks[i] = lv_obj_create(screen);
    lv_obj_set_size(ui.dailyTracks[i], 258, 12);
    lv_obj_set_pos(ui.dailyTracks[i], 88, y + 4);
    lv_obj_set_style_radius(ui.dailyTracks[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ui.dailyTracks[i], lvColor(22, 28, 36), 0);
    lv_obj_set_style_border_width(ui.dailyTracks[i], 0, 0);
    lv_obj_set_style_pad_all(ui.dailyTracks[i], 0, 0);

    ui.dailyFills[i] = lv_obj_create(ui.dailyTracks[i]);
    lv_obj_set_size(ui.dailyFills[i], 40, 12);
    lv_obj_set_pos(ui.dailyFills[i], 0, 0);
    lv_obj_set_style_radius(ui.dailyFills[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ui.dailyFills[i], 0, 0);
    lv_obj_set_style_pad_all(ui.dailyFills[i], 0, 0);
  }

  return screen;
}

void refreshCurrentWeatherScreen(WeatherUi &ui,
                                 const WeatherState &state,
                                 bool networkOnline,
                                 bool debugOverrideEnabled,
                                 WeatherSceneType debugScene)
{
  if (!ui.currentScreen) {
    return;
  }

  WeatherSceneType scene = debugOverrideEnabled
                               ? debugScene
                               : (state.hasData ? weatherSceneTypeForCode(state.weatherCode) : WeatherSceneType::Cloudy);
  const char *sceneLabel = weatherSceneName(scene);

  lv_label_set_text(ui.cityLabel, state.location.c_str());

  if (state.hasData) {
    lv_label_set_text(ui.tempLabel, (String(state.temperatureF) + "\xC2\xB0").c_str());
    lv_label_set_text(ui.rangeLine,
      (String("#ff6868 ") + state.highF + "\xC2\xB0# / #6cb4ff " + state.lowF + "\xC2\xB0# / feels " + state.feelsLikeF + "\xC2\xB0").c_str());
    lv_label_set_text(ui.conditionLabel, debugOverrideEnabled ? sceneLabel : state.condition.c_str());
    lv_label_set_text(ui.primaryLabel,
                      (String(LV_SYMBOL_REFRESH) + " " + String(state.windMph) + " mph   " +
                       LV_SYMBOL_TINT + " " + String(state.precipitationPercent) + "%")
                          .c_str());
    lv_label_set_text(ui.sunEventTimeLabel, (state.isDay ? state.sunset : state.sunrise).c_str());
    lv_label_set_text(ui.sunEventArrow, state.isDay ? LV_SYMBOL_DOWN : LV_SYMBOL_UP);
    lv_obj_align(ui.sunEventSun, LV_ALIGN_BOTTOM_MID, 0, state.isDay ? -8 : -3);
    lv_label_set_text(ui.updatedValueLabel, (state.stale ? "Offline - cached forecast" : state.updated).c_str());
  } else {
    String noDataStatus = state.updated;
    if (networkOnline &&
        (noDataStatus == "Waiting for Wi-Fi" || noDataStatus == "Offline - no cached weather")) {
      noDataStatus = "Connected - no cached weather";
    }
    lv_label_set_text(ui.tempLabel, "--");
    lv_label_set_text(ui.rangeLine, "#ff6868 --# / #6cb4ff --# / feels --");
    lv_label_set_text(ui.conditionLabel, sceneLabel);
    lv_label_set_text(ui.primaryLabel,
                      networkOnline ? (String(LV_SYMBOL_REFRESH) + " -- mph   " + LV_SYMBOL_TINT + " --%").c_str()
                                    : "Weather offline");
    lv_label_set_text(ui.sunEventTimeLabel, networkOnline ? "--:--" : "--:--");
    lv_label_set_text(ui.sunEventArrow, LV_SYMBOL_UP);
    lv_obj_align(ui.sunEventSun, LV_ALIGN_BOTTOM_MID, 0, -3);
    lv_label_set_text(ui.updatedValueLabel, noDataStatus.c_str());
  }
}

void refreshHourlyForecastScreen(WeatherUi &ui, const WeatherState &state)
{
  if (!ui.hourlyScreen) {
    return;
  }

  int minTemp = 0;
  int maxTemp = 0;
  hourlyTemperatureBounds(state, minTemp, maxTemp);
  const float range = static_cast<float>(maxTemp - minTemp);

  for (size_t i = 0; i < kHourlyForecastCount; ++i) {
    const HourlyForecastEntry &entry = state.hourly[i];
    lv_obj_t *fill = ui.hourlyFills[i];
    lv_label_set_text(ui.hourlyHourLabels[i], entry.valid ? entry.hour.c_str() : "--");
    lv_obj_set_style_bg_color(ui.hourlyTracks[i], lvColor(22, 28, 36), 0);

    if (!entry.valid) {
      lv_obj_set_width(fill, 8);
      lv_obj_set_style_bg_color(fill, lvColor(56, 64, 74), 0);
      lv_obj_set_style_bg_grad_color(fill, lvColor(56, 64, 74), 0);
      continue;
    }

    float normalized = (entry.temperatureF - minTemp) / range;
    normalized = fminf(fmaxf(normalized, 0.0f), 1.0f);
    int fillWidth = max(8, static_cast<int>(8 + (normalized * 112.0f)));
    lv_obj_set_width(fill, fillWidth);
    lv_color_t startColor = temperatureColor(minTemp);
    lv_color_t endColor = temperatureColor(entry.temperatureF);
    lv_obj_set_style_bg_color(fill, startColor, 0);
    lv_obj_set_style_bg_grad_color(fill, endColor, 0);
    lv_obj_set_style_bg_grad_dir(fill, LV_GRAD_DIR_HOR, 0);
  }
}

void refreshDailyForecastScreen(WeatherUi &ui, const WeatherState &state)
{
  if (!ui.dailyScreen) {
    return;
  }

  int minTemp = 0;
  int maxTemp = 0;
  dailyTemperatureBounds(state, minTemp, maxTemp);
  const float range = static_cast<float>(maxTemp - minTemp);

  for (size_t i = 0; i < kDailyForecastCount; ++i) {
    const DailyForecastEntry &entry = state.daily[i];
    lv_label_set_text(ui.dailyDayLabels[i], entry.valid ? entry.day.c_str() : "---");

    if (!entry.valid) {
      lv_obj_set_x(ui.dailyFills[i], 0);
      lv_obj_set_width(ui.dailyFills[i], 36);
      lv_obj_set_style_bg_color(ui.dailyFills[i], lvColor(56, 64, 74), 0);
      lv_obj_set_style_bg_grad_color(ui.dailyFills[i], lvColor(56, 64, 74), 0);
      continue;
    }

    float lowNorm = (entry.lowF - minTemp) / range;
    float highNorm = (entry.highF - minTemp) / range;
    lowNorm = fminf(fmaxf(lowNorm, 0.0f), 1.0f);
    highNorm = fminf(fmaxf(highNorm, 0.0f), 1.0f);
    int trackWidth = 258;
    int x = static_cast<int>(lowNorm * trackWidth);
    int width = max(14, static_cast<int>((highNorm - lowNorm) * trackWidth));
    if (x + width > trackWidth) {
      width = trackWidth - x;
    }
    lv_obj_set_x(ui.dailyFills[i], x);
    lv_obj_set_width(ui.dailyFills[i], max(14, width));
    lv_obj_set_style_bg_color(ui.dailyFills[i], temperatureColor(entry.lowF), 0);
    lv_obj_set_style_bg_grad_color(ui.dailyFills[i], temperatureColor(entry.highF), 0);
    lv_obj_set_style_bg_grad_dir(ui.dailyFills[i], LV_GRAD_DIR_HOR, 0);
  }
}

void updateWeatherHero(WeatherUi &ui,
                       const WeatherState &state,
                       bool debugOverrideEnabled,
                       WeatherSceneType debugScene,
                       uint32_t nowMs)
{
  if (!ui.currentScreen) {
    return;
  }

  uint32_t tick = nowMs / 90;
  uint32_t slowPulseTick = nowMs / 22;
  const bool isDayScene = state.hasData ? state.isDay : true;
  WeatherSceneType scene = debugOverrideEnabled
                               ? debugScene
                               : (state.hasData ? weatherSceneTypeForCode(state.weatherCode) : WeatherSceneType::Cloudy);

  lv_color_t topColor = lvColor(24, 34, 52);
  lv_color_t bottomColor = lvColor(6, 10, 18);
  lv_color_t cloudColor = lvColor(228, 236, 246);
  lv_color_t particleColor = lvColor(144, 194, 255);
  lv_color_t fogColor = lvColor(214, 222, 232);
  lv_color_t boltColor = lvColor(255, 220, 84);
  switch (scene) {
    case WeatherSceneType::Clear:
      topColor = isDayScene ? lvColor(60, 110, 210) : lvColor(16, 26, 64);
      bottomColor = isDayScene ? lvColor(148, 194, 255) : lvColor(6, 10, 24);
      break;
    case WeatherSceneType::PartlyCloudy:
      topColor = isDayScene ? lvColor(58, 82, 140) : lvColor(18, 30, 60);
      bottomColor = isDayScene ? lvColor(110, 150, 220) : lvColor(44, 70, 118);
      cloudColor = isDayScene ? lvColor(228, 236, 246) : lvColor(134, 148, 170);
      break;
    case WeatherSceneType::Cloudy:
      topColor = isDayScene ? lvColor(42, 50, 62) : lvColor(20, 24, 36);
      bottomColor = isDayScene ? lvColor(76, 88, 108) : lvColor(46, 54, 72);
      cloudColor = isDayScene ? lvColor(212, 220, 232) : lvColor(118, 128, 148);
      break;
    case WeatherSceneType::Rain:
      topColor = isDayScene ? lvColor(26, 34, 48) : lvColor(10, 16, 28);
      bottomColor = isDayScene ? lvColor(48, 66, 94) : lvColor(24, 36, 58);
      cloudColor = isDayScene ? lvColor(176, 190, 210) : lvColor(98, 110, 132);
      particleColor = isDayScene ? lvColor(144, 194, 255) : lvColor(96, 142, 214);
      break;
    case WeatherSceneType::Snow:
      topColor = isDayScene ? lvColor(82, 110, 156) : lvColor(24, 40, 72);
      bottomColor = isDayScene ? lvColor(192, 214, 244) : lvColor(70, 98, 144);
      cloudColor = isDayScene ? lvColor(234, 240, 248) : lvColor(142, 156, 182);
      particleColor = isDayScene ? lvColor(248, 250, 255) : lvColor(208, 220, 244);
      break;
    case WeatherSceneType::Storm:
      topColor = isDayScene ? lvColor(20, 24, 34) : lvColor(6, 8, 16);
      bottomColor = isDayScene ? lvColor(58, 70, 92) : lvColor(22, 28, 44);
      cloudColor = isDayScene ? lvColor(132, 142, 160) : lvColor(82, 90, 108);
      particleColor = isDayScene ? lvColor(144, 194, 255) : lvColor(94, 128, 182);
      boltColor = isDayScene ? lvColor(255, 220, 84) : lvColor(230, 236, 255);
      break;
    case WeatherSceneType::Fog:
      topColor = isDayScene ? lvColor(72, 82, 96) : lvColor(24, 30, 40);
      bottomColor = isDayScene ? lvColor(144, 154, 166) : lvColor(58, 68, 82);
      fogColor = isDayScene ? lvColor(214, 222, 232) : lvColor(136, 148, 164);
      break;
  }

  lv_obj_set_style_bg_color(ui.hero, topColor, 0);
  lv_obj_set_style_bg_grad_color(ui.hero, bottomColor, 0);
  lv_obj_set_style_bg_grad_dir(ui.hero, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_text_color(ui.bolt, boltColor, 0);
  for (auto *cloud : ui.clouds) {
    lv_obj_set_style_bg_color(cloud, cloudColor, 0);
  }
  for (auto *bar : ui.fogBars) {
    lv_obj_set_style_bg_color(bar, fogColor, 0);
  }

  lv_obj_add_flag(ui.sun, LV_OBJ_FLAG_HIDDEN);
  for (auto *ray : ui.sunRays) {
    lv_obj_add_flag(ray, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_add_flag(ui.moon, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui.bolt, LV_OBJ_FLAG_HIDDEN);
  for (auto *cloud : ui.clouds) {
    lv_obj_add_flag(cloud, LV_OBJ_FLAG_HIDDEN);
  }
  for (auto *particle : ui.particles) {
    lv_obj_add_flag(particle, LV_OBJ_FLAG_HIDDEN);
  }
  for (auto *bar : ui.fogBars) {
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
  }
  if (ui.flashOverlay) {
    lv_obj_add_flag(ui.flashOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(ui.flashOverlay, LV_OPA_TRANSP, 0);
  }

  if (scene == WeatherSceneType::Clear || scene == WeatherSceneType::PartlyCloudy) {
    lv_obj_t *celestial = isDayScene ? ui.sun : ui.moon;
    lv_obj_clear_flag(celestial, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(celestial, 42, 32);
    if (isDayScene) {
      static constexpr int rayVectors[kWeatherSunRayCount][2] = {
          {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1},
      };
      static constexpr uint8_t rayPhaseOrder[kWeatherSunRayCount] = {0, 5, 2, 7, 1, 6, 3, 4};
      const float pulse = 0.5f + (0.5f * sinf(static_cast<float>(slowPulseTick) * 0.18f));
      const float sunRadius = static_cast<float>(lv_obj_get_width(ui.sun)) * 0.5f;
      uint8_t sunOpa = static_cast<uint8_t>(220 + static_cast<int>(20 * sinf(static_cast<float>(tick) * 0.18f)));
      const float sunCenterX = static_cast<float>(lv_obj_get_x(ui.sun) + (lv_obj_get_width(ui.sun) / 2));
      const float sunCenterY = static_cast<float>(lv_obj_get_y(ui.sun) + (lv_obj_get_height(ui.sun) / 2));
      const uint32_t rayCycleMs = 1500;
      const float travelRange = 26.0f + (pulse * 6.0f);
      lv_obj_set_style_bg_opa(ui.sun, sunOpa, 0);
      for (size_t i = 0; i < kWeatherSunRayCount; ++i) {
        lv_obj_t *ray = ui.sunRays[i];
        lv_obj_clear_flag(ray, LV_OBJ_FLAG_HIDDEN);
        const float dx = static_cast<float>(rayVectors[i][0]);
        const float dy = static_cast<float>(rayVectors[i][1]);
        const float invLen = 1.0f / sqrtf((dx * dx) + (dy * dy));
        const float dirX = dx * invLen;
        const float dirY = dy * invLen;
        const uint32_t phaseMs = (nowMs + (rayPhaseOrder[i] * (rayCycleMs / kWeatherSunRayCount))) % rayCycleMs;
        const float progress = static_cast<float>(phaseMs) / static_cast<float>(rayCycleMs);
        const float startRadius = sunRadius + 6.0f + (progress * travelRange);
        const float segmentLength = 14.0f - (progress * 5.0f) + (pulse * 2.0f);
        const float endRadius = startRadius + segmentLength;
        setLinePoints(ui.sunRayPoints[i],
                      sunCenterX + (dirX * startRadius),
                      sunCenterY + (dirY * startRadius),
                      sunCenterX + (dirX * endRadius),
                      sunCenterY + (dirY * endRadius));
        const float fade = 1.0f - progress;
        const uint8_t rayOpa = static_cast<uint8_t>(32 + (176.0f * fade * fade));
        lv_obj_set_style_line_opa(ray, static_cast<lv_opa_t>(rayOpa), 0);
      }
    } else {
      updateMoonPhaseVisual(ui, topColor);
    }
  }

  if (scene == WeatherSceneType::PartlyCloudy || scene == WeatherSceneType::Cloudy ||
      scene == WeatherSceneType::Rain || scene == WeatherSceneType::Snow ||
      scene == WeatherSceneType::Storm || scene == WeatherSceneType::Fog) {
    lv_obj_clear_flag(ui.clouds[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui.clouds[1], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui.clouds[2], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(ui.clouds[0], 168 + static_cast<int>((tick % 12) - 6), 52);
    lv_obj_set_pos(ui.clouds[1], 64 + static_cast<int>((tick % 16) - 8), 96);
    lv_obj_set_pos(ui.clouds[2], 188 + static_cast<int>((tick % 10) - 5), 112);
  }

  if (scene == WeatherSceneType::Rain || scene == WeatherSceneType::Storm) {
    for (size_t i = 0; i < kWeatherParticleCount; ++i) {
      lv_obj_clear_flag(ui.particles[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_color(ui.particles[i], particleColor, 0);
      lv_obj_set_size(ui.particles[i], 4, 18);
      lv_obj_set_pos(ui.particles[i], 42 + static_cast<int>(i * 36), 86 + static_cast<int>((tick * 12 + i * 23) % 110));
    }
  }

  if (scene == WeatherSceneType::Snow) {
    for (size_t i = 0; i < kWeatherParticleCount; ++i) {
      lv_obj_clear_flag(ui.particles[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_color(ui.particles[i], particleColor, 0);
      lv_obj_set_size(ui.particles[i], 8, 8);
      lv_obj_set_style_radius(ui.particles[i], LV_RADIUS_CIRCLE, 0);
      lv_obj_set_pos(ui.particles[i], 30 + static_cast<int>((i * 38 + tick * 5) % 280), 74 + static_cast<int>((tick * 8 + i * 17) % 120));
    }
  }

  if (scene == WeatherSceneType::Storm) {
    const uint32_t stormPhaseMs = nowMs % 2600;
    bool strikeVisible = false;
    int boltX = 236;
    int boltY = 68;
    lv_opa_t boltOpa = LV_OPA_TRANSP;
    lv_opa_t flashOpa = LV_OPA_TRANSP;

    if (stormPhaseMs >= 1500 && stormPhaseMs < 1585) {
      strikeVisible = true;
      boltOpa = LV_OPA_100;
      flashOpa = LV_OPA_50;
    } else if (stormPhaseMs >= 1585 && stormPhaseMs < 1650) {
      strikeVisible = true;
      boltX += 4;
      boltY += 6;
      boltOpa = LV_OPA_90;
      flashOpa = LV_OPA_90;
    } else if (stormPhaseMs >= 1650 && stormPhaseMs < 1730) {
      strikeVisible = true;
      boltOpa = LV_OPA_40;
      flashOpa = LV_OPA_20;
    } else if (stormPhaseMs >= 1870 && stormPhaseMs < 1945) {
      strikeVisible = true;
      boltX -= 6;
      boltY += 2;
      boltOpa = LV_OPA_100;
      flashOpa = LV_OPA_70;
    } else if (stormPhaseMs >= 1945 && stormPhaseMs < 2020) {
      strikeVisible = true;
      boltX -= 2;
      boltY += 8;
      boltOpa = LV_OPA_70;
      flashOpa = LV_OPA_30;
    }

    if (strikeVisible) {
      lv_obj_clear_flag(ui.bolt, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_text_opa(ui.bolt, boltOpa, 0);
      lv_obj_set_pos(ui.bolt, boltX, boltY);
    }

    if (flashOpa > LV_OPA_TRANSP && ui.flashOverlay) {
      lv_obj_clear_flag(ui.flashOverlay, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_opa(ui.flashOverlay, flashOpa, 0);
    }
  }

  if (scene == WeatherSceneType::Fog) {
    for (int i = 0; i < 3; ++i) {
      lv_obj_clear_flag(ui.fogBars[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(ui.fogBars[i], 48 + static_cast<int>((tick % 10) - 5), 82 + (i * 34));
    }
  }
}

void handleWeatherStackSwipe(int deltaY)
{
  if (deltaY <= -kWeatherNavSwipeMinDistancePx) {
    showNextScreen();
    noteActivity();
    return;
  }

  if (deltaY >= kWeatherNavSwipeMinDistancePx) {
    showPreviousScreen();
    noteActivity();
  }
}

void handleWeatherSwipe(int deltaX)
{
  if (deltaX <= -kWeatherSwipeMinDistancePx) {
    weatherModuleCycleDebugScene(1);
    noteActivity();
    return;
  }

  if (deltaX >= kWeatherSwipeMinDistancePx) {
    weatherModuleCycleDebugScene(-1);
    noteActivity();
  }
}

void setUnavailableState(const char *updatedText)
{
  gState.hasData = false;
  gState.stale = true;
  gState.isDay = true;
  gState.weatherCode = -1;
  gState.temperatureF = 0;
  gState.feelsLikeF = 0;
  gState.highF = 0;
  gState.lowF = 0;
  gState.windMph = 0;
  gState.precipitationPercent = 0;
  gState.cloudCoverPercent = 0;
  gState.condition = kDefaultUnavailableCondition;
  gState.updated = updatedText ? updatedText : "Waiting for Wi-Fi";
  gState.sunrise = "--:--";
  gState.sunset = "--:--";
  gState.location = WEATHER_LOCATION_LABEL;
  for (size_t i = 0; i < kHourlyForecastCount; ++i) {
    gState.hourly[i] = {};
  }
  for (size_t i = 0; i < kDailyForecastCount; ++i) {
    gState.daily[i] = {};
  }
}

void saveWeatherCache()
{
  if (!gConfig.preferences || !gConfig.cacheKey || !gState.hasData) {
    return;
  }

  DynamicJsonDocument doc(8192);
  doc["hasData"] = gState.hasData;
  doc["stale"] = gState.stale;
  doc["isDay"] = gState.isDay;
  doc["weatherCode"] = gState.weatherCode;
  doc["temperatureF"] = gState.temperatureF;
  doc["feelsLikeF"] = gState.feelsLikeF;
  doc["highF"] = gState.highF;
  doc["lowF"] = gState.lowF;
  doc["windMph"] = gState.windMph;
  doc["precipitationPercent"] = gState.precipitationPercent;
  doc["cloudCoverPercent"] = gState.cloudCoverPercent;
  doc["condition"] = gState.condition;
  doc["updated"] = gState.updated;
  doc["sunrise"] = gState.sunrise;
  doc["sunset"] = gState.sunset;
  doc["location"] = gState.location;

  JsonArray hourly = doc.createNestedArray("hourly");
  for (size_t i = 0; i < kHourlyForecastCount; ++i) {
    JsonObject item = hourly.createNestedObject();
    item["valid"] = gState.hourly[i].valid;
    item["hour"] = gState.hourly[i].hour;
    item["temperatureF"] = gState.hourly[i].temperatureF;
    item["precipitationPercent"] = gState.hourly[i].precipitationPercent;
    item["weatherCode"] = gState.hourly[i].weatherCode;
    item["isDay"] = gState.hourly[i].isDay;
  }

  JsonArray daily = doc.createNestedArray("daily");
  for (size_t i = 0; i < kDailyForecastCount; ++i) {
    JsonObject item = daily.createNestedObject();
    item["valid"] = gState.daily[i].valid;
    item["day"] = gState.daily[i].day;
    item["highF"] = gState.daily[i].highF;
    item["lowF"] = gState.daily[i].lowF;
    item["precipitationPercent"] = gState.daily[i].precipitationPercent;
    item["weatherCode"] = gState.daily[i].weatherCode;
  }

  String serialized;
  serializeJson(doc, serialized);
  gConfig.preferences->putString(gConfig.cacheKey, serialized);
}

static void refreshUi()
{
  refreshCurrentWeatherScreen(gUi, gState, networkOnline(), gDebugOverrideEnabled, gDebugScene);
  updateWeatherHero(gUi, gState, gDebugOverrideEnabled, gDebugScene, millis());
}

WeatherSceneType cycleWeatherScene(WeatherSceneType scene, int direction)
{
  int value = static_cast<int>(scene);
  value = (value + direction + static_cast<int>(WeatherSceneType::Fog) + 1) %
          (static_cast<int>(WeatherSceneType::Fog) + 1);
  return static_cast<WeatherSceneType>(value);
}

bool fetchWeather()
{
  if (!networkOnline()) {
    return false;
  }

  String url = apiUrl();
  if (url.isEmpty()) {
    return false;
  }

  gFetchInProgress = true;
  Serial.println("Fetching weather...");

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(gConfig.fetchTimeoutMs);
  http.setTimeout(gConfig.fetchTimeoutMs);

  bool success = false;
  if (http.begin(secureClient, url)) {
    int statusCode = http.GET();
    if (statusCode == HTTP_CODE_OK) {
      DynamicJsonDocument doc(24576);
      DeserializationError error = deserializeJson(doc, http.getString());
      if (!error) {
        JsonObject current = doc["current"];
        JsonObject hourly = doc["hourly"];
        JsonObject daily = doc["daily"];

        JsonArray hourlyTimes = hourly["time"].as<JsonArray>();
        JsonArray hourlyTemps = hourly["temperature_2m"].as<JsonArray>();
        JsonArray hourlyPrecip = hourly["precipitation_probability"].as<JsonArray>();
        JsonArray hourlyCodes = hourly["weather_code"].as<JsonArray>();
        JsonArray hourlyIsDay = hourly["is_day"].as<JsonArray>();

        JsonArray dayArray = daily["time"].as<JsonArray>();
        JsonArray highArray = daily["temperature_2m_max"].as<JsonArray>();
        JsonArray lowArray = daily["temperature_2m_min"].as<JsonArray>();
        JsonArray sunriseArray = daily["sunrise"].as<JsonArray>();
        JsonArray sunsetArray = daily["sunset"].as<JsonArray>();
        JsonArray precipitationArray = daily["precipitation_probability_max"].as<JsonArray>();
        JsonArray dailyCodes = daily["weather_code"].as<JsonArray>();

        float temperature = current["temperature_2m"].as<float>();
        float feelsLike = current["apparent_temperature"].as<float>();
        float wind = current["wind_speed_10m"].as<float>();
        float high = !highArray.isNull() && highArray.size() > 0 ? highArray[0].as<float>() : temperature;
        float low = !lowArray.isNull() && lowArray.size() > 0 ? lowArray[0].as<float>() : temperature;

        gState.hasData = true;
        gState.stale = false;
        gState.isDay = (current["is_day"] | 1) == 1;
        gState.weatherCode = current["weather_code"] | -1;
        gState.temperatureF = static_cast<int>(roundf(temperature));
        gState.feelsLikeF = static_cast<int>(roundf(feelsLike));
        gState.highF = static_cast<int>(roundf(high));
        gState.lowF = static_cast<int>(roundf(low));
        gState.windMph = static_cast<int>(roundf(wind));
        gState.precipitationPercent =
            !precipitationArray.isNull() && precipitationArray.size() > 0 ? precipitationArray[0].as<int>() : 0;
        gState.cloudCoverPercent = current["cloud_cover"] | 0;
        gState.condition = weatherConditionFromCode(gState.weatherCode, gState.isDay);
        gState.updated = updatedLabel();
        gState.sunrise =
            !sunriseArray.isNull() && sunriseArray.size() > 0 ? weatherTimeFragment(sunriseArray[0].as<const char *>()) : "--:--";
        gState.sunset =
            !sunsetArray.isNull() && sunsetArray.size() > 0 ? weatherTimeFragment(sunsetArray[0].as<const char *>()) : "--:--";
        gState.location = WEATHER_LOCATION_LABEL;

        for (size_t i = 0; i < kHourlyForecastCount; ++i) {
          gState.hourly[i] = {};
          if (hourlyTimes.isNull() || hourlyTemps.isNull() || i >= hourlyTimes.size() || i >= hourlyTemps.size()) {
            continue;
          }

          gState.hourly[i].valid = true;
          gState.hourly[i].hour = weatherHourFragment(hourlyTimes[i].as<const char *>());
          gState.hourly[i].temperatureF = static_cast<int>(roundf(hourlyTemps[i].as<float>()));
          gState.hourly[i].precipitationPercent =
              !hourlyPrecip.isNull() && i < hourlyPrecip.size() ? hourlyPrecip[i].as<int>() : 0;
          gState.hourly[i].weatherCode =
              !hourlyCodes.isNull() && i < hourlyCodes.size() ? (hourlyCodes[i] | -1) : -1;
          gState.hourly[i].isDay =
              !hourlyIsDay.isNull() && i < hourlyIsDay.size() ? ((hourlyIsDay[i] | 1) == 1) : true;
        }

        for (size_t i = 0; i < kDailyForecastCount; ++i) {
          gState.daily[i] = {};
          if (dayArray.isNull() || highArray.isNull() || lowArray.isNull() ||
              i >= dayArray.size() || i >= highArray.size() || i >= lowArray.size()) {
            continue;
          }

          gState.daily[i].valid = true;
          gState.daily[i].day = weatherDayFragment(dayArray[i].as<const char *>());
          gState.daily[i].highF = static_cast<int>(roundf(highArray[i].as<float>()));
          gState.daily[i].lowF = static_cast<int>(roundf(lowArray[i].as<float>()));
          gState.daily[i].precipitationPercent =
              !precipitationArray.isNull() && i < precipitationArray.size() ? precipitationArray[i].as<int>() : 0;
          gState.daily[i].weatherCode =
              !dailyCodes.isNull() && i < dailyCodes.size() ? (dailyCodes[i] | -1) : -1;
        }

        saveWeatherCache();
        gNextRefreshAtMs = millis() + gConfig.refreshIntervalMs;
        success = true;
      } else {
        Serial.printf("Weather JSON parse failed: %s\n", error.c_str());
      }
    } else {
      String responseBody = http.getString();
      Serial.printf("Weather request failed: HTTP %d\n", statusCode);
      if (responseBody.length() > 0) {
        Serial.printf("Weather response body: %s\n", responseBody.c_str());
      }
    }
    http.end();
  } else {
    Serial.println("Weather request setup failed");
  }

  if (!success) {
    if (gState.hasData) {
      gState.stale = true;
      gState.updated = kDefaultOfflineWeatherLabel;
    } else {
      setUnavailableState(networkOnline() ? "Retrying shortly" : "Waiting for Wi-Fi");
    }
    gNextRefreshAtMs = millis() + gConfig.retryIntervalMs;
  }

  gFetchInProgress = false;
  return success;
}

void weatherModuleConfigure(const WeatherModuleConfig &config)
{
  gConfig = config;
}

bool weatherModuleRestoreCache()
{
  if (!gConfig.preferences || !gConfig.cacheKey) {
    return false;
  }

  String serialized = gConfig.preferences->getString(gConfig.cacheKey, "");
  if (serialized.isEmpty()) {
    return false;
  }

  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, serialized);
  if (error) {
    Serial.printf("Weather cache parse failed: %s\n", error.c_str());
    return false;
  }

  gState.hasData = doc["hasData"] | false;
  if (!gState.hasData) {
    return false;
  }

  gState.stale = true;
  gState.isDay = doc["isDay"] | true;
  gState.weatherCode = doc["weatherCode"] | -1;
  gState.temperatureF = doc["temperatureF"] | 0;
  gState.feelsLikeF = doc["feelsLikeF"] | 0;
  gState.highF = doc["highF"] | 0;
  gState.lowF = doc["lowF"] | 0;
  gState.windMph = doc["windMph"] | 0;
  gState.precipitationPercent = doc["precipitationPercent"] | 0;
  gState.cloudCoverPercent = doc["cloudCoverPercent"] | 0;
  gState.condition = String(static_cast<const char *>(doc["condition"] | kDefaultUnavailableCondition));
  gState.updated = kDefaultOfflineWeatherLabel;
  gState.sunrise = String(static_cast<const char *>(doc["sunrise"] | "--:--"));
  gState.sunset = String(static_cast<const char *>(doc["sunset"] | "--:--"));
  gState.location = String(static_cast<const char *>(doc["location"] | WEATHER_LOCATION_LABEL));

  JsonArray hourly = doc["hourly"].as<JsonArray>();
  for (size_t i = 0; i < kHourlyForecastCount; ++i) {
    gState.hourly[i] = {};
    if (hourly.isNull() || i >= hourly.size()) {
      continue;
    }
    JsonObject item = hourly[i].as<JsonObject>();
    gState.hourly[i].valid = item["valid"] | false;
    gState.hourly[i].hour = String(static_cast<const char *>(item["hour"] | "--"));
    gState.hourly[i].temperatureF = item["temperatureF"] | 0;
    gState.hourly[i].precipitationPercent = item["precipitationPercent"] | 0;
    gState.hourly[i].weatherCode = item["weatherCode"] | -1;
    gState.hourly[i].isDay = item["isDay"] | true;
  }

  JsonArray daily = doc["daily"].as<JsonArray>();
  for (size_t i = 0; i < kDailyForecastCount; ++i) {
    gState.daily[i] = {};
    if (daily.isNull() || i >= daily.size()) {
      continue;
    }
    JsonObject item = daily[i].as<JsonObject>();
    gState.daily[i].valid = item["valid"] | false;
    gState.daily[i].day = String(static_cast<const char *>(item["day"] | "---"));
    gState.daily[i].highF = item["highF"] | 0;
    gState.daily[i].lowF = item["lowF"] | 0;
    gState.daily[i].precipitationPercent = item["precipitationPercent"] | 0;
    gState.daily[i].weatherCode = item["weatherCode"] | -1;
  }

  Serial.println("Restored cached weather state");
  return true;
}

bool weatherModuleHasData()
{
  return gState.hasData;
}

void weatherModuleSetUnavailable(const char *updatedLabel)
{
  setUnavailableState(updatedLabel);
}

void weatherModuleMarkOffline()
{
  if (gState.hasData) {
    gState.stale = true;
    gState.updated = kDefaultOfflineWeatherLabel;
  } else {
    setUnavailableState("Offline - no cached weather");
  }
}

void weatherModuleScheduleRefreshIn(uint32_t delayMs)
{
  gNextRefreshAtMs = millis() + delayMs;
}

void weatherModuleUpdate()
{
  if (!networkOnline()) {
    if (gState.hasData && !gState.stale) {
      gState.stale = true;
      gState.updated = kDefaultOfflineWeatherLabel;
    }
    return;
  }

  if (!gConfig.fetchEnabled || gFetchInProgress || otaBusy() || lightSleepActive()) {
    return;
  }

  if (static_cast<int32_t>(millis() - gNextRefreshAtMs) >= 0) {
    fetchWeather();
  }
}

void weatherModuleCycleDebugScene(int direction)
{
  gDebugOverrideEnabled = true;
  gDebugScene = cycleWeatherScene(gDebugScene, direction);
  if (gUi.currentScreen) {
    refreshUi();
  }
}

void weatherModuleClearDebugOverride()
{
  gDebugOverrideEnabled = false;
}

void weatherModuleRefreshUi()
{
  if (!gUi.currentScreen || !gUi.cityLabel || !gUi.tempLabel || !gUi.conditionLabel) {
    return;
  }

  refreshUi();
}

lv_obj_t *waveformWeatherScreenRoot()
{
  return gRoot;
}

bool waveformBuildWeatherScreen()
{
  if (!gRoot) {
    gRoot = buildCurrentWeatherScreen(gUi);
  }

  return gRoot && gUi.currentScreen && gUi.cityLabel && gUi.tempLabel && gUi.conditionLabel;
}

bool waveformRefreshWeatherScreen()
{
  if (!gUi.currentScreen || !gUi.cityLabel || !gUi.tempLabel || !gUi.conditionLabel) {
    return false;
  }

  refreshUi();
  return true;
}

void waveformEnterWeatherScreen()
{
  gDebugOverrideEnabled = false;
  if (gConfig.fetchEnabled && networkOnline() && !gFetchInProgress && !otaBusy() && !lightSleepActive()) {
    fetchWeather();
  }
}

void waveformLeaveWeatherScreen()
{
}

void waveformDestroyWeatherScreen()
{
  if (gUi.moonBuf) {
    lv_draw_buf_destroy(gUi.moonBuf);
    gUi.moonBuf = nullptr;
  }
  if (gRoot) {
    lv_obj_delete(gRoot);
    gRoot = nullptr;
  }
  gUi = WeatherUi{};
}

void waveformTickWeatherScreen(uint32_t nowMs)
{
  (void)nowMs;
  if (gUi.currentScreen) {
    updateWeatherHero(gUi, gState, gDebugOverrideEnabled, gDebugScene, millis());
  }
}

const ScreenModule &weatherScreenModule()
{
  return kModule;
}


String weatherApiUrl()
{
  char url[768];
  snprintf(url,
           sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,apparent_temperature,is_day,weather_code,wind_speed_10m,cloud_cover"
           "&hourly=temperature_2m,precipitation_probability,weather_code,is_day"
           "&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset,precipitation_probability_max,weather_code"
           "&temperature_unit=fahrenheit&wind_speed_unit=mph&timezone=auto&forecast_days=10&forecast_hours=24",
           WEATHER_LATITUDE,
           WEATHER_LONGITUDE);
  return String(url);
}

String weatherUpdatedLabel()
{
  if (!hasValidTime()) {
    return "Updated just now";
  }

  char buffer[24];
  struct tm localTime = {};
  time_t now = time(nullptr);
  localtime_r(&now, &localTime);
  strftime(buffer, sizeof(buffer), "Updated %H:%M", &localTime);
  return String(buffer);
}
