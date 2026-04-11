#include "ota_config.h"
#include "screen_manager.h"
#include "wifi_manager.h"

#include <string.h>

extern ConnectivityState connectivityState;

namespace
{
constexpr size_t kMaxKnownNetworks = 6;
constexpr size_t kMaxScanEntries = 8;
constexpr uint32_t kScanIntervalMs = 45000;
constexpr uint32_t kScanRetryMs = 12000;

char gKnownNetworks[kMaxKnownNetworks][33] = {};
size_t gKnownNetworkCount = 0;

WiFiScanEntry gScanEntries[kMaxScanEntries] = {};
size_t gScanEntryCount = 0;
bool gScanRequested = true;
bool gScanInProgress = false;
uint32_t gLastScanStartedAtMs = 0;
uint32_t gLastScanCompletedAtMs = 0;

bool isKnownNetwork(const String &ssid)
{
  if (ssid.isEmpty()) {
    return false;
  }

  for (size_t index = 0; index < gKnownNetworkCount; ++index) {
    if (ssid.equals(gKnownNetworks[index])) {
      return true;
    }
  }
  return false;
}

void clearEntries()
{
  gScanEntries[0] = WiFiScanEntry{};
  for (size_t index = 1; index < kMaxScanEntries; ++index) {
    gScanEntries[index] = WiFiScanEntry{};
  }
  gScanEntryCount = 0;
}

void sortEntries()
{
  for (size_t outer = 0; outer < gScanEntryCount; ++outer) {
    size_t best = outer;
    for (size_t inner = outer + 1; inner < gScanEntryCount; ++inner) {
      if (gScanEntries[inner].rssi > gScanEntries[best].rssi) {
        best = inner;
      }
    }
    if (best != outer) {
      WiFiScanEntry tmp = gScanEntries[outer];
      gScanEntries[outer] = gScanEntries[best];
      gScanEntries[best] = tmp;
    }
  }
}

void insertOrUpdateEntry(const String &ssid,
                         int32_t rssi,
                         int32_t channel,
                         wifi_auth_mode_t encryption)
{
  const bool hidden = ssid.isEmpty();
  for (size_t index = 0; index < gScanEntryCount; ++index) {
    if ((!hidden && strcmp(gScanEntries[index].ssid, ssid.c_str()) == 0) ||
        (hidden && gScanEntries[index].hidden && gScanEntries[index].channel == channel)) {
      if (rssi > gScanEntries[index].rssi) {
        gScanEntries[index].rssi = rssi;
        gScanEntries[index].channel = channel;
        gScanEntries[index].encryption = encryption;
        gScanEntries[index].known = isKnownNetwork(ssid);
      }
      return;
    }
  }

  size_t targetIndex = gScanEntryCount;
  if (targetIndex >= kMaxScanEntries) {
    targetIndex = kMaxScanEntries - 1;
    if (rssi <= gScanEntries[targetIndex].rssi) {
      return;
    }
  } else {
    ++gScanEntryCount;
  }

  WiFiScanEntry &entry = gScanEntries[targetIndex];
  memset(&entry, 0, sizeof(entry));
  if (!hidden) {
    snprintf(entry.ssid, sizeof(entry.ssid), "%s", ssid.c_str());
  }
  entry.rssi = rssi;
  entry.channel = channel;
  entry.encryption = encryption;
  entry.hidden = hidden;
  entry.known = isKnownNetwork(ssid);
  sortEntries();
}

void consumeScanResults(int16_t networkCount)
{
  clearEntries();
  for (int16_t index = 0; index < networkCount; ++index) {
    insertOrUpdateEntry(WiFi.SSID(index), WiFi.RSSI(index), WiFi.channel(index), WiFi.encryptionType(index));
  }
  sortEntries();
}

void beginAsyncScan(uint32_t nowMs)
{
  WiFi.scanDelete();
  int16_t result = WiFi.scanNetworks(true, true, false, 120);
  gLastScanStartedAtMs = nowMs;
  if (result == WIFI_SCAN_RUNNING) {
    gScanInProgress = true;
    return;
  }

  gScanInProgress = false;
  if (result >= 0) {
    consumeScanResults(result);
    gLastScanCompletedAtMs = nowMs;
    WiFi.scanDelete();
  } else {
    gLastScanCompletedAtMs = nowMs;
  }
}
} // namespace

// @TODO: why is this here? this is for the QR module
extern const QrEntry kQrEntries[] = {
    {"Join Wi-Fi", "WIFI:T:WPA;S:" WIFI_SSID_PRIMARY ";P:" WIFI_PASSWORD_PRIMARY ";;"}
};
extern const size_t kQrEntryCount = sizeof(kQrEntries) / sizeof(kQrEntries[0]);

bool networkIsOnline()
{
  return WiFi.getMode() != WIFI_OFF && WiFi.status() == WL_CONNECTED;
}

void wifiManagerConfigureKnownNetworks(const char *const *ssids, size_t count)
{
  gKnownNetworkCount = 0;
  for (size_t index = 0; index < count && gKnownNetworkCount < kMaxKnownNetworks; ++index) {
    if (!ssids[index] || ssids[index][0] == '\0') {
      continue;
    }
    snprintf(gKnownNetworks[gKnownNetworkCount], sizeof(gKnownNetworks[gKnownNetworkCount]), "%s", ssids[index]);
    ++gKnownNetworkCount;
  }
}

void wifiManagerUpdateScan(uint32_t nowMs, bool wifiEnabled, bool allowScan)
{
  if (!wifiEnabled) {
    gScanInProgress = false;
    return;
  }

  if (gScanInProgress) {
    int16_t status = WiFi.scanComplete();
    if (status == WIFI_SCAN_RUNNING) {
      return;
    }

    gScanInProgress = false;
    gLastScanCompletedAtMs = nowMs;
    if (status >= 0) {
      consumeScanResults(status);
    }
    WiFi.scanDelete();
  }

  if (!allowScan) {
    return;
  }

  const uint32_t elapsedSinceStart = nowMs - gLastScanStartedAtMs;
  const uint32_t targetInterval = gScanEntryCount > 0 ? kScanIntervalMs : kScanRetryMs;
  if (!gScanRequested && elapsedSinceStart < targetInterval) {
    return;
  }

  gScanRequested = false;
  beginAsyncScan(nowMs);
}

void wifiManagerRequestScan()
{
  gScanRequested = true;
}

void wifiManagerClearScanResults()
{
  clearEntries();
  WiFi.scanDelete();
  gScanInProgress = false;
}

bool wifiManagerScanInProgress()
{
  return gScanInProgress;
}

size_t wifiManagerScanCount()
{
  return gScanEntryCount;
}

size_t wifiManagerSnapshot(WiFiScanEntry *dest, size_t maxEntries)
{
  if (!dest || maxEntries == 0) {
    return 0;
  }

  size_t copyCount = gScanEntryCount < maxEntries ? gScanEntryCount : maxEntries;
  for (size_t index = 0; index < copyCount; ++index) {
    dest[index] = gScanEntries[index];
  }
  return copyCount;
}

uint32_t wifiManagerLastScanAtMs()
{
  return gLastScanCompletedAtMs;
}

bool wifiManagerHasKnownNetworks()
{
  return gKnownNetworkCount > 0;
}

const char *wifiManagerEncryptionLabel(wifi_auth_mode_t mode)
{
  switch (mode) {
    case WIFI_AUTH_OPEN:
      return "Open";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2-EAP";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2/WPA3";
    case WIFI_AUTH_WAPI_PSK:
      return "WAPI";
    default:
      return "Secured";
  }
}
