#include "screen_constants.h"
#include "screen_manager.h"
#ifdef SCREEN_PLANETS
#include "screen_callbacks.h"
#include "time_utils.h"
#include <math.h>

extern lv_obj_t *screenRoots[];
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
float compressedOrbitRadius(float distanceAu);
bool otaUpdateInProgress();
bool lightSleepActive();
extern int currentScreenIndex;
void refreshPlanetsScreen();

namespace
{
  const ScreenModule kModule = {
    ScreenId::Planets,
    "Planets",
    waveformBuildPlanetsScreen,
    waveformRefreshPlanetsScreen,
    waveformEnterPlanetsScreen,
    waveformLeavePlanetsScreen,
    waveformTickPlanetsScreen,
    waveformPlanetsScreenRoot,
  };
} // namespace

const ScreenModule &planetsScreenModule()
{
  return kModule;
}


struct SolarPlanetDefinition
{
  const char *name;
  const char *shortLabel;
  float orbitRadiusAu;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

struct PlanetApproximation
{
  double a0;
  double aRate;
  double e0;
  double eRate;
  double i0;
  double iRate;
  double l0;
  double lRate;
  double peri0;
  double periRate;
  double node0;
  double nodeRate;
  double b;
  double c;
  double s;
  double f;
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
  String updated = "Waiting for time";
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
  {"Mercury", "Me", 0.387f, 180, 180, 170},
  {"Venus", "Ve", 0.723f, 214, 174, 104},
  {"Earth", "Ea", 1.000f, 86, 160, 255},
  {"Mars", "Ma", 1.524f, 220, 110, 82},
  {"Jupiter", "Ju", 5.203f, 210, 174, 126},
  {"Saturn", "Sa", 9.537f, 214, 192, 142},
  {"Uranus", "Ur", 19.191f, 126, 214, 214},
  {"Neptune", "Ne", 30.070f, 92, 132, 255},
};

constexpr PlanetApproximation kPlanetApproximations[kSolarPlanetCount] = {
  { 0.38709927,  0.00000037, 0.20563593,  0.00001906,  7.00497902, -0.00594749, 252.25032350, 149472.67411175,  77.45779628,  0.16047689,  48.33076593, -0.12534081, 0.0, 0.0, 0.0, 0.0},
  { 0.72333566,  0.00000390, 0.00677672, -0.00004107,  3.39467605, -0.00078890, 181.97909950,  58517.81538729, 131.60246718,  0.00268329,  76.67984255, -0.27769418, 0.0, 0.0, 0.0, 0.0},
  { 1.00000261,  0.00000562, 0.01671123, -0.00004392, -0.00001531, -0.01294668, 100.46457166,  35999.37244981, 102.93768193,  0.32327364,   0.0,         0.0,        0.0, 0.0, 0.0, 0.0},
  { 1.52371034,  0.00001847, 0.09339410,  0.00007882,  1.84969142, -0.00813131,  -4.55343205,  19140.30268499, -23.94362959,  0.44441088,  49.55953891, -0.29257343, 0.0, 0.0, 0.0, 0.0},
  { 5.20288700, -0.00011607, 0.04838624, -0.00013253,  1.30439695, -0.00183714,  34.39644051,   3034.74612775,  14.72847983,  0.21252668, 100.47390909,  0.20469106, 0.0, 0.0, 0.0, 0.0},
  { 9.53667594, -0.00125060, 0.05386179, -0.00050991,  2.48599187,  0.00193609,  49.95424423,   1222.49362201,  92.59887831, -0.41897216, 113.66242448, -0.28867794, 0.0, 0.0, 0.0, 0.0},
  {19.18916464, -0.00196176, 0.04725744, -0.00004397,  0.77263783, -0.00242939, 313.23810451,    428.48202785, 170.95427630,  0.40805281,  74.01692503,  0.04240589, 0.0, 0.0, 0.0, 0.0},
  {30.06992276,  0.00026291, 0.00859048,  0.00005105,  1.77004347,  0.00035372, -55.12002969,    218.45945325,  44.96476227, -0.32241464, 131.78422574, -0.00508664, 0.0, 0.0, 0.0, 0.0},
};


SolarSystemState solarSystemState;
SolarUi solarUi;
bool solarBuilt = false;
uint32_t nextPlanetsRefreshAtMs = 0;

void setPlanetsUnavailableState(const char *updatedLabel)
{
  solarSystemState.hasData = false;
  solarSystemState.stale = true;
  solarSystemState.updated = updatedLabel;
  for (size_t i = 0; i < kSolarPlanetCount; ++i) {
    solarSystemState.planets[i] = {};
  }
}

double normalizeDegrees(double degrees)
{
  double normalized = fmod(degrees, 360.0);
  if (normalized < -180.0) {
    normalized += 360.0;
  }
  if (normalized > 180.0) {
    normalized -= 360.0;
  }
  return normalized;
}

double degreesToRadiansLocal(double degrees)
{
  return degrees * DEG_TO_RAD;
}

double julianDateFromUnixLocal(time_t unixTime)
{
  return 2440587.5 + (static_cast<double>(unixTime) / 86400.0);
}

double solveEccentricAnomalyDegrees(double meanAnomalyDegrees, double eccentricity)
{
  double eDegrees = eccentricity * 57.29577951308232;
  double eccentricAnomaly = meanAnomalyDegrees + (eDegrees * sin(degreesToRadiansLocal(meanAnomalyDegrees)));

  for (int iteration = 0; iteration < 8; ++iteration) {
    double deltaM = meanAnomalyDegrees -
                    (eccentricAnomaly - (eDegrees * sin(degreesToRadiansLocal(eccentricAnomaly))));
    double deltaE = deltaM / (1.0 - (eccentricity * cos(degreesToRadiansLocal(eccentricAnomaly))));
    eccentricAnomaly += deltaE;
    if (fabs(deltaE) <= 1e-6) {
      break;
    }
  }
  return eccentricAnomaly;
}

bool computeApproxPlanetVector(const PlanetApproximation &elements, time_t unixTime, float &xAu, float &yAu, float &zAu)
{
  if (unixTime <= 0) {
    return false;
  }

  double julianDate = julianDateFromUnixLocal(unixTime);
  double centuries = (julianDate - 2451545.0) / 36525.0;

  double a = elements.a0 + (elements.aRate * centuries);
  double e = elements.e0 + (elements.eRate * centuries);
  double inclination = elements.i0 + (elements.iRate * centuries);
  double meanLongitude = elements.l0 + (elements.lRate * centuries);
  double longitudePerihelion = elements.peri0 + (elements.periRate * centuries);
  double ascendingNode = elements.node0 + (elements.nodeRate * centuries);
  double argumentPerihelion = longitudePerihelion - ascendingNode;
  double meanAnomaly = meanLongitude - longitudePerihelion;
  meanAnomaly += (elements.b * centuries * centuries) +
                 (elements.c * cos(degreesToRadiansLocal(elements.f * centuries))) +
                 (elements.s * sin(degreesToRadiansLocal(elements.f * centuries)));
  meanAnomaly = normalizeDegrees(meanAnomaly);

  double eccentricAnomaly = solveEccentricAnomalyDegrees(meanAnomaly, e);
  double eccentricAnomalyRadians = degreesToRadiansLocal(eccentricAnomaly);
  double orbitalX = a * (cos(eccentricAnomalyRadians) - e);
  double orbitalY = a * sqrt(1.0 - (e * e)) * sin(eccentricAnomalyRadians);

  double omegaRadians = degreesToRadiansLocal(argumentPerihelion);
  double nodeRadians = degreesToRadiansLocal(ascendingNode);
  double inclinationRadians = degreesToRadiansLocal(inclination);

  double cosOmega = cos(omegaRadians);
  double sinOmega = sin(omegaRadians);
  double cosNode = cos(nodeRadians);
  double sinNode = sin(nodeRadians);
  double cosInclination = cos(inclinationRadians);
  double sinInclination = sin(inclinationRadians);

  double eclipticX = ((cosOmega * cosNode) - (sinOmega * sinNode * cosInclination)) * orbitalX +
                     ((-sinOmega * cosNode) - (cosOmega * sinNode * cosInclination)) * orbitalY;
  double eclipticY = ((cosOmega * sinNode) + (sinOmega * cosNode * cosInclination)) * orbitalX +
                     ((-sinOmega * sinNode) + (cosOmega * cosNode * cosInclination)) * orbitalY;
  double eclipticZ = (sinOmega * sinInclination) * orbitalX +
                     (cosOmega * sinInclination) * orbitalY;

  xAu = static_cast<float>(eclipticX);
  yAu = static_cast<float>(eclipticY);
  zAu = static_cast<float>(eclipticZ);
  return true;
}

bool computeSolarSystemState()
{
  time_t now = waveform::effectiveNow();
  if (now < waveform::minimumReasonableEpoch()) {
    Serial.println("Planets: time is not valid yet");
    return false;
  }

  Serial.printf("Planets: computing positions at epoch %ld\n", static_cast<long>(now));

  for (size_t i = 0; i < kSolarPlanetCount; ++i) {
    float xAu = 0.0f, yAu = 0.0f, zAu = 0.0f;
    if (!computeApproxPlanetVector(kPlanetApproximations[i], now, xAu, yAu, zAu)) {
      Serial.printf("Planets: compute failed for %s\n", kSolarPlanets[i].name);
      return false;
    }
    solarSystemState.planets[i].valid = true;
    solarSystemState.planets[i].xAu = xAu;
    solarSystemState.planets[i].yAu = yAu;
    solarSystemState.planets[i].zAu = zAu;
    Serial.printf(
      "Planets: %s x=%.3f y=%.3f z=%.3f\n",
      kSolarPlanets[i].name,
      static_cast<double>(xAu),
      static_cast<double>(yAu),
      static_cast<double>(zAu)
    );
  }

  solarSystemState.hasData = true;
  solarSystemState.stale = false;
  // solarSystemState.updated = weatherUpdatedLabel() + "  Approx";
  nextPlanetsRefreshAtMs = millis() + kSolarRefreshIntervalMs;
  return true;
}

void startSolarSystemFetch()
{
  if (waveform::effectiveNow() < waveform::minimumReasonableEpoch()) {
    setPlanetsUnavailableState("Waiting for time");
    return;
  }

  if (!computeSolarSystemState()) {
    if (solarSystemState.hasData) {
      solarSystemState.stale = true;
      solarSystemState.updated = "Retrying with cached solar data";
    } else {
      setPlanetsUnavailableState("Retrying shortly");
    }
    nextPlanetsRefreshAtMs = millis() + kSolarRetryIntervalMs;
  }
  refreshPlanetsScreen();
}

void updatePlanets()
{
  ScreenId currentScreen = static_cast<ScreenId>(currentScreenIndex);
  if (currentScreen != ScreenId::Planets) {
    return;
  }

  if (otaUpdateInProgress() || lightSleepActive()) {
    return;
  }

  if (waveform::effectiveNow() < waveform::minimumReasonableEpoch()) {
    if (!solarSystemState.hasData) {
      setPlanetsUnavailableState("Waiting for time");
      refreshPlanetsScreen();
    }
    return;
  }

  if (!solarSystemState.hasData || solarSystemState.stale || static_cast<int32_t>(millis() - nextPlanetsRefreshAtMs) >= 0) {
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
  lv_label_set_text(solarUi.titleLabel, "Planets");
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
    lv_obj_add_flag(solarUi.planetDots[i], LV_OBJ_FLAG_HIDDEN);

    solarUi.planetLabels[i] = lv_label_create(solarUi.panel);
    lv_obj_set_style_text_font(solarUi.planetLabels[i], &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(solarUi.planetLabels[i], lvColor(228, 234, 242), 0);
    lv_label_set_text(solarUi.planetLabels[i], kSolarPlanets[i].shortLabel);
    lv_obj_add_flag(solarUi.planetLabels[i], LV_OBJ_FLAG_HIDDEN);
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
  screenRoots[static_cast<size_t>(ScreenId::Planets)] = buildSolarScreen();
}

void refreshPlanetsScreen()
{
  if (!solarBuilt || !solarUi.panel || !solarUi.updatedLabel || !solarUi.sourceLabel || !solarUi.sun) {
    return;
  }

  lv_obj_align(solarUi.sun, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(solarUi.sourceLabel, "Approx heliocentric positions");
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
      Serial.printf("Planets: %s invalid, hiding dot\n", kSolarPlanets[i].name);
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
    Serial.printf("Planets: %s r=%.2f screen=(%.1f, %.1f)\n",
                  kSolarPlanets[i].name,
                  static_cast<double>(displayRadius),
                  static_cast<double>(localX),
                  static_cast<double>(localY));

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

lv_obj_t *waveformPlanetsScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Planets)];
}

bool waveformBuildPlanetsScreen()
{
  if (!waveformPlanetsScreenRoot()) {
    buildSolarScreenRoot();
  }

  return solarBuilt && waveformPlanetsScreenRoot() && solarUi.screen && solarUi.panel && solarUi.updatedLabel;
}

bool waveformRefreshPlanetsScreen()
{
  if (!solarBuilt || !solarUi.screen || !solarUi.panel || !solarUi.updatedLabel) {
    return false;
  }

  refreshPlanetsScreen();
  return true;
}

void waveformEnterPlanetsScreen()
{
  if (!solarSystemState.hasData || solarSystemState.stale ||
      static_cast<int32_t>(millis() - nextPlanetsRefreshAtMs) >= 0) {
    nextPlanetsRefreshAtMs = 0;
  }
}

void waveformLeavePlanetsScreen()
{
}

void waveformTickPlanetsScreen(uint32_t nowMs)
{
  (void)nowMs;
  refreshPlanetsScreen();
}

#endif // SCREEN_PLANETS
