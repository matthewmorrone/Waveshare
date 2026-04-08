# Locket

Minimal standalone display project for the Waveshare ESP32-S3 Touch AMOLED 1.8.

Current behavior:
- Initializes the display with the safe expander-first boot order
- Shows a looping blue-sky animation
- Keeps stars fixed in place
- Moves randomly generated clouds from right to left forever

Build:
- `pio run -d /Users/matthewmorrone/Documents/Arduino/Waveshare/Locket -e locket_usb`

Upload:
- `pio run -d /Users/matthewmorrone/Documents/Arduino/Waveshare/Locket -e locket_usb -t upload`
