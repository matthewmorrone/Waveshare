Build and upload the Waveform firmware to the device using PlatformIO. Try USB first, fall back to OTA if it fails.

```bash
cd /Users/matthewmorrone/Documents/Arduino/Waveshare/Waveform && pio run -e waveform_usb --target upload || pio run -e waveform_ota --target upload
```
