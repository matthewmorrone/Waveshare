#pragma once

#include <stdint.h>
#include <stddef.h>
#include "config/pin_config.h"

// --- Geo screen ---
constexpr uint32_t kGeoRefreshIntervalMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kGeoRetryIntervalMs   = 60UL * 1000UL;
constexpr uint32_t kGeoFetchTimeoutMs    = 2500;
constexpr int kGeoTitleY                 = 28;
constexpr int kGeoIpY                    = 72;
constexpr int kGeoLocationY              = 132;
constexpr int kGeoDetailY                = 252;

// --- QR screen ---
constexpr int kQrPanelSize               = 280;
constexpr int kQrModulePadding           = 18;
constexpr int kQrCanvasSize              = kQrPanelSize - (kQrModulePadding * 2);
constexpr int kQrTitleY                  = 24;
constexpr int kQrPanelY                  = 64;
constexpr int kQrTextY                   = 364;

// --- Sky screen ---
constexpr int kSkyTitleY                 = 20;
constexpr int kSkyPanelY                 = 54;
constexpr int kSkyPanelSize              = 304;
constexpr size_t kSkyStarCount           = 20;

// --- Solar screen ---
constexpr int kSolarTitleY               = 20;
constexpr int kSolarPanelY               = 52;
constexpr int kSolarPanelSize            = 312;
constexpr size_t kSolarPlanetCount       = 8;
constexpr uint32_t kSolarRefreshIntervalMs = 30UL * 60UL * 1000UL;
constexpr uint32_t kSolarRetryIntervalMs   = 60UL * 1000UL;
constexpr uint32_t kSolarFetchTimeoutMs    = 2500;

// --- Watchface screen ---
constexpr int kBatteryTrackWidth         = 266;
constexpr int kBatteryTrackHeight        = 12;
constexpr int kBatteryTrackY             = 358;
constexpr int kWatchTimeY                = 58;
constexpr int kWatchDateY                = 272;
constexpr int kWatchTimezoneY            = 306;
constexpr int kClockDigitWidth           = 50;
constexpr int kClockColonWidth           = 22;
constexpr int kClockGlyphHeight          = 148;
constexpr int kClockZoom                 = 384;

// --- Motion screen ---
constexpr size_t kCubeEdgeCount          = 12;
constexpr size_t kCubeArrowSegmentCount  = 3;
constexpr size_t kCubeArrowCount         = 3;
constexpr size_t kCubeArrowLineCount     = kCubeArrowCount * kCubeArrowSegmentCount;
constexpr int kDotDiameter               = 26;
constexpr float kMotionDeltaThreshold          = 0.18f;
constexpr float kMotionSensorSmoothingAlpha    = 0.18f;
constexpr float kMotionReadoutSmoothingAlpha   = 0.14f;
constexpr float kMotionIndicatorSmoothingAlpha = 0.12f;
constexpr float kMotionIndicatorDeadzoneVector = 0.035f;
constexpr float kMotionIndicatorFullScaleVector = 0.55f;
constexpr int kMotionViewAnimOffsetPx    = 28;
constexpr uint32_t kMotionViewAnimMs     = 140;
constexpr uint32_t kMotionRefreshMs      = 40;

// --- Calculator screen ---
constexpr int kCalculatorCardWidth       = LCD_WIDTH - 28;
constexpr int kCalculatorButtonWidth     = 72;
constexpr int kCalculatorButtonHeight    = 52;
constexpr int kCalculatorButtonGap       = 5;
constexpr uint32_t kCalculatorClearLongPressMs = 450;

// --- Recorder screen ---
constexpr uint32_t kRecorderSampleRate         = 16000;
constexpr uint32_t kRecorderBytesPerSecond     = kRecorderSampleRate * 2U * 2U;
constexpr uint32_t kRecorderAudioTimeoutMs     = 100;
constexpr uint32_t kRecorderButtonDiameter     = 168;
constexpr size_t   kRecorderAudioChunkBytes    = 4096;
constexpr uint32_t kRecorderPulseStepMs        = 90;
constexpr size_t   kRecorderSpinnerSegmentCount = 12;
constexpr size_t   kRecorderWaveformBarCount   = 32;
constexpr int kRecorderWaveformWidth           = 288;
constexpr int kRecorderWaveformHeight          = 90;
constexpr int kRecorderButtonCenterYOffset     = -42;
constexpr int kRecorderWaveformYOffset         = 112;
constexpr int kRecorderCodecVolume             = 85;

// --- SD card ---
constexpr const char *kSdMountPath       = "/sdcard";
constexpr const char *kSdStartupDirs[]   = {
    "/assets",
    "/config",
    "/cache",
    "/recordings",
    "/update",
};

// --- Recorder clip path ---
constexpr const char *kRecorderClipPath  = "/recordings/last_take.pcm";

// --- Weather ---
constexpr uint32_t kWeatherRefreshIntervalMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kWeatherRetryIntervalMs   = 60UL * 1000UL;
constexpr uint32_t kWeatherFetchTimeoutMs    = 2500;
constexpr int kWeatherSwipeMinDistancePx     = 22;
constexpr int kWeatherSwipeMinDominancePx    = 8;
constexpr int kWeatherNavSwipeMinDistancePx  = 12;
constexpr int kWeatherNavSwipeMinDominancePx = -10;
