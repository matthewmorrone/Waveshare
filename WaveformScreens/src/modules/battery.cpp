#include "modules/battery.h"

#include "config/pin_config.h"
#include <Arduino.h>
#include <XPowersLib.h>

extern XPowersPMU power;
extern bool powerReady;

bool batteryIsAvailable()
{
  return powerReady && power.isBatteryConnect();
}

int batteryPercentValue()
{
  if (!batteryIsAvailable()) {
    return -1;
  }

  int percent = power.getBatteryPercent();
  return percent < 0 ? -1 : constrain(percent, 0, 100);
}

bool usbIsConnected()
{
  return powerReady && power.isVbusIn();
}

bool batteryIsCharging()
{
  if (!powerReady || !usbIsConnected()) {
    return false;
  }

  int percent = power.getBatteryPercent();
  if (percent >= 100) {
    return true;
  }

  if (power.isCharging()) {
    return true;
  }

  switch (power.getChargerStatus()) {
    case XPOWERS_AXP2101_CHG_PRE_STATE:
    case XPOWERS_AXP2101_CHG_CC_STATE:
    case XPOWERS_AXP2101_CHG_CV_STATE:
    case XPOWERS_AXP2101_CHG_TRI_STATE:
      return true;
    default:
      return false;
  }
}

lv_color_t batteryIndicatorColor(int percent, bool charging)
{
  if (charging) {
    return lv_color_make(0, 120, 255);
  }

  if (percent < 0) {
    return lv_color_make(88, 88, 96);
  }

  int clamped = constrain(percent, 0, 100);
  uint8_t red = 0;
  uint8_t green = 0;

  if (clamped <= 50) {
    red = 255;
    green = static_cast<uint8_t>((clamped * 255) / 50);
  } else {
    red = static_cast<uint8_t>(((100 - clamped) * 255) / 50);
    green = 255;
  }

  return lv_color_make(red, green, 0);
}
