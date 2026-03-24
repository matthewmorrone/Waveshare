#include "config/screen_constants.h"
#include "core/screen_manager.h"
#include "modules/weather_module.h"
#include "modules/wifi_manager.h"
#include "screens/screen_callbacks.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>

extern lv_obj_t *screenRoots[];
bool hasValidTime();
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
float compressedOrbitRadius(float distanceAu);
bool parseHorizonsVectorRow(const String &result, float &xAu, float &yAu, float &zAu);
extern bool otaInProgress;
extern bool inLightSleep;
extern int currentScreenIndex;
void refreshSolarScreen();

namespace
{
const ScreenModule kModule = {
    ScreenId::Solar,
    "Solar",
    waveformBuildSolarScreen,
    waveformRefreshSolarScreen,
    waveformEnterSolarScreen,
    waveformLeaveSolarScreen,
    waveformTickSolarScreen,
    waveformSolarScreenRoot,
};
} // namespace

const ScreenModule &solarScreenModule()
{
  return kModule;
}


struct SolarPlanetDefinition
{
  const char *name;
  const char *shortLabel;
  const char *command;
  float orbitRadiusAu;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

struct SolarPlanetState
{
  bool valid = false;
  float xAu = 0.0f;
  float yAu = 0.0f;
  float zAu = 0.0f;
};

struct SolarSystemState
{
  bool hasData = false;
  bool stale = true;
  size_t fetchIndex = 0;
  String updated = "Waiting for Wi-Fi";
  SolarPlanetState planets[kSolarPlanetCount] = {};
};

struct SolarUi
{
  lv_obj_t *screen = nullptr;
  lv_obj_t *titleLabel = nullptr;
  lv_obj_t *panel = nullptr;
  lv_obj_t *sourceLabel = nullptr;
  lv_obj_t *updatedLabel = nullptr;
  lv_obj_t *sun = nullptr;
  lv_obj_t *orbitRings[kSolarPlanetCount] = {};
  lv_obj_t *planetDots[kSolarPlanetCount] = {};
  lv_obj_t *planetLabels[kSolarPlanetCount] = {};
};


constexpr SolarPlanetDefinition kSolarPlanets[kSolarPlanetCount] = {
  {"Mercury", "Me", "199", 0.387f, 180, 180, 170},
  {"Venus", "Ve", "299", 0.723f, 214, 174, 104},
  {"Earth", "Ea", "399", 1.000f, 86, 160, 255},
  {"Mars", "Ma", "499", 1.524f, 220, 110, 82},
  {"Jupiter", "Ju", "599", 5.203f, 210, 174, 126},
  {"Saturn", "Sa", "699", 9.537f, 214, 192, 142},
  {"Uranus", "Ur", "799", 19.191f, 126, 214, 214},
  {"Neptune", "Ne", "899", 30.070f, 92, 132, 255},
};


SolarSystemState solarSystemState;
SolarUi solarUi;
bool solarBuilt = false;
bool solarFetchInProgress = false;
uint32_t nextSolarRefreshAtMs = 0;

void setSolarUnavailableState(const char *updatedLabel)
{
  solarSystemState.hasData = false;
  solarSystemState.stale = true;
  solarSystemState.fetchIndex = 0;
  solarSystemState.updated = updatedLabel;
  for (size_t i = 0; i < kSolarPlanetCount; ++i) {
    solarSystemState.planets[i] = {};
  }
}


bool fetchSolarPlanetVector(const char *command, float &xAu, float &yAu, float &zAu)
{
  if (!networkIsOnline() || !hasValidTime()) {
    return false;
  }

  time_t now = time(nullptr);
  time_t then = now + 60;
  struct tm startTm = {};
  struct tm stopTm = {};
  gmtime_r(&now, &startTm);
  gmtime_r(&then, &stopTm);

  char startDate[16];
  char startClock[8];
  char stopDate[16];
  char stopClock[8];
  strftime(startDate, sizeof(startDate), "%Y-%m-%d", &startTm);
  strftime(startClock, sizeof(startClock), "%H:%M", &startTm);
  strftime(stopDate, sizeof(stopDate), "%Y-%m-%d", &stopTm);
  strftime(stopClock, sizeof(stopClock), "%H:%M", &stopTm);

  char url[512];
  snprintf(url,
           sizeof(url),
           "https://ssd.jpl.nasa.gov/api/horizons.api?format=json&COMMAND='%.3s'&OBJ_DATA='NO'"
           "&MAKE_EPHEM='YES'&EPHEM_TYPE='VECTORS'&CENTER='500@10'&TIME_TYPE='UT'"
           "&START_TIME='%s%%20%s'&STOP_TIME='%s%%20%s'&STEP_SIZE='1%%20m'"
           "&OUT_UNITS='AU-D'&VEC_TABLE='2'&VEC_LABELS='NO'&CSV_FORMAT='YES'",
           command,
           startDate,
           startClock,
           stopDate,
           stopClock);

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(kSolarFetchTimeoutMs);
  http.setTimeout(kSolarFetchTimeoutMs);

  if (!http.begin(secureClient, url)) {
    Serial.println("Solar request setup failed");
    return false;
  }

  int statusCode = http.GET();
  if (statusCode != HTTP_CODE_OK) {
    String responseBody = http.getString();
    Serial.printf("Solar request failed for %s: HTTP %d\n", command, statusCode);
    if (responseBody.length() > 0) {
      Serial.printf("Solar response body: %s\n", responseBody.c_str());
    }
    http.end();
    return false;
  }

  DynamicJsonDocument doc(12288);
  DeserializationError error = deserializeJson(doc, http.getString());
  http.end();
  if (error) {
    Serial.printf("Solar JSON parse failed for %s: %s\n", command, error.c_str());
    return false;
  }

  String result = String(static_cast<const char *>(doc["result"] | ""));
  if (!parseHorizonsVectorRow(result, xAu, yAu, zAu)) {
    Serial.printf("Solar vector parse failed for %s\n", command);
    return false;
  }

  return true;
}


void startSolarSystemFetch()
{
  if (solarFetchInProgress) {
    return;
  }

  if (!hasValidTime()) {
    setSolarUnavailableState("Waiting for time");
    return;
  }

  if (!networkIsOnline()) {
    setSolarUnavailableState("Waiting for Wi-Fi");
    return;
  }

  solarFetchInProgress = true;
  solarSystemState.fetchIndex = 0;
  solarSystemState.updated = "Fetching Mercury";
  for (size_t i = 0; i < kSolarPlanetCount; ++i) {
    solarSystemState.planets[i] = {};
  }
}

void serviceSolarSystemFetch()
{
  if (!solarFetchInProgress) {
    return;
  }

  if (solarSystemState.fetchIndex >= kSolarPlanetCount) {
    solarFetchInProgress = false;
    solarSystemState.hasData = true;
    solarSystemState.stale = false;
    solarSystemState.updated = weatherUpdatedLabel() + "  JPL Horizons";
    nextSolarRefreshAtMs = millis() + kSolarRefreshIntervalMs;
    refreshSolarScreen();
    return;
  }

  size_t index = solarSystemState.fetchIndex;
  solarSystemState.updated = String("Fetching ") + kSolarPlanets[index].name;
  float xAu = 0.0f;
  float yAu = 0.0f;
  float zAu = 0.0f;
  if (!fetchSolarPlanetVector(kSolarPlanets[index].command, xAu, yAu, zAu)) {
    solarFetchInProgress = false;
    if (solarSystemState.hasData) {
      solarSystemState.stale = true;
      solarSystemState.updated = networkIsOnline() ? "Retrying with cached solar data" : "Offline - cached solar data";
    } else {
      setSolarUnavailableState(networkIsOnline() ? "Retrying shortly" : "Waiting for Wi-Fi");
    }
    nextSolarRefreshAtMs = millis() + kSolarRetryIntervalMs;
    refreshSolarScreen();
    return;
  }

  solarSystemState.planets[index].valid = true;
  solarSystemState.planets[index].xAu = xAu;
  solarSystemState.planets[index].yAu = yAu;
  solarSystemState.planets[index].zAu = zAu;
  ++solarSystemState.fetchIndex;
  refreshSolarScreen();
}

void updateSolarSystem()
{
  ScreenId currentScreen = static_cast<ScreenId>(currentScreenIndex);
  if (currentScreen != ScreenId::Solar) {
    return;
  }

  if (solarFetchInProgress) {
    serviceSolarSystemFetch();
    return;
  }

  if (otaInProgress || inLightSleep) {
    return;
  }

  if (!networkIsOnline()) {
    if (solarSystemState.hasData && !solarSystemState.stale) {
      solarSystemState.stale = true;
      solarSystemState.updated = "Offline - cached solar data";
      refreshSolarScreen();
    }
    return;
  }

  if (!hasValidTime()) {
    if (!solarSystemState.hasData) {
      setSolarUnavailableState("Waiting for time");
      refreshSolarScreen();
    }
    return;
  }

  if (!solarSystemState.hasData || solarSystemState.stale || static_cast<int32_t>(millis() - nextSolarRefreshAtMs) >= 0) {
    startSolarSystemFetch();
  }
}


lv_obj_t *buildSolarScreen()
{
  solarUi.screen = lv_obj_create(nullptr);
  applyRootStyle(solarUi.screen);

  solarUi.titleLabel = lv_label_create(solarUi.screen);
  lv_obj_set_width(solarUi.titleLabel, LCD_WIDTH - 40);
  lv_obj_set_style_text_font(solarUi.titleLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(solarUi.titleLabel, lvColor(248, 250, 252), 0);
  lv_obj_set_style_text_align(solarUi.titleLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(solarUi.titleLabel, "Solar System");
  lv_obj_align(solarUi.titleLabel, LV_ALIGN_TOP_MID, 0, kSolarTitleY);

  solarUi.panel = lv_obj_create(solarUi.screen);
  lv_obj_set_size(solarUi.panel, kSolarPanelSize, kSolarPanelSize);
  lv_obj_align(solarUi.panel, LV_ALIGN_TOP_MID, 0, kSolarPanelY);
  lv_obj_set_style_radius(solarUi.panel, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(solarUi.panel, lvColor(4, 10, 18), 0);
  lv_obj_set_style_bg_opa(solarUi.panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(solarUi.panel, 1, 0);
  lv_obj_set_style_border_color(solarUi.panel, lvColor(28, 42, 58), 0);
  lv_obj_set_style_pad_all(solarUi.panel, 0, 0);
  lv_obj_clear_flag(solarUi.panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(solarUi.panel, LV_OBJ_FLAG_CLICKABLE);

  for (size_t i = 0; i < kSolarPlanetCount; ++i) {
    solarUi.orbitRings[i] = lv_obj_create(solarUi.panel);
    lv_obj_set_style_radius(solarUi.orbitRings[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(solarUi.orbitRings[i], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(solarUi.orbitRings[i], 1, 0);
    lv_obj_set_style_border_color(solarUi.orbitRings[i], lvColor(24, 32, 44), 0);
    lv_obj_set_style_pad_all(solarUi.orbitRings[i], 0, 0);
    lv_obj_clear_flag(solarUi.orbitRings[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(solarUi.orbitRings[i], LV_OBJ_FLAG_CLICKABLE);

    solarUi.planetDots[i] = lv_obj_create(solarUi.panel);
    lv_obj_set_style_radius(solarUi.planetDots[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(solarUi.planetDots[i], 0, 0);
    lv_obj_set_style_pad_all(solarUi.planetDots[i], 0, 0);
    lv_obj_clear_flag(solarUi.planetDots[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(solarUi.planetDots[i], LV_OBJ_FLAG_CLICKABLE);

    solarUi.planetLabels[i] = lv_label_create(solarUi.panel);
    lv_obj_set_style_text_font(solarUi.planetLabels[i], &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(solarUi.planetLabels[i], lvColor(228, 234, 242), 0);
    lv_label_set_text(solarUi.planetLabels[i], kSolarPlanets[i].shortLabel);
  }

  solarUi.sun = lv_obj_create(solarUi.panel);
  lv_obj_set_size(solarUi.sun, 16, 16);
  lv_obj_set_style_radius(solarUi.sun, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(solarUi.sun, lvColor(255, 184, 42), 0);
  lv_obj_set_style_bg_opa(solarUi.sun, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(solarUi.sun, 0, 0);
  lv_obj_set_style_pad_all(solarUi.sun, 0, 0);
  lv_obj_clear_flag(solarUi.sun, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(solarUi.sun, LV_OBJ_FLAG_CLICKABLE);

  solarUi.sourceLabel = lv_label_create(solarUi.screen);
  lv_obj_set_width(solarUi.sourceLabel, LCD_WIDTH - 52);
  lv_obj_set_style_text_font(solarUi.sourceLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(solarUi.sourceLabel, lvColor(164, 174, 188), 0);
  lv_obj_set_style_text_align(solarUi.sourceLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(solarUi.sourceLabel, "Live heliocentric positions");
  lv_obj_align(solarUi.sourceLabel, LV_ALIGN_BOTTOM_MID, 0, -40);

  solarUi.updatedLabel = lv_label_create(solarUi.screen);
  lv_obj_set_width(solarUi.updatedLabel, LCD_WIDTH - 56);
  lv_obj_set_style_text_font(solarUi.updatedLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(solarUi.updatedLabel, lvColor(112, 124, 140), 0);
  lv_obj_set_style_text_align(solarUi.updatedLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(solarUi.updatedLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(solarUi.updatedLabel, "");
  lv_obj_align(solarUi.updatedLabel, LV_ALIGN_BOTTOM_MID, 0, -16);

  solarBuilt = true;
  return solarUi.screen;
}

void buildSolarScreenRoot()
{
  screenRoots[static_cast<size_t>(ScreenId::Solar)] = buildSolarScreen();
}

void refreshSolarScreen()
{
  if (!solarBuilt || !solarUi.panel || !solarUi.updatedLabel || !solarUi.sourceLabel || !solarUi.sun) {
    return;
  }

  lv_obj_align(solarUi.sun, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(solarUi.sourceLabel, "Live JPL Horizons vectors");
  lv_label_set_text(solarUi.updatedLabel, solarSystemState.updated.c_str());

  float maxCompressedRadius = compressedOrbitRadius(kSolarPlanets[kSolarPlanetCount - 1].orbitRadiusAu);
  float scale = ((static_cast<float>(kSolarPanelSize) * 0.5f) - 18.0f) / maxCompressedRadius;
  float panelRadius = static_cast<float>(kSolarPanelSize) * 0.5f;

  for (size_t i = 0; i < kSolarPlanetCount; ++i) {
    float orbitRadius = compressedOrbitRadius(kSolarPlanets[i].orbitRadiusAu) * scale;
    int orbitDiameter = static_cast<int>(roundf(orbitRadius * 2.0f));
    lv_obj_set_size(solarUi.orbitRings[i], orbitDiameter, orbitDiameter);
    lv_obj_align(solarUi.orbitRings[i], LV_ALIGN_CENTER, 0, 0);

    if (!solarSystemState.planets[i].valid) {
      lv_obj_add_flag(solarUi.planetDots[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(solarUi.planetLabels[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }

    float xAu = solarSystemState.planets[i].xAu;
    float yAu = solarSystemState.planets[i].yAu;
    float distanceAu = sqrtf((xAu * xAu) + (yAu * yAu));
    float displayRadius = compressedOrbitRadius(distanceAu) * scale;
    float angleRadians = atan2f(yAu, xAu);
    float localX = panelRadius + (cosf(angleRadians) * displayRadius);
    float localY = panelRadius - (sinf(angleRadians) * displayRadius);
    int dotSize = i < 4 ? 8 : 10;

    lv_obj_set_size(solarUi.planetDots[i], dotSize, dotSize);
    lv_obj_set_pos(solarUi.planetDots[i],
                   static_cast<int>(roundf(localX)) - (dotSize / 2),
                   static_cast<int>(roundf(localY)) - (dotSize / 2));
    lv_obj_set_style_bg_color(solarUi.planetDots[i],
                              lvColor(kSolarPlanets[i].red, kSolarPlanets[i].green, kSolarPlanets[i].blue),
                              0);
    lv_obj_set_style_bg_opa(solarUi.planetDots[i], LV_OPA_COVER, 0);
    lv_obj_clear_flag(solarUi.planetDots[i], LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_pos(solarUi.planetLabels[i],
                   static_cast<int>(roundf(localX)) + 6,
                   static_cast<int>(roundf(localY)) - 8);
    lv_obj_clear_flag(solarUi.planetLabels[i], LV_OBJ_FLAG_HIDDEN);
  }
}

lv_obj_t *waveformSolarScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Solar)];
}

bool waveformBuildSolarScreen()
{
  if (!waveformSolarScreenRoot()) {
    buildSolarScreenRoot();
  }

  return solarBuilt && waveformSolarScreenRoot() && solarUi.screen && solarUi.panel && solarUi.updatedLabel;
}

bool waveformRefreshSolarScreen()
{
  if (!solarBuilt || !solarUi.screen || !solarUi.panel || !solarUi.updatedLabel) {
    return false;
  }

  refreshSolarScreen();
  return true;
}

void waveformEnterSolarScreen()
{
  if (!solarSystemState.hasData || solarSystemState.stale ||
      static_cast<int32_t>(millis() - nextSolarRefreshAtMs) >= 0) {
    startSolarSystemFetch();
  }
}

void waveformLeaveSolarScreen()
{
}

void waveformTickSolarScreen(uint32_t nowMs)
{
  (void)nowMs;
  refreshSolarScreen();
}
