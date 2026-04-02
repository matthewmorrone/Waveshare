#include "modules/ble_manager.h"

#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace
{
constexpr size_t kMaxBleEntries = 8;
constexpr uint32_t kBleScanIntervalMs = 60000;
constexpr uint32_t kBleScanRetryMs = 15000;
constexpr uint32_t kBleScanDurationSeconds = 4;

portMUX_TYPE gBleMux = portMUX_INITIALIZER_UNLOCKED;
BleScanEntry gEntries[kMaxBleEntries] = {};
size_t gEntryCount = 0;
TaskHandle_t gScanTaskHandle = nullptr;
bool gEnabled = true;
bool gAvailable = true;
bool gInitialized = false;
bool gScanRequested = true;
bool gScanInProgress = false;
uint32_t gLastScanStartedAtMs = 0;
uint32_t gLastScanCompletedAtMs = 0;

void clearEntriesLocked()
{
  for (size_t index = 0; index < kMaxBleEntries; ++index) {
    gEntries[index] = BleScanEntry{};
  }
  gEntryCount = 0;
}

void copyEntry(BleScanEntry &entry, BLEAdvertisedDevice &device)
{
  const String name = device.haveName() && device.getName().length() > 0 ? device.getName() : String("Unnamed");
  snprintf(entry.name, sizeof(entry.name), "%s", name.c_str());
  snprintf(entry.address, sizeof(entry.address), "%s", device.getAddress().toString().c_str());
  entry.rssi = device.getRSSI();
  entry.connectable = device.isConnectable();
  entry.scannable = device.isScannable();
}

void finishScan()
{
  taskENTER_CRITICAL(&gBleMux);
  gScanInProgress = false;
  gScanTaskHandle = nullptr;
  gLastScanCompletedAtMs = millis();
  taskEXIT_CRITICAL(&gBleMux);
}

void bleScanTask(void *parameter)
{
  (void)parameter;

  if (!BLEDevice::getInitialized()) {
    if (!BLEDevice::init("")) {
      taskENTER_CRITICAL(&gBleMux);
      gAvailable = false;
      clearEntriesLocked();
      taskEXIT_CRITICAL(&gBleMux);
      finishScan();
      vTaskDelete(nullptr);
      return;
    }
    gInitialized = true;
  }

  BLEScan *scanner = BLEDevice::getScan();
  scanner->setActiveScan(false);
  scanner->setInterval(160);
  scanner->setWindow(80);

  BLEScanResults *results = scanner->start(kBleScanDurationSeconds, false);
  BleScanEntry localEntries[kMaxBleEntries] = {};
  size_t localCount = 0;

  if (results) {
    const int totalCount = results->getCount();
    for (int index = 0; index < totalCount; ++index) {
      BLEAdvertisedDevice device = results->getDevice(index);
      if (localCount < kMaxBleEntries) {
        copyEntry(localEntries[localCount], device);
        ++localCount;
        continue;
      }

      size_t weakestIndex = 0;
      for (size_t entryIndex = 1; entryIndex < localCount; ++entryIndex) {
        if (localEntries[entryIndex].rssi < localEntries[weakestIndex].rssi) {
          weakestIndex = entryIndex;
        }
      }
      if (device.getRSSI() > localEntries[weakestIndex].rssi) {
        copyEntry(localEntries[weakestIndex], device);
      }
    }
    scanner->clearResults();
  }

  for (size_t outer = 0; outer < localCount; ++outer) {
    size_t best = outer;
    for (size_t inner = outer + 1; inner < localCount; ++inner) {
      if (localEntries[inner].rssi > localEntries[best].rssi) {
        best = inner;
      }
    }
    if (best != outer) {
      BleScanEntry tmp = localEntries[outer];
      localEntries[outer] = localEntries[best];
      localEntries[best] = tmp;
    }
  }

  taskENTER_CRITICAL(&gBleMux);
  clearEntriesLocked();
  for (size_t index = 0; index < localCount; ++index) {
    gEntries[index] = localEntries[index];
  }
  gEntryCount = localCount;
  gAvailable = true;
  taskEXIT_CRITICAL(&gBleMux);

  finishScan();
  vTaskDelete(nullptr);
}

void startScanTask(uint32_t nowMs)
{
  taskENTER_CRITICAL(&gBleMux);
  if (gScanInProgress || !gEnabled) {
    taskEXIT_CRITICAL(&gBleMux);
    return;
  }
  gScanInProgress = true;
  gLastScanStartedAtMs = nowMs;
  taskEXIT_CRITICAL(&gBleMux);

  BaseType_t taskCreated = xTaskCreatePinnedToCore(
      bleScanTask,
      "wave_ble_scan",
      6144,
      nullptr,
      1,
      &gScanTaskHandle,
      tskNO_AFFINITY);

  if (taskCreated != pdPASS) {
    taskENTER_CRITICAL(&gBleMux);
    gScanInProgress = false;
    gScanTaskHandle = nullptr;
    gLastScanCompletedAtMs = nowMs;
    gAvailable = false;
    taskEXIT_CRITICAL(&gBleMux);
  }
}
} // namespace

void bleManagerSetEnabled(bool enabled)
{
  bool changed = false;
  taskENTER_CRITICAL(&gBleMux);
  changed = gEnabled != enabled;
  gEnabled = enabled;
  if (changed && enabled) {
    gScanRequested = true;
  }
  if (!enabled) {
    clearEntriesLocked();
  }
  taskEXIT_CRITICAL(&gBleMux);

  if (changed && !enabled && BLEDevice::getInitialized()) {
    BLEScan *scanner = BLEDevice::getScan();
    if (scanner && scanner->isScanning()) {
      scanner->stop();
    }
  }
}

void bleManagerUpdate(uint32_t nowMs, bool enabled, bool allowScan)
{
  bleManagerSetEnabled(enabled);
  if (!enabled || !allowScan) {
    return;
  }

  taskENTER_CRITICAL(&gBleMux);
  const bool scanRequested = gScanRequested;
  const bool scanInProgress = gScanInProgress;
  const uint32_t lastStartedAtMs = gLastScanStartedAtMs;
  const size_t entryCount = gEntryCount;
  taskEXIT_CRITICAL(&gBleMux);

  const uint32_t targetInterval = entryCount > 0 ? kBleScanIntervalMs : kBleScanRetryMs;
  if (!scanRequested && (nowMs - lastStartedAtMs) < targetInterval) {
    return;
  }
  if (scanInProgress) {
    return;
  }

  taskENTER_CRITICAL(&gBleMux);
  gScanRequested = false;
  taskEXIT_CRITICAL(&gBleMux);
  startScanTask(nowMs);
}

void bleManagerRequestScan()
{
  taskENTER_CRITICAL(&gBleMux);
  gScanRequested = true;
  taskEXIT_CRITICAL(&gBleMux);
}

bool bleManagerScanInProgress()
{
  taskENTER_CRITICAL(&gBleMux);
  const bool value = gScanInProgress;
  taskEXIT_CRITICAL(&gBleMux);
  return value;
}

bool bleManagerAvailable()
{
  taskENTER_CRITICAL(&gBleMux);
  const bool value = gAvailable;
  taskEXIT_CRITICAL(&gBleMux);
  return value;
}

size_t bleManagerSnapshot(BleScanEntry *dest, size_t maxEntries)
{
  if (!dest || maxEntries == 0) {
    return 0;
  }

  taskENTER_CRITICAL(&gBleMux);
  size_t copyCount = gEntryCount < maxEntries ? gEntryCount : maxEntries;
  for (size_t index = 0; index < copyCount; ++index) {
    dest[index] = gEntries[index];
  }
  taskEXIT_CRITICAL(&gBleMux);
  return copyCount;
}

size_t bleManagerDeviceCount()
{
  taskENTER_CRITICAL(&gBleMux);
  const size_t count = gEntryCount;
  taskEXIT_CRITICAL(&gBleMux);
  return count;
}

uint32_t bleManagerLastScanAtMs()
{
  taskENTER_CRITICAL(&gBleMux);
  const uint32_t value = gLastScanCompletedAtMs;
  taskEXIT_CRITICAL(&gBleMux);
  return value;
}

void bleManagerClearResults()
{
  taskENTER_CRITICAL(&gBleMux);
  clearEntriesLocked();
  taskEXIT_CRITICAL(&gBleMux);
}
