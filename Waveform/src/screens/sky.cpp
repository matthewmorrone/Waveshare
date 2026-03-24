#include "config/ota_config.h"
#include "config/screen_constants.h"
#include "core/screen_manager.h"
#include "modules/weather_module.h"
#include "screens/screen_callbacks.h"
#include "state/geo_state.h"

extern lv_obj_t *screenRoots[];
extern GeoState geoState;
bool hasValidTime();
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
float degreesToRadians(float degrees);
bool equatorialToHorizontal(float raHours, float decDegrees, float latitudeDegrees, float longitudeDegrees,
                             time_t utcTime, float &altitudeDegrees, float &azimuthDegrees);

namespace
{
const ScreenModule kModule = {
    ScreenId::Sky,
    "Sky",
    waveformBuildSkyScreen,
    waveformRefreshSkyScreen,
    waveformEnterSkyScreen,
    waveformLeaveSkyScreen,
    waveformTickSkyScreen,
    waveformSkyScreenRoot,
};
} // namespace

const ScreenModule &skyScreenModule()
{
  return kModule;
}


struct SkyStarDefinition
{
  const char *name;
  float raHours;
  float decDegrees;
  float magnitude;
};

struct SkyUi
{
  lv_obj_t *screen = nullptr;
  lv_obj_t *titleLabel = nullptr;
  lv_obj_t *panel = nullptr;
  lv_obj_t *locationLabel = nullptr;
  lv_obj_t *updatedLabel = nullptr;
  lv_obj_t *altitudeRings[2] = {};
  lv_obj_t *compassLabels[4] = {};
  lv_obj_t *starDots[kSkyStarCount] = {};
  lv_obj_t *starLabels[kSkyStarCount] = {};
};



constexpr SkyStarDefinition kSkyStars[kSkyStarCount] = {
    {"Sirius", 6.7525f, -16.7161f, -1.46f},
    {"Canopus", 6.3992f, -52.6957f, -0.72f},
    {"Arcturus", 14.2610f, 19.1825f, -0.05f},
    {"Vega", 18.6156f, 38.7837f, 0.03f},
    {"Capella", 5.2782f, 45.9980f, 0.08f},
    {"Rigel", 5.2423f, -8.2016f, 0.13f},
    {"Procyon", 7.6550f, 5.2250f, 0.38f},
    {"Betelgeuse", 5.9195f, 7.4070f, 0.45f},
    {"Achernar", 1.6286f, -57.2368f, 0.46f},
    {"Hadar", 14.0637f, -60.3730f, 0.61f},
    {"Altair", 19.8464f, 8.8683f, 0.77f},
    {"Acrux", 12.4433f, -63.0991f, 0.76f},
    {"Aldebaran", 4.5987f, 16.5093f, 0.85f},
    {"Spica", 13.4199f, -11.1614f, 0.98f},
    {"Antares", 16.4901f, -26.4319f, 1.06f},
    {"Pollux", 7.7553f, 28.0262f, 1.14f},
    {"Fomalhaut", 22.9608f, -29.6222f, 1.16f},
    {"Deneb", 20.6905f, 45.2803f, 1.25f},
    {"Regulus", 10.1395f, 11.9672f, 1.35f},
    {"Polaris", 2.5303f, 89.2641f, 1.98f},
};


float astronomyLatitude()
{
  if (geoState.hasData && fabsf(geoState.latitude) > 0.001f) {
    return geoState.latitude;
  }
  return WEATHER_LATITUDE;
}

float astronomyLongitude()
{
  if (geoState.hasData && fabsf(geoState.longitude) > 0.001f) {
    return geoState.longitude;
  }
  return WEATHER_LONGITUDE;
}

String astronomyLocationLabel()
{
  if (geoState.hasData) {
    String label;
    if (geoState.city.length() > 0) {
      label += geoState.city;
    }
    if (geoState.regionName.length() > 0) {
      if (label.length() > 0) {
        label += ", ";
      }
      label += geoState.regionName;
    }
    if (label.length() == 0 && geoState.countryName.length() > 0) {
      label = geoState.countryName;
    }
    if (label.length() > 0) {
      return label;
    }
  }
  return WEATHER_LOCATION_LABEL;
}


SkyUi skyUi;
bool skyBuilt = false;


lv_obj_t *buildSkyScreen()
{
  skyUi.screen = lv_obj_create(nullptr);
  applyRootStyle(skyUi.screen);

  skyUi.titleLabel = lv_label_create(skyUi.screen);
  lv_obj_set_width(skyUi.titleLabel, LCD_WIDTH - 40);
  lv_obj_set_style_text_font(skyUi.titleLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(skyUi.titleLabel, lvColor(248, 250, 252), 0);
  lv_obj_set_style_text_align(skyUi.titleLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(skyUi.titleLabel, "Sky Map");
  lv_obj_align(skyUi.titleLabel, LV_ALIGN_TOP_MID, 0, kSkyTitleY);

  skyUi.panel = lv_obj_create(skyUi.screen);
  lv_obj_set_size(skyUi.panel, kSkyPanelSize, kSkyPanelSize);
  lv_obj_align(skyUi.panel, LV_ALIGN_TOP_MID, 0, kSkyPanelY);
  lv_obj_set_style_radius(skyUi.panel, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(skyUi.panel, lvColor(2, 8, 18), 0);
  lv_obj_set_style_bg_opa(skyUi.panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(skyUi.panel, 1, 0);
  lv_obj_set_style_border_color(skyUi.panel, lvColor(32, 46, 64), 0);
  lv_obj_set_style_pad_all(skyUi.panel, 0, 0);
  lv_obj_clear_flag(skyUi.panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(skyUi.panel, LV_OBJ_FLAG_CLICKABLE);

  for (size_t i = 0; i < 2; ++i) {
    skyUi.altitudeRings[i] = lv_obj_create(skyUi.panel);
    lv_obj_set_style_radius(skyUi.altitudeRings[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(skyUi.altitudeRings[i], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(skyUi.altitudeRings[i], 1, 0);
    lv_obj_set_style_border_color(skyUi.altitudeRings[i], lvColor(18, 30, 46), 0);
    lv_obj_set_style_pad_all(skyUi.altitudeRings[i], 0, 0);
    lv_obj_clear_flag(skyUi.altitudeRings[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(skyUi.altitudeRings[i], LV_OBJ_FLAG_CLICKABLE);
  }

  const char *compass[4] = {"N", "E", "S", "W"};
  for (size_t i = 0; i < 4; ++i) {
    skyUi.compassLabels[i] = lv_label_create(skyUi.panel);
    lv_obj_set_style_text_font(skyUi.compassLabels[i], &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(skyUi.compassLabels[i], lvColor(124, 146, 170), 0);
    lv_label_set_text(skyUi.compassLabels[i], compass[i]);
  }

  for (size_t i = 0; i < kSkyStarCount; ++i) {
    skyUi.starDots[i] = lv_obj_create(skyUi.panel);
    lv_obj_set_style_radius(skyUi.starDots[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(skyUi.starDots[i], 0, 0);
    lv_obj_set_style_pad_all(skyUi.starDots[i], 0, 0);
    lv_obj_clear_flag(skyUi.starDots[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(skyUi.starDots[i], LV_OBJ_FLAG_CLICKABLE);

    skyUi.starLabels[i] = lv_label_create(skyUi.panel);
    lv_obj_set_style_text_font(skyUi.starLabels[i], &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(skyUi.starLabels[i], lvColor(214, 224, 236), 0);
    lv_label_set_text(skyUi.starLabels[i], kSkyStars[i].name);
  }

  skyUi.locationLabel = lv_label_create(skyUi.screen);
  lv_obj_set_width(skyUi.locationLabel, LCD_WIDTH - 52);
  lv_obj_set_style_text_font(skyUi.locationLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(skyUi.locationLabel, lvColor(164, 174, 188), 0);
  lv_obj_set_style_text_align(skyUi.locationLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(skyUi.locationLabel, "");
  lv_obj_align(skyUi.locationLabel, LV_ALIGN_BOTTOM_MID, 0, -40);

  skyUi.updatedLabel = lv_label_create(skyUi.screen);
  lv_obj_set_width(skyUi.updatedLabel, LCD_WIDTH - 56);
  lv_obj_set_style_text_font(skyUi.updatedLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(skyUi.updatedLabel, lvColor(112, 124, 140), 0);
  lv_obj_set_style_text_align(skyUi.updatedLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(skyUi.updatedLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(skyUi.updatedLabel, "");
  lv_obj_align(skyUi.updatedLabel, LV_ALIGN_BOTTOM_MID, 0, -16);

  skyBuilt = true;
  return skyUi.screen;
}

void buildSkyScreenRoot()
{
  screenRoots[static_cast<size_t>(ScreenId::Sky)] = buildSkyScreen();
}

void refreshSkyScreen()
{
  if (!skyBuilt || !skyUi.panel || !skyUi.locationLabel || !skyUi.updatedLabel) {
    return;
  }

  int center = kSkyPanelSize / 2;
  int radius = (kSkyPanelSize / 2) - 4;
  int ringSizes[2] = {static_cast<int>(roundf(radius * 0.67f)) * 2,
                      static_cast<int>(roundf(radius * 0.34f)) * 2};

  for (size_t i = 0; i < 2; ++i) {
    lv_obj_set_size(skyUi.altitudeRings[i], ringSizes[i], ringSizes[i]);
    lv_obj_align(skyUi.altitudeRings[i], LV_ALIGN_CENTER, 0, 0);
  }

  lv_obj_set_pos(skyUi.compassLabels[0], center - 5, 6);
  lv_obj_set_pos(skyUi.compassLabels[1], kSkyPanelSize - 16, center - 6);
  lv_obj_set_pos(skyUi.compassLabels[2], center - 5, kSkyPanelSize - 18);
  lv_obj_set_pos(skyUi.compassLabels[3], 6, center - 6);

  if (!hasValidTime()) {
    lv_label_set_text(skyUi.locationLabel, astronomyLocationLabel().c_str());
    lv_label_set_text(skyUi.updatedLabel, "Waiting for time");
    for (size_t i = 0; i < kSkyStarCount; ++i) {
      lv_obj_add_flag(skyUi.starDots[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(skyUi.starLabels[i], LV_OBJ_FLAG_HIDDEN);
    }
    return;
  }

  float latitude = astronomyLatitude();
  float longitude = astronomyLongitude();
  time_t now = time(nullptr);
  char latitudeCardinal = latitude >= 0.0f ? 'N' : 'S';
  char longitudeCardinal = longitude >= 0.0f ? 'E' : 'W';
  String locationText = astronomyLocationLabel() + "  " + String(static_cast<int>(roundf(fabsf(latitude)))) +
                        latitudeCardinal + " " +
                        String(static_cast<int>(roundf(fabsf(longitude)))) + longitudeCardinal;
  lv_label_set_text(skyUi.locationLabel, locationText.c_str());
  lv_label_set_text(skyUi.updatedLabel, weatherUpdatedLabel().c_str());

  for (size_t i = 0; i < kSkyStarCount; ++i) {
    float altitude = 0.0f;
    float azimuth = 0.0f;
    equatorialToHorizontal(kSkyStars[i].raHours,
                           kSkyStars[i].decDegrees,
                           latitude,
                           longitude,
                           now,
                           altitude,
                           azimuth);

    if (altitude <= 0.0f) {
      lv_obj_add_flag(skyUi.starDots[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(skyUi.starLabels[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }

    float radial = ((90.0f - altitude) / 90.0f) * static_cast<float>(radius - 8);
    float azimuthRadians = degreesToRadians(azimuth);
    float x = static_cast<float>(center) + (sinf(azimuthRadians) * radial);
    float y = static_cast<float>(center) - (cosf(azimuthRadians) * radial);
    int starSize = constrain(static_cast<int>(roundf(6.0f - kSkyStars[i].magnitude)), 2, 7);
    lv_color_t starColor = kSkyStars[i].magnitude < 0.2f ? lvColor(255, 248, 222) : lvColor(228, 238, 255);

    lv_obj_set_size(skyUi.starDots[i], starSize, starSize);
    lv_obj_set_pos(skyUi.starDots[i],
                   static_cast<int>(roundf(x)) - (starSize / 2),
                   static_cast<int>(roundf(y)) - (starSize / 2));
    lv_obj_set_style_bg_color(skyUi.starDots[i], starColor, 0);
    lv_obj_set_style_bg_opa(skyUi.starDots[i], LV_OPA_COVER, 0);
    lv_obj_clear_flag(skyUi.starDots[i], LV_OBJ_FLAG_HIDDEN);

    if (kSkyStars[i].magnitude <= 0.85f || strcmp(kSkyStars[i].name, "Polaris") == 0) {
      lv_obj_set_pos(skyUi.starLabels[i], static_cast<int>(roundf(x)) + 4, static_cast<int>(roundf(y)) - 8);
      lv_obj_clear_flag(skyUi.starLabels[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(skyUi.starLabels[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

lv_obj_t *waveformSkyScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Sky)];
}

bool waveformBuildSkyScreen()
{
  if (!waveformSkyScreenRoot()) {
    buildSkyScreenRoot();
  }

  return skyBuilt && waveformSkyScreenRoot() && skyUi.screen && skyUi.panel && skyUi.updatedLabel;
}

bool waveformRefreshSkyScreen()
{
  if (!skyBuilt || !skyUi.screen || !skyUi.panel || !skyUi.updatedLabel) {
    return false;
  }

  refreshSkyScreen();
  return true;
}

void waveformEnterSkyScreen()
{
}

void waveformLeaveSkyScreen()
{
}

void waveformTickSkyScreen(uint32_t nowMs)
{
  (void)nowMs;
  refreshSkyScreen();
}
