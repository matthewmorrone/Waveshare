#pragma once

// Primary Wi-Fi network.
#define WIFI_SSID_PRIMARY "The Y-Files"
#define WIFI_PASSWORD_PRIMARY "quartz21wrench10crown"

// Fallback Wi-Fi network.
#define WIFI_SSID_FALLBACK "Monoceros"
#define WIFI_PASSWORD_FALLBACK "12345678"

// Timezone and NTP configuration.
#define TIMEZONE_POSIX "EST5EDT,M3.2.0,M11.1.0"
#define NTP_SERVER_PRIMARY "pool.ntp.org"
#define NTP_SERVER_SECONDARY "time.nist.gov"

// Weather configuration.
#define WEATHER_LOCATION_LABEL "New York"
#define WEATHER_LATITUDE 40.7128
#define WEATHER_LONGITUDE -74.0060

// The device will advertise this name via mDNS for OTA uploads.
#define OTA_HOSTNAME "waveform"

// Leave empty to disable OTA password protection.
#define OTA_PASSWORD ""
