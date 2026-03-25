#pragma once

#include <Arduino.h>
#include <lvgl.h>

constexpr size_t kWeatherParticleCount = 8;
constexpr size_t kWeatherSunRayCount = 8;
constexpr size_t kHourlyForecastCount = 24;
constexpr size_t kDailyForecastCount = 10;

enum class WeatherSceneType : uint8_t
{
  Clear,
  PartlyCloudy,
  Cloudy,
  Rain,
  Snow,
  Storm,
  Fog,
};

struct HourlyForecastEntry
{
  bool valid = false;
  String hour = "--";
  int temperatureF = 0;
  int precipitationPercent = 0;
  int weatherCode = -1;
  bool isDay = true;
};

struct DailyForecastEntry
{
  bool valid = false;
  String day = "---";
  int highF = 0;
  int lowF = 0;
  int precipitationPercent = 0;
  int weatherCode = -1;
};

struct WeatherState
{
  bool hasData = false;
  bool stale = true;
  bool isDay = true;
  int weatherCode = -1;
  int temperatureF = 0;
  int feelsLikeF = 0;
  int highF = 0;
  int lowF = 0;
  int windMph = 0;
  int precipitationPercent = 0;
  int cloudCoverPercent = 0;
  String condition = "Waiting for weather";
  String updated = "Waiting for Wi-Fi";
  String sunrise = "--:--";
  String sunset = "--:--";
  String location = "Weather";
  HourlyForecastEntry hourly[kHourlyForecastCount] = {};
  DailyForecastEntry daily[kDailyForecastCount] = {};
};

struct WeatherUi
{
  lv_obj_t *currentScreen = nullptr;
  lv_obj_t *hero = nullptr;
  lv_obj_t *cityLabel = nullptr;
  lv_obj_t *tempLabel = nullptr;
  lv_obj_t *tempMeta = nullptr;
  lv_obj_t *rangeLine = nullptr;
  lv_obj_t *sunEventIcon = nullptr;
  lv_obj_t *sunEventHorizon = nullptr;
  lv_obj_t *sunEventSun = nullptr;
  lv_obj_t *sunEventArrow = nullptr;
  lv_obj_t *sunEventTimeLabel = nullptr;
  lv_obj_t *feelsLabel = nullptr;
  lv_obj_t *conditionLabel = nullptr;
  lv_obj_t *primaryLabel = nullptr;
  lv_obj_t *updatedValueLabel = nullptr;
  lv_obj_t *sun = nullptr;
  lv_obj_t *sunRays[kWeatherSunRayCount] = {nullptr};
  lv_point_precise_t sunRayPoints[kWeatherSunRayCount][2] = {};
  lv_obj_t *moon = nullptr;         // canvas replacing the old circle pair
  lv_draw_buf_t *moonBuf = nullptr;
  lv_obj_t *moonShadow = nullptr;   // unused, kept nil
  lv_obj_t *clouds[3] = {nullptr};
  lv_obj_t *particles[kWeatherParticleCount] = {nullptr};
  lv_obj_t *bolt = nullptr;
  lv_obj_t *fogBars[3] = {nullptr};
  lv_obj_t *flashOverlay = nullptr;

  lv_obj_t *hourlyScreen = nullptr;
  lv_obj_t *hourlyHourLabels[kHourlyForecastCount] = {nullptr};
  lv_obj_t *hourlyTracks[kHourlyForecastCount] = {nullptr};
  lv_obj_t *hourlyFills[kHourlyForecastCount] = {nullptr};

  lv_obj_t *dailyScreen = nullptr;
  lv_obj_t *dailyDayLabels[kDailyForecastCount] = {nullptr};
  lv_obj_t *dailyTracks[kDailyForecastCount] = {nullptr};
  lv_obj_t *dailyFills[kDailyForecastCount] = {nullptr};
};

String weatherTimeFragment(const char *isoText);
String weatherHourFragment(const char *isoText);
String weatherDayFragment(const char *isoText);
WeatherSceneType weatherSceneTypeForCode(int weatherCode);
const char *weatherSceneName(WeatherSceneType scene);
String weatherConditionFromCode(int weatherCode, bool isDay);

lv_obj_t *buildCurrentWeatherScreen(WeatherUi &ui);
lv_obj_t *buildHourlyForecastScreen(WeatherUi &ui);
lv_obj_t *buildDailyForecastScreen(WeatherUi &ui);

void refreshCurrentWeatherScreen(WeatherUi &ui,
                                 const WeatherState &state,
                                 bool networkOnline,
                                 bool debugOverrideEnabled,
                                 WeatherSceneType debugScene);
void refreshHourlyForecastScreen(WeatherUi &ui, const WeatherState &state);
void refreshDailyForecastScreen(WeatherUi &ui, const WeatherState &state);
void updateWeatherHero(WeatherUi &ui,
                       const WeatherState &state,
                       bool debugOverrideEnabled,
                       WeatherSceneType debugScene,
                       uint32_t nowMs);
