#pragma once

#include <Arduino.h>

struct GeoState {
  bool hasData = false;
  bool stale = true;
  String ip = "--";
  String countryCode = "";
  String countryName = "";
  String regionCode = "";
  String regionName = "";
  String city = "";
  String zipCode = "";
  String timeZone = "";
  float latitude = 0.0f;
  float longitude = 0.0f;
  int metroCode = 0;
  String updated = "Waiting for Wi-Fi";
};
