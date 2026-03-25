#include "config/ota_config.h"
#include "core/screen_manager.h"
#include "modules/wifi_manager.h"
#include <WiFi.h>

extern ConnectivityState connectivityState;

extern const QrEntry kQrEntries[] = {
    {"Join Wi-Fi", "WIFI:T:WPA;S:" WIFI_SSID_PRIMARY ";P:" WIFI_PASSWORD_PRIMARY ";;"}
};
extern const size_t kQrEntryCount = sizeof(kQrEntries) / sizeof(kQrEntries[0]);

bool networkIsOnline()
{
  return connectivityState == ConnectivityState::Online && WiFi.status() == WL_CONNECTED;
}
