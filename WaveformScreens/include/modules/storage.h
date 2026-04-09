#pragma once

#include <Arduino.h>

const char *sdCardTypeLabel(uint8_t cardType);
bool storageIsMounted();
uint8_t storageCardType();
uint64_t storageCardSizeMb();
void ensureSdDirectories();
void initSdCard();
