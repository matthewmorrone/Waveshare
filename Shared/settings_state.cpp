#include "settings_state.h"

namespace
{
SettingsState gSettings;

constexpr const char *kKeyBrightness = "brightness";
constexpr const char *kKeyWifi = "wifi_on";
constexpr const char *kKeyBle = "ble_on";
constexpr const char *kKey24h = "use24h";
constexpr const char *kKeyCelsius = "celsius";
constexpr const char *kKeyAutoCycle = "autocycle";
constexpr const char *kKeyAutoCycleMs = "cyclems";
constexpr const char *kKeySleepMs = "sleepms";
constexpr const char *kKeyUtcOffset = "utcoff";
constexpr const char *kKeyFaceDown = "facedown";
} // namespace

SettingsState &settingsState()
{
  return gSettings;
}

void settingsLoad(Preferences &prefs)
{
  gSettings.brightness = prefs.getUChar(kKeyBrightness, 255);
  gSettings.wifiEnabled = prefs.getBool(kKeyWifi, true);
  gSettings.bleEnabled = prefs.getBool(kKeyBle, true);
  gSettings.use24hClock = prefs.getBool(kKey24h, true);
  gSettings.useCelsius = prefs.getBool(kKeyCelsius, false);
  gSettings.autoCycleEnabled = prefs.getBool(kKeyAutoCycle, false);
  gSettings.autoCycleIntervalMs = prefs.getUInt(kKeyAutoCycleMs, 5000);
  gSettings.sleepAfterMs = prefs.getUInt(kKeySleepMs, 5UL * 60UL * 1000UL);
  gSettings.utcOffsetHours = prefs.getInt(kKeyUtcOffset, 0);
  gSettings.faceDownBlackout = prefs.getBool(kKeyFaceDown, true);
}

void settingsSave(Preferences &prefs)
{
  prefs.putUChar(kKeyBrightness, gSettings.brightness);
  prefs.putBool(kKeyWifi, gSettings.wifiEnabled);
  prefs.putBool(kKeyBle, gSettings.bleEnabled);
  prefs.putBool(kKey24h, gSettings.use24hClock);
  prefs.putBool(kKeyCelsius, gSettings.useCelsius);
  prefs.putBool(kKeyAutoCycle, gSettings.autoCycleEnabled);
  prefs.putUInt(kKeyAutoCycleMs, gSettings.autoCycleIntervalMs);
  prefs.putUInt(kKeySleepMs, gSettings.sleepAfterMs);
  prefs.putInt(kKeyUtcOffset, gSettings.utcOffsetHours);
  prefs.putBool(kKeyFaceDown, gSettings.faceDownBlackout);
}
