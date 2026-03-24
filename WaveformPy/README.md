# WaveformPy

MicroPython port of [Waveform](../Waveform) for the **Waveshare ESP32-S3 Touch AMOLED 1.8"**.

Same feature set as the C++ original, implemented in Python using [lv_micropython](https://github.com/lvgl/lv_micropython).

---

## Features

| Screen | Description |
|--------|-------------|
| Watchface | Time, date, battery bar, WiFi/USB/charging status |
| Weather | Animated conditions (rain, snow, storm), 5-day forecast, moon phase, sunrise/sunset |
| Motion | IMU visualization — Dot (pitch/roll), Cube (3D), Raw (numeric) |
| Geo | IP-based geolocation with lat/lon, timezone, city |
| Solar | Real-time planetary positions using orbital mechanics |
| Sky | Procedural starfield driven by device orientation |
| Recorder | 16kHz PCM audio recording to SD card with waveform display |
| QR Code | Multi-entry QR generator (swipe to cycle) |
| Calculator | Scientific calculator with sin/cos/tan/ln/√ function toggle |

---

## Hardware

**Waveshare ESP32-S3 Touch AMOLED 1.8"**

| Peripheral | Part | Interface |
|------------|------|-----------|
| Display | SH8601 AMOLED 368×448 | QSPI |
| Touch | FT3x68 | I2C |
| IMU | QMI8658 | I2C |
| RTC | PCF85063 | I2C |
| PMU | AXP2101 | I2C |
| GPIO Expander | XCA9554 | I2C |
| Audio Codec | ES8311 | I2S + I2C |
| SD Card | Standard | SDMMC |

---

## Requirements

### Firmware

Flash [lv_micropython](https://github.com/lvgl/lv_micropython) with ESP32-S3 support:

```bash
# Download pre-built binary or build from source
esptool.py --chip esp32s3 --port /dev/ttyUSB0 erase_flash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash -z 0x0 lv_micropython_v1.22_esp32s3.bin
```

> If Waveshare provides a custom MicroPython firmware for this board, use that
> instead as it will include the SH8601 QSPI display driver.

### Python packages (bundled in lv_micropython)
- `lvgl` — UI framework
- `ujson`, `urequests` — JSON + HTTP

---

## Setup

1. Copy `ota_config.py` and fill in your WiFi credentials:
   ```python
   WIFI_SSID = "YourSSID"
   WIFI_PASS = "YourPassword"
   ```

2. Deploy all files:
   ```bash
   chmod +x deploy.sh
   ./deploy.sh
   ```
   Or manually with mpremote/Thonny.

---

## Project Structure

```
WaveformPy/
├── boot.py              # MicroPython boot script
├── main.py              # Entry point, main loop
├── config.py            # Pin definitions, constants
├── state.py             # Shared hardware state
├── screen_manager.py    # Screen lifecycle management
├── prefs.py             # Persistent preferences (JSON file)
├── ota_config.py        # WiFi credentials (gitignored)
├── deploy.sh            # mpremote deploy script
├── drivers/
│   ├── sh8601.py        # AMOLED display (QSPI)
│   ├── ft3x68.py        # Touch controller
│   ├── xca9554.py       # GPIO expander
│   ├── axp2101.py       # Power management
│   ├── pcf85063.py      # RTC
│   ├── qmi8658.py       # IMU (accel + gyro)
│   └── es8311.py        # Audio codec
├── screens/
│   ├── watchface.py     # Clock / battery
│   ├── weather.py       # Weather + animations
│   ├── motion.py        # IMU visualization
│   ├── geo.py           # Geolocation
│   ├── solar.py         # Solar system
│   ├── sky.py           # Starfield
│   ├── recorder.py      # Audio recorder
│   ├── qr_code.py       # QR generator
│   └── calculator.py    # Scientific calculator
└── modules/
    ├── wifi_manager.py  # WiFi connection
    ├── weather_fetch.py # Open-Meteo API
    └── geo_fetch.py     # ip-api.com geolocation
```

---

## Navigation

- **Side button** — next screen
- **Swipe left/right** — next/previous screen (on gesture-aware screens)
- **Motion screen** — tap mode label to cycle Dot → Cube → Raw
- **QR screen** — swipe left/right to cycle entries
- **Calculator** — tap `fn` to toggle scientific functions

---

## Differences from C++ Waveform

| Feature | Waveform (C++) | WaveformPy |
|---------|----------------|------------|
| Framework | LVGL 9 (native) | lv_micropython |
| Weather API | configurable | Open-Meteo (free, no key) |
| OTA | ArduinoOTA | mpremote / manual |
| Fonts | DIN Clock custom font | Montserrat (LVGL built-in) |
| Audio playback | ES8311 DAC | stub (record only) |
| Deep sleep wake | PMU interrupt | `machine.deepsleep` |
| Build system | PlatformIO | none (MicroPython) |
