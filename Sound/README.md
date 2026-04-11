# Sound

Standalone flashable build for the \ screen profile.

Build:
- \Processing usb (platform: https://github.com/pioarduino/platform-espressif32/releases/download/55.03.37/platform-espressif32.zip; board: esp32-s3-devkitc-1; framework: arduino)
--------------------------------------------------------------------------------
Verbose mode can be enabled via `-v, --verbose` option
CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/esp32-s3-devkitc-1.html
PLATFORM: Espressif 32 (55.3.37) > Espressif ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)
HARDWARE: ESP32S3 240MHz, 320KB RAM, 8MB Flash
DEBUG: Current (esp-builtin) On-board (esp-builtin) External (cmsis-dap, esp-bridge, esp-prog, iot-bus-jtag, jlink, minimodule, olimex-arm-usb-ocd, olimex-arm-usb-ocd-h, olimex-arm-usb-tiny-h, olimex-jtag-tiny, tumpa)
PACKAGES: 
 - contrib-piohome @ 3.4.4 
 - framework-arduinoespressif32 @ 3.3.7 
 - framework-arduinoespressif32-libs @ 5.5.0+sha.87912cd291 
 - tool-esptoolpy @ 5.1.2 
 - toolchain-xtensa-esp-elf @ 14.2.0+20251107
LDF: Library Dependency Finder -> https://bit.ly/configure-pio-ldf
LDF Modes: Finder ~ chain, Compatibility ~ soft
Library Manager: Installing XPowersLib
Unpacking 0% 10% 20% 30% 40% 50% 60% 70% 80% 90% 100%
Library Manager: XPowersLib@0.3.3 has been installed!
Library Manager: Installing SensorLib
Unpacking 0% 10% 20% 30% 40% 50% 60% 70% 80% 90% 100%
Library Manager: SensorLib@0.4.0 has been installed!
Found 50 compatible libraries
Scanning dependencies...
Dependency Graph
|-- ArduinoJson @ 6.21.6
|-- lvgl @ 9.5.0
|-- GFX Library for Arduino @ 1.6.5+sha.4c1d0a6
|-- Adafruit XCA9554 @ 1.0.0
|-- XPowersLib @ 0.3.3
|-- SensorLib @ 0.4.0
|-- ESP_I2S @ 3.3.7
|-- FS @ 3.3.7
|-- HTTPClient @ 3.3.7
|-- Preferences @ 3.3.7
|-- SD_MMC @ 3.3.7
|-- WiFi @ 3.3.7
|-- NetworkClientSecure @ 3.3.7
|-- Wire @ 3.3.7
|-- BLE @ 3.3.7
|-- ArduinoOTA @ 3.3.7
Building in release mode
Compiling .pio/build/usb/libdf9/FS/FS.cpp.o
Compiling .pio/build/usb/libdf9/FS/vfs_api.cpp.o
Compiling .pio/build/usb/lib5c2/LittleFS/LittleFS.cpp.o
Compiling .pio/build/usb/lib3c4/SPI/SPI.cpp.o
Compiling .pio/build/usb/libd63/SD/SD.cpp.o
Compiling .pio/build/usb/libd63/SD/sd_diskio.cpp.o
Compiling .pio/build/usb/libd63/SD/sd_diskio_crc.c.o
Compiling .pio/build/usb/libdb4/lvgl/core/lv_group.c.o
Compiling .pio/build/usb/libdb4/lvgl/core/lv_obj.c.o
Compiling .pio/build/usb/libdb4/lvgl/core/lv_obj_class.c.o
Compiling .pio/build/usb/libdb4/lvgl/core/lv_obj_draw.c.o

Upload over USB:
- \Processing usb (platform: https://github.com/pioarduino/platform-espressif32/releases/download/55.03.37/platform-espressif32.zip; board: esp32-s3-devkitc-1; framework: arduino)
--------------------------------------------------------------------------------
Verbose mode can be enabled via `-v, --verbose` option
CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/esp32-s3-devkitc-1.html
PLATFORM: Espressif 32 (55.3.37) > Espressif ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)
HARDWARE: ESP32S3 240MHz, 320KB RAM, 8MB Flash
DEBUG: Current (esp-builtin) On-board (esp-builtin) External (cmsis-dap, esp-bridge, esp-prog, iot-bus-jtag, jlink, minimodule, olimex-arm-usb-ocd, olimex-arm-usb-ocd-h, olimex-arm-usb-tiny-h, olimex-jtag-tiny, tumpa)
PACKAGES: 
 - contrib-piohome @ 3.4.4 
 - framework-arduinoespressif32 @ 3.3.7 
 - framework-arduinoespressif32-libs @ 5.5.0+sha.87912cd291 
 - tool-esptoolpy @ 5.1.2 
 - toolchain-xtensa-esp-elf @ 14.2.0+20251107
LDF: Library Dependency Finder -> https://bit.ly/configure-pio-ldf
LDF Modes: Finder ~ chain, Compatibility ~ soft
Found 50 compatible libraries
Scanning dependencies...
Dependency Graph
|-- ArduinoJson @ 6.21.6
|-- lvgl @ 9.5.0
|-- GFX Library for Arduino @ 1.6.5+sha.4c1d0a6
|-- Adafruit XCA9554 @ 1.0.0
|-- XPowersLib @ 0.3.3
|-- SensorLib @ 0.4.0
|-- ESP_I2S @ 3.3.7
|-- FS @ 3.3.7
|-- HTTPClient @ 3.3.7
|-- Preferences @ 3.3.7
|-- SD_MMC @ 3.3.7
|-- WiFi @ 3.3.7
|-- NetworkClientSecure @ 3.3.7
|-- Wire @ 3.3.7
|-- BLE @ 3.3.7
|-- ArduinoOTA @ 3.3.7
Building in release mode
Compiling .pio/build/usb/libdf9/FS/FS.cpp.o
Compiling .pio/build/usb/libdf9/FS/vfs_api.cpp.o
Compiling .pio/build/usb/lib5c2/LittleFS/LittleFS.cpp.o
Compiling .pio/build/usb/lib3c4/SPI/SPI.cpp.o
Compiling .pio/build/usb/libd63/SD/SD.cpp.o
Compiling .pio/build/usb/libd63/SD/sd_diskio.cpp.o
Compiling .pio/build/usb/libdb4/lvgl/core/lv_obj_event.c.o
Compiling .pio/build/usb/libdb4/lvgl/core/lv_obj_id_builtin.c.o
Compiling .pio/build/usb/libdb4/lvgl/core/lv_obj_pos.c.o
Compiling .pio/build/usb/libdb4/lvgl/core/lv_obj_property.c.o
Compiling .pio/build/usb/libdb4/lvgl/core/lv_obj_scroll.c.o
