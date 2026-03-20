# Waveform

LVGL 9 watch firmware for the Waveshare ESP32-S3 Touch AMOLED 1.8.

What it does now:
- Initializes the SH8601 AMOLED over QSPI
- Brings up FT3x68 touch input
- Reads the two physical side/power buttons
- Mounts the SD card automatically when present
- Uses Wi-Fi, NTP, RTC persistence, and ArduinoOTA
- Runs an LVGL 9 multi-screen watch UI

Current screens:
- Watch face
- Motion / IMU
- Weather

Project root:
- `platformio.ini`
- `include/pin_config.h`
- `include/lv_conf.h`
- `src/main.cpp`

Build:
- USB: `pio run -e waveform_usb`
- OTA: `pio run -e waveform_ota -t upload`
- Big app USB: `pio run -e waveform_usb_bigapp`

This is the long-term watch project.
