# Reboot

Minimal scratch project for the Waveshare ESP32-S3 Touch AMOLED 1.8.

What it does:
- Brings up the SH8601 display
- Enables the XCA9554-controlled rails
- Probes PMU, RTC, touch, IMU, and audio codec over I2C
- Mounts the SD card if present
- Initializes Wi-Fi station mode without connecting
- Initializes BLE without scanning
- Renders a plain hardware checklist on screen

Build:
- `pio run -d Reboot -e hardware_check_usb`

Upload:
- `pio run -d Reboot -e hardware_check_usb -t upload`
