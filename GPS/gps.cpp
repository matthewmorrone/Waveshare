#include "screen_constants.h"
#include "screen_manager.h"
#ifdef SCREEN_GPS
#include "wifi_manager.h"
#include "screen_callbacks.h"
#include "geo_state.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

extern lv_obj_t *screenRoots[];
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
void scheduleScreenRefresh(ScreenId id);
bool otaUpdateInProgress();
bool lightSleepActive();
String weatherUpdatedLabel();
extern size_t currentScreenIndex;
void refreshGpsScreen();

struct GeoUi {
  lv_obj_t *screen = nullptr;
  lv_obj_t *titleLabel = nullptr;
  lv_obj_t *ipLabel = nullptr;
  lv_obj_t *locationLabel = nullptr;
  lv_obj_t *detailLabel = nullptr;
  lv_obj_t *updatedLabel = nullptr;
};

GeoState geoState;
GeoUi geoUi;
bool geoBuilt = false;
bool geoFetchInProgress = false;
uint32_t nextGpsRefreshAtMs = 0;

namespace
{
const ScreenModule kModule = {
    ScreenId::Gps,
    "GPS",
    waveformBuildGpsScreen,
    waveformRefreshGpsScreen,
    waveformEnterGpsScreen,
    waveformLeaveGpsScreen,
    waveformTickGpsScreen,
    waveformGpsScreenRoot,
};
} // namespace

const ScreenModule &gpsScreenModule()
{
  return kModule;
}


void setGpsUnavailableState(const char *updatedLabel)
{
  geoState.hasData = false;
  geoState.stale = true;
  geoState.ip = "--";
  geoState.countryCode = "";
  geoState.countryName = "";
  geoState.regionCode = "";
  geoState.regionName = "";
  geoState.city = "";
  geoState.zipCode = "";
  geoState.timeZone = "";
  geoState.latitude = 0.0f;
  geoState.longitude = 0.0f;
  geoState.metroCode = 0;
  geoState.updated = updatedLabel;
}

bool fetchGeoData()
{
  if (!networkIsOnline()) {
    return false;
  }

  geoFetchInProgress = true;
  Serial.println("Fetching geo IP...");

  HTTPClient http;
  http.setConnectTimeout(kGeoFetchTimeoutMs);
  http.setTimeout(kGeoFetchTimeoutMs);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  bool success = false;
  if (http.begin("http://208.95.112.1/json/")) {
    int statusCode = http.GET();
    if (statusCode == HTTP_CODE_OK) {
      String responseBody = http.getString();
      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, responseBody);
      if (!error) {
        geoState.hasData = true;
        geoState.stale = false;
        geoState.ip = String(static_cast<const char *>(doc["query"] | "--"));
        geoState.countryCode = String(static_cast<const char *>(doc["countryCode"] | ""));
        geoState.countryName = String(static_cast<const char *>(doc["country"] | ""));
        geoState.regionCode = String(static_cast<const char *>(doc["region"] | ""));
        geoState.regionName = String(static_cast<const char *>(doc["regionName"] | ""));
        geoState.city = String(static_cast<const char *>(doc["city"] | ""));
        geoState.zipCode = String(static_cast<const char *>(doc["zip"] | ""));
        geoState.timeZone = String(static_cast<const char *>(doc["timezone"] | ""));
        geoState.latitude = doc["lat"] | 0.0f;
        geoState.longitude = doc["lon"] | 0.0f;
        geoState.metroCode = 0;
        geoState.updated = weatherUpdatedLabel();
        nextGpsRefreshAtMs = millis() + kGeoRefreshIntervalMs;
        success = true;
      } else {
        Serial.printf("Geo JSON parse failed: %s\n", error.c_str());
      }
    } else {
      String responseBody = http.getString();
      Serial.printf("Geo request failed: HTTP %d\n", statusCode);
      if (responseBody.length() > 0) {
        Serial.printf("Geo response body: %s\n", responseBody.c_str());
      }
    }
    http.end();
  } else {
    Serial.println("Geo request setup failed");
  }

  if (!success) {
    if (geoState.hasData) {
      geoState.stale = true;
      geoState.updated = networkIsOnline() ? "Retrying with cached geodata" : "Offline - cached geodata";
    } else {
      setGpsUnavailableState(networkIsOnline() ? "Retrying shortly" : "Waiting for Wi-Fi");
    }
    nextGpsRefreshAtMs = millis() + kGeoRetryIntervalMs;
  }

  geoFetchInProgress = false;
  return success;
}

void updateGps()
{
  if (!networkIsOnline()) {
    if (geoState.hasData && !geoState.stale) {
      geoState.stale = true;
      geoState.updated = "Offline - cached geodata";
    }
    return;
  }

  if (geoFetchInProgress || otaUpdateInProgress() || lightSleepActive()) {
    return;
  }

  if (static_cast<ScreenId>(currentScreenIndex) != ScreenId::Gps) {
    return;
  }

  if (static_cast<int32_t>(millis() - nextGpsRefreshAtMs) >= 0) {
    fetchGeoData();
    refreshGpsScreen();
  }
}

String formatGeoCoordinate(float value)
{
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%.4f", static_cast<double>(value));
  return String(buffer);
}

lv_obj_t *buildGeoScreen()
{
  geoUi.screen = lv_obj_create(nullptr);
  applyRootStyle(geoUi.screen);

  geoUi.titleLabel = lv_label_create(geoUi.screen);
  lv_obj_set_width(geoUi.titleLabel, LCD_WIDTH - 40);
  lv_obj_set_style_text_font(geoUi.titleLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(geoUi.titleLabel, lvColor(164, 174, 188), 0);
  lv_obj_set_style_text_align(geoUi.titleLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(geoUi.titleLabel, "GPS");
  lv_obj_align(geoUi.titleLabel, LV_ALIGN_TOP_MID, 0, kGeoTitleY);

  geoUi.ipLabel = lv_label_create(geoUi.screen);
  lv_obj_set_width(geoUi.ipLabel, LCD_WIDTH - 40);
  lv_obj_set_style_text_font(geoUi.ipLabel, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(geoUi.ipLabel, lvColor(248, 250, 252), 0);
  lv_obj_set_style_text_align(geoUi.ipLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(geoUi.ipLabel, "--");
  lv_obj_align(geoUi.ipLabel, LV_ALIGN_TOP_MID, 0, kGeoIpY);

  geoUi.locationLabel = lv_label_create(geoUi.screen);
  lv_obj_set_width(geoUi.locationLabel, LCD_WIDTH - 48);
  lv_obj_set_style_text_font(geoUi.locationLabel, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(geoUi.locationLabel, lvColor(232, 238, 247), 0);
  lv_obj_set_style_text_align(geoUi.locationLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(geoUi.locationLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(geoUi.locationLabel, "Waiting for Wi-Fi");
  lv_obj_align(geoUi.locationLabel, LV_ALIGN_TOP_MID, 0, kGeoLocationY);

  geoUi.detailLabel = lv_label_create(geoUi.screen);
  lv_obj_set_width(geoUi.detailLabel, LCD_WIDTH - 52);
  lv_obj_set_style_text_font(geoUi.detailLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(geoUi.detailLabel, lvColor(164, 174, 188), 0);
  lv_obj_set_style_text_align(geoUi.detailLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(geoUi.detailLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(geoUi.detailLabel, "");
  lv_obj_align(geoUi.detailLabel, LV_ALIGN_TOP_MID, 0, kGeoDetailY);

  geoUi.updatedLabel = lv_label_create(geoUi.screen);
  lv_obj_set_width(geoUi.updatedLabel, LCD_WIDTH - 56);
  lv_obj_set_style_text_font(geoUi.updatedLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(geoUi.updatedLabel, lvColor(112, 124, 140), 0);
  lv_obj_set_style_text_align(geoUi.updatedLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(geoUi.updatedLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(geoUi.updatedLabel, "");
  lv_obj_align(geoUi.updatedLabel, LV_ALIGN_BOTTOM_MID, 0, -24);

  geoBuilt = true;
  return geoUi.screen;
}

void buildGpsScreenRoot()
{
  screenRoots[static_cast<size_t>(ScreenId::Gps)] = buildGeoScreen();
}

void refreshGpsScreen()
{
  if (!geoBuilt || !geoUi.screen || !geoUi.ipLabel || !geoUi.locationLabel || !geoUi.detailLabel || !geoUi.updatedLabel) {
    return;
  }

  String locationText;
  String detailText;
  String updatedText = geoState.updated;

  if (geoState.hasData) {
    if (geoState.city.length() > 0) {
      locationText = geoState.city;
    }

    String regionCountry;
    if (geoState.regionName.length() > 0) {
      regionCountry = geoState.regionName;
    }
    if (geoState.countryName.length() > 0) {
      if (regionCountry.length() > 0) {
        regionCountry += "\n";
      }
      regionCountry += geoState.countryName;
    }
    if (regionCountry.length() > 0) {
      if (locationText.length() > 0) {
        locationText += "\n";
      }
      locationText += regionCountry;
    }
    if (locationText.length() == 0) {
      locationText = "Location unavailable";
    }

    detailText = "ZIP  ";
    detailText += geoState.zipCode.length() > 0 ? geoState.zipCode : "--";
    detailText += "\nTZ  ";
    detailText += geoState.timeZone.length() > 0 ? geoState.timeZone : "--";
    detailText += "\nCodes  ";
    detailText += geoState.countryCode.length() > 0 ? geoState.countryCode : "--";
    detailText += " / ";
    detailText += geoState.regionCode.length() > 0 ? geoState.regionCode : "--";
    detailText += "\nCoords  ";
    detailText += formatGeoCoordinate(geoState.latitude);
    detailText += ", ";
    detailText += formatGeoCoordinate(geoState.longitude);
    if (geoState.metroCode > 0) {
      detailText += "\nMetro  ";
      detailText += String(geoState.metroCode);
    }

    if (geoState.stale) {
      updatedText += "  cached";
    }
  } else {
    locationText = networkIsOnline() ? "Public IP geodata unavailable" : "Waiting for Wi-Fi";
    detailText = "Source\nip-api.com via 208.95.112.1\n\nShows the public network IP and its geolocation.";
  }

  lv_label_set_text(geoUi.ipLabel, geoState.hasData ? geoState.ip.c_str() : "--");
  lv_label_set_text(geoUi.locationLabel, locationText.c_str());
  lv_label_set_text(geoUi.detailLabel, detailText.c_str());
  lv_label_set_text(geoUi.updatedLabel, updatedText.c_str());
}

lv_obj_t *waveformGpsScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Gps)];
}

bool waveformBuildGpsScreen()
{
  if (!waveformGpsScreenRoot()) {
    buildGpsScreenRoot();
  }

  return geoBuilt && waveformGpsScreenRoot() && geoUi.screen && geoUi.ipLabel && geoUi.locationLabel &&
         geoUi.detailLabel && geoUi.updatedLabel;
}

bool waveformRefreshGpsScreen()
{
  if (!geoBuilt || !geoUi.screen || !geoUi.ipLabel || !geoUi.locationLabel || !geoUi.detailLabel || !geoUi.updatedLabel) {
    return false;
  }

  refreshGpsScreen();
  return true;
}

void waveformEnterGpsScreen()
{
  if ((!geoState.hasData || geoState.stale || static_cast<int32_t>(millis() - nextGpsRefreshAtMs) >= 0) &&
      networkIsOnline() && !geoFetchInProgress && !otaUpdateInProgress() && !lightSleepActive()) {
    nextGpsRefreshAtMs = 0;
  }
}

void waveformLeaveGpsScreen()
{
}

void waveformTickGpsScreen(uint32_t nowMs)
{
  (void)nowMs;
  refreshGpsScreen();
}

#endif // SCREEN_GPS
