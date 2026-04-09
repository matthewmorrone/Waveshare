#pragma once

#include <stdint.h>
#include <esp_heap_caps.h>

/**
 * Debug monitoring utilities for memory tracking and hang detection
 */
namespace DebugMonitor {

/**
 * Initialize debug monitoring (call once in setup)
 */
void init();

/**
 * Update debug monitoring (call in loop)
 * Detects hangs and periodically logs memory stats
 */
void update(uint32_t nowMs);

/**
 * Get current free heap memory
 */
uint32_t getFreeHeap();

/**
 * Get peak free heap memory observed
 */
uint32_t getMinFreeHeap();

/**
 * Log memory stats immediately
 */
void logMemoryStats();

/**
 * Get loop frame time in milliseconds
 */
uint32_t getFrameTimeMs();

/**
 * Log watchface-specific memory and performance metrics
 */
void logWatchfaceMetrics(const char* screenName);

/**
 * Check if memory is critically low (below threshold)
 */
bool isMemoryCritical();

}  // namespace DebugMonitor
