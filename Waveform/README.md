# Waveform

Standalone PlatformIO watch-face project for the Waveshare ESP32-S3 Touch AMOLED 1.8.

Behavior:
- Initializes the SH8601 display over QSPI
- Shows time, timezone, date, battery percentage, charging status, and connectivity status
- Connects to Wi-Fi with a primary network and fallback network
- Starts Arduino OTA when Wi-Fi is available
- Stays functional in offline mode and retries Wi-Fi in the background

Project root:
- `platformio.ini`
- `include/pin_config.h`
- `include/ota_config.h`
- `src/main.cpp`
- `TODO.md`

OTA notes:
- Wi-Fi credentials live in `include/ota_config.h`
- Optional: set `OTA_PASSWORD` for protected OTA updates
- The firmware advertises itself as `waveform.local`
- Future screens should keep rendering from local state and treat online features as optional
