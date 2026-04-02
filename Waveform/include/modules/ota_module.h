#pragma once

#include <Arduino.h>

void otaModuleBuildOverlay();
void otaModuleStart();
void otaModuleReset();
void otaModuleHandle();
void otaModuleUpdateOverlay(const String &status, const String &footer, int percent);
void otaModuleHideOverlay();
void otaModuleScheduleHide(uint32_t delayMs);
bool otaModuleIsInProgress();
bool otaModuleIsReady();
void otaModuleUpdate();
