# Waveform_LVGL

Clean LVGL 9 foundation project for the Waveshare ESP32-S3 Touch AMOLED 1.8.

What it does now:
- Initializes the SH8601 AMOLED over QSPI
- Brings up FT3x68 touch input
- Reads the two physical side/power buttons
- Runs an LVGL 9 app shell with multiple screens
- Uses touch and hardware buttons to navigate between screens

Current screens:
- Home
- System
- Roadmap

Project root:
- `platformio.ini`
- `include/pin_config.h`
- `include/lv_conf.h`
- `src/main.cpp`

Build:
- USB: `pio run -e waveform_lvgl_usb`
- OTA: `pio run -e waveform_lvgl_ota -t upload`

This project is intended to become the long-term UI foundation for the watch.
