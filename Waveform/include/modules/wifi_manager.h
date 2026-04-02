#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "screens/screen_callbacks.h"

extern const QrEntry kQrEntries[];
extern const size_t kQrEntryCount;

struct WiFiScanEntry
{
  char ssid[33] = "";
  int32_t rssi = -127;
  int32_t channel = 0;
  wifi_auth_mode_t encryption = WIFI_AUTH_OPEN;
  bool hidden = false;
  bool known = false;
};

bool networkIsOnline();
void wifiManagerConfigureKnownNetworks(const char *const *ssids, size_t count);
void wifiManagerUpdateScan(uint32_t nowMs, bool wifiEnabled, bool allowScan);
void wifiManagerRequestScan();
void wifiManagerClearScanResults();
bool wifiManagerScanInProgress();
size_t wifiManagerScanCount();
size_t wifiManagerSnapshot(WiFiScanEntry *dest, size_t maxEntries);
uint32_t wifiManagerLastScanAtMs();
bool wifiManagerHasKnownNetworks();
const char *wifiManagerEncryptionLabel(wifi_auth_mode_t mode);
