#pragma once

#include <Arduino.h>
#include <lvgl.h>

bool batteryIsAvailable();
int batteryPercentValue();
bool usbIsConnected();
bool batteryIsCharging();
lv_color_t batteryIndicatorColor(int percent, bool charging);
