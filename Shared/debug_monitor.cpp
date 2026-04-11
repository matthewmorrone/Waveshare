#include "debug_monitor.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

namespace DebugMonitor {

// Configuration
constexpr uint32_t MEMORY_LOG_INTERVAL_MS = 30000;  // Log every 30 seconds
constexpr uint32_t HANG_DETECTION_THRESHOLD_MS = 2000;  // Alert if loop takes > 2 seconds
constexpr uint32_t MIN_FREE_HEAP_THRESHOLD = 5000;  // Alert if free heap < 5KB (critical only)

// State
static uint32_t lastMemoryLogAtMs = 0;
static uint32_t lastLoopAtMs = 0;
static uint32_t minFreeHeap = UINT32_MAX;
static uint32_t lastFrameTimeMs = 0;
static bool firstRun = true;

void init() {
  Serial.println("\n[DEBUG] Initializing debug monitor...");
  minFreeHeap = getFreeHeap();
  lastLoopAtMs = millis();
  lastMemoryLogAtMs = millis();
  firstRun = false;
  logMemoryStats();
}

void update(uint32_t nowMs) {
  // Calculate frame time
  uint32_t frameTime = nowMs - lastLoopAtMs;
  lastFrameTimeMs = frameTime;

  // Detect hangs (loop taking too long)
  if (frameTime > HANG_DETECTION_THRESHOLD_MS) {
    Serial.printf("[HANG] Loop iteration took %lums (threshold: %lums)\n",
                  frameTime, HANG_DETECTION_THRESHOLD_MS);
  }

  lastLoopAtMs = nowMs;

  // Periodically log memory stats
  if (nowMs - lastMemoryLogAtMs >= MEMORY_LOG_INTERVAL_MS) {
    lastMemoryLogAtMs = nowMs;
    logMemoryStats();
  }

  // Track minimum free heap
  uint32_t currentFree = getFreeHeap();
  if (currentFree < minFreeHeap) {
    minFreeHeap = currentFree;
  }

  // Alert on low memory
  if (currentFree < MIN_FREE_HEAP_THRESHOLD) {
    Serial.printf("[MEMORY_WARNING] Low heap: %lu bytes\n", currentFree);
  }
}

uint32_t getFreeHeap() {
  return esp_get_free_heap_size();
}

uint32_t getMinFreeHeap() {
  return minFreeHeap;
}

void logMemoryStats() {
  uint32_t freeHeap = getFreeHeap();
  uint32_t freeInternalHeap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  uint32_t freeExternalHeap = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

  Serial.printf("[MEMORY] Free: %lu | Min: %lu | Internal: %lu | External: %lu\n",
                freeHeap, minFreeHeap, freeInternalHeap, freeExternalHeap);
}

uint32_t getFrameTimeMs() {
  return lastFrameTimeMs;
}

void logWatchfaceMetrics(const char* screenName) {
  uint32_t freeHeap = getFreeHeap();
  uint32_t frameTime = getFrameTimeMs();

  Serial.printf("[WATCHFACE] Screen: %s | Free: %lu bytes | Frame: %lums | Status: %s\n",
                screenName, freeHeap, frameTime,
                (freeHeap < 10000) ? "CRITICAL" : (freeHeap < 30000) ? "LOW" : "OK");
}

bool isMemoryCritical() {
  return getFreeHeap() < 5000;
}

}  // namespace DebugMonitor
