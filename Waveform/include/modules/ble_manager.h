#pragma once

#include <Arduino.h>

struct BleScanEntry
{
  char name[25] = "";
  char address[18] = "";
  int rssi = -127;
  bool connectable = false;
  bool scannable = false;
};

void bleManagerSetEnabled(bool enabled);
void bleManagerUpdate(uint32_t nowMs, bool enabled, bool allowScan);
void bleManagerRequestScan();
bool bleManagerScanInProgress();
bool bleManagerAvailable();
size_t bleManagerSnapshot(BleScanEntry *dest, size_t maxEntries);
size_t bleManagerDeviceCount();
uint32_t bleManagerLastScanAtMs();
void bleManagerClearResults();
