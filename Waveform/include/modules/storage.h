#pragma once

#include <Arduino.h>

const char *sdCardTypeLabel(uint8_t cardType);
void ensureSdDirectories();
void initSdCard();
