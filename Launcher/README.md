# Launcher

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
Found 136 compatible libraries
Scanning dependencies...
Dependency Graph
|-- ArduinoJson @ 6.21.6
|-- lvgl @ 9.5.0
|-- GFX Library for Arduino @ 1.6.5+sha.4c1d0a6
|-- Adafruit XCA9554 @ 1.0.0
|-- XPowersLib @ 0.3.3
|-- SensorLib @ 0.4.0
|-- Driver Bus Library Based on Arduino @ 1.0.1
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
Compiling .pio/build/usb/lib3c4/SPI/SPI.cpp.o
Compiling .pio/build/usb/libdf9/FS/FS.cpp.o
Compiling .pio/build/usb/libdf9/FS/vfs_api.cpp.o
Compiling .pio/build/usb/libfc7/SPIFFS/SPIFFS.cpp.o
Compiling .pio/build/usb/lib5c2/LittleFS/LittleFS.cpp.o
Compiling .pio/build/usb/lib43e/TFT_eSPI/TFT_eSPI.cpp.o
Compiling .pio/build/usb/libd63/SD/SD.cpp.o
Compiling .pio/build/usb/libd63/SD/sd_diskio.cpp.o
Compiling .pio/build/usb/libd63/SD/sd_diskio_crc.c.o
Compiling .pio/build/usb/lib1ec/lvgl/core/lv_group.c.o
Compiling .pio/build/usb/lib1ec/lvgl/core/lv_obj.c.o
Compiling .pio/build/usb/lib1ec/lvgl/core/lv_obj_class.c.o
Compiling .pio/build/usb/lib1ec/lvgl/core/lv_obj_draw.c.o
Compiling .pio/build/usb/lib1ec/lvgl/core/lv_obj_event.c.o
Compiling .pio/build/usb/lib1ec/lvgl/core/lv_obj_id_builtin.c.o
Compiling .pio/build/usb/lib1ec/lvgl/core/lv_obj_pos.c.o
Compiling .pio/build/usb/lib1ec/lvgl/core/lv_obj_property.c.o
Compiling .pio/build/usb/lib1ec/lvgl/core/lv_obj_scroll.c.o
Compiling .pio/build/usb/lib1ec/lvgl/core/lv_obj_style.c.o
Compiling .pio/build/usb/lib1ec/lvgl/core/lv_obj_style_gen.c.o
Compiling .pio/build/usb/lib1ec/lvgl/core/lv_obj_tree.c.o
Compiling .pio/build/usb/lib1ec/lvgl/core/lv_observer.c.o
Compiling .pio/build/usb/lib1ec/lvgl/core/lv_refr.c.o
Compiling .pio/build/usb/lib1ec/lvgl/debugging/monkey/lv_monkey.c.o
Compiling .pio/build/usb/lib1ec/lvgl/debugging/sysmon/lv_sysmon.c.o
Compiling .pio/build/usb/lib1ec/lvgl/debugging/test/lv_test_display.c.o
Compiling .pio/build/usb/lib1ec/lvgl/debugging/test/lv_test_fs.c.o
Compiling .pio/build/usb/lib1ec/lvgl/debugging/test/lv_test_helpers.c.o
Compiling .pio/build/usb/lib1ec/lvgl/debugging/test/lv_test_indev.c.o
Compiling .pio/build/usb/lib1ec/lvgl/debugging/test/lv_test_indev_gesture.c.o
Compiling .pio/build/usb/lib1ec/lvgl/debugging/test/lv_test_screenshot_compare.c.o
Compiling .pio/build/usb/lib1ec/lvgl/debugging/vg_lite_tvg/vg_lite_matrix.c.o
Compiling .pio/build/usb/lib1ec/lvgl/debugging/vg_lite_tvg/vg_lite_tvg.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/display/lv_display.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/convert/helium/lv_draw_buf_convert_helium.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/convert/lv_draw_buf_convert.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/convert/neon/lv_draw_buf_convert_neon.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/dma2d/lv_draw_dma2d.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/dma2d/lv_draw_dma2d_fill.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/dma2d/lv_draw_dma2d_img.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/espressif/ppa/lv_draw_ppa.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/espressif/ppa/lv_draw_ppa_buf.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/espressif/ppa/lv_draw_ppa_fill.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/espressif/ppa/lv_draw_ppa_img.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/eve/lv_draw_eve.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/eve/lv_draw_eve_arc.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/eve/lv_draw_eve_fill.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/eve/lv_draw_eve_image.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/eve/lv_draw_eve_letter.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/eve/lv_draw_eve_line.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/eve/lv_draw_eve_ram_g.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/eve/lv_draw_eve_triangle.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/eve/lv_eve.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/lv_draw.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/lv_draw_3d.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/lv_draw_arc.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/lv_draw_blur.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/lv_draw_buf.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/lv_draw_image.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/lv_draw_label.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/lv_draw_line.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/lv_draw_mask.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/lv_draw_rect.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/lv_draw_triangle.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/lv_draw_vector.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/lv_image_decoder.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_draw_nanovg.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_draw_nanovg_3d.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_draw_nanovg_arc.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_draw_nanovg_border.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_draw_nanovg_box_shadow.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_draw_nanovg_fill.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_draw_nanovg_grad.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_draw_nanovg_image.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_draw_nanovg_label.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_draw_nanovg_layer.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_draw_nanovg_line.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_draw_nanovg_mask_rect.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_draw_nanovg_triangle.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_draw_nanovg_vector.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_nanovg_fbo_cache.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_nanovg_image_cache.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nanovg/lv_nanovg_utils.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nema_gfx/lv_draw_nema_gfx.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nema_gfx/lv_draw_nema_gfx_arc.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nema_gfx/lv_draw_nema_gfx_border.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nema_gfx/lv_draw_nema_gfx_fill.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nema_gfx/lv_draw_nema_gfx_img.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nema_gfx/lv_draw_nema_gfx_label.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nema_gfx/lv_draw_nema_gfx_layer.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nema_gfx/lv_draw_nema_gfx_line.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nema_gfx/lv_draw_nema_gfx_stm32_hal.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nema_gfx/lv_draw_nema_gfx_triangle.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nema_gfx/lv_draw_nema_gfx_utils.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nema_gfx/lv_draw_nema_gfx_vector.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nema_gfx/lv_nema_gfx_path.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nxp/g2d/lv_draw_buf_g2d.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nxp/g2d/lv_draw_g2d.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nxp/g2d/lv_draw_g2d_fill.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nxp/g2d/lv_draw_g2d_img.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nxp/g2d/lv_g2d_buf_map.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nxp/g2d/lv_g2d_utils.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nxp/pxp/lv_draw_buf_pxp.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nxp/pxp/lv_draw_pxp.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nxp/pxp/lv_draw_pxp_fill.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nxp/pxp/lv_draw_pxp_img.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nxp/pxp/lv_draw_pxp_layer.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nxp/pxp/lv_pxp_cfg.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nxp/pxp/lv_pxp_osa.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/nxp/pxp/lv_pxp_utils.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/opengles/lv_draw_opengles.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/renesas/dave2d/lv_draw_dave2d.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/renesas/dave2d/lv_draw_dave2d_arc.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/renesas/dave2d/lv_draw_dave2d_border.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/renesas/dave2d/lv_draw_dave2d_fill.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/renesas/dave2d/lv_draw_dave2d_image.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/renesas/dave2d/lv_draw_dave2d_label.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/renesas/dave2d/lv_draw_dave2d_line.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/renesas/dave2d/lv_draw_dave2d_mask_rectangle.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/renesas/dave2d/lv_draw_dave2d_triangle.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/renesas/dave2d/lv_draw_dave2d_utils.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sdl/lv_draw_sdl.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/snapshot/lv_snapshot.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/blend/helium/lv_blend_helium.S.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/blend/lv_draw_sw_blend.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/blend/lv_draw_sw_blend_to_a8.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/blend/lv_draw_sw_blend_to_al88.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/blend/lv_draw_sw_blend_to_argb8888.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/blend/lv_draw_sw_blend_to_argb8888_premultiplied.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/blend/lv_draw_sw_blend_to_i1.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/blend/lv_draw_sw_blend_to_l8.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/blend/lv_draw_sw_blend_to_rgb565.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/blend/lv_draw_sw_blend_to_rgb565_swapped.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/blend/lv_draw_sw_blend_to_rgb888.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/blend/neon/lv_draw_sw_blend_neon_to_rgb565.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/blend/neon/lv_draw_sw_blend_neon_to_rgb888.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/blend/riscv_v/lv_draw_sw_blend_riscv_v_to_rgb888.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/lv_draw_sw.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/lv_draw_sw_arc.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/lv_draw_sw_blur.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/lv_draw_sw_border.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/lv_draw_sw_box_shadow.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/lv_draw_sw_fill.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/lv_draw_sw_grad.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/lv_draw_sw_img.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/lv_draw_sw_letter.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/lv_draw_sw_line.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/lv_draw_sw_mask.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/lv_draw_sw_mask_rect.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/lv_draw_sw_transform.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/lv_draw_sw_triangle.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/lv_draw_sw_utils.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/sw/lv_draw_sw_vector.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_draw_buf_vg_lite.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_draw_vg_lite.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_draw_vg_lite_arc.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_draw_vg_lite_border.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_draw_vg_lite_box_shadow.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_draw_vg_lite_fill.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_draw_vg_lite_img.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_draw_vg_lite_label.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_draw_vg_lite_layer.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_draw_vg_lite_line.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_draw_vg_lite_mask_rect.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_draw_vg_lite_triangle.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_draw_vg_lite_vector.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_vg_lite_bitmap_font_cache.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_vg_lite_decoder.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_vg_lite_grad.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_vg_lite_math.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_vg_lite_path.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_vg_lite_pending.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_vg_lite_stroke.c.o
Compiling .pio/build/usb/lib1ec/lvgl/draw/vg_lite/lv_vg_lite_utils.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/display/drm/lv_linux_drm.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/display/drm/lv_linux_drm_common.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/display/drm/lv_linux_drm_egl.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/display/fb/lv_linux_fbdev.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/display/ft81x/lv_ft81x.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/display/ili9341/lv_ili9341.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/display/lcd/lv_lcd_generic_mipi.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/display/lovyan_gfx/lv_lovyan_gfx.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/display/nv3007/lv_nv3007.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/display/nxp_elcdif/lv_nxp_elcdif.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/display/renesas_glcdc/lv_renesas_glcdc.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/display/st7735/lv_st7735.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/display/st7789/lv_st7789.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/display/st7796/lv_st7796.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/display/st_ltdc/lv_st_ltdc.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/display/tft_espi/lv_tft_espi.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/draw/eve/lv_draw_eve_display.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/evdev/lv_evdev.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/libinput/lv_libinput.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/libinput/lv_xkb.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/nuttx/lv_nuttx_cache.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/nuttx/lv_nuttx_entry.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/nuttx/lv_nuttx_fbdev.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/nuttx/lv_nuttx_image_cache.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/nuttx/lv_nuttx_lcd.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/nuttx/lv_nuttx_libuv.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/nuttx/lv_nuttx_mouse.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/nuttx/lv_nuttx_profiler.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/nuttx/lv_nuttx_touchscreen.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/opengles/assets/lv_opengles_shader.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/opengles/glad/src/egl.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/opengles/glad/src/gl.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/opengles/glad/src/gles2.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/opengles/lv_opengles_debug.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/opengles/lv_opengles_driver.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/opengles/lv_opengles_egl.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/opengles/lv_opengles_glfw.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/opengles/lv_opengles_texture.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/opengles/opengl_shader/lv_opengl_shader_manager.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/opengles/opengl_shader/lv_opengl_shader_program.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/qnx/lv_qnx.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/sdl/lv_sdl_egl.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/sdl/lv_sdl_keyboard.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/sdl/lv_sdl_mouse.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/sdl/lv_sdl_mousewheel.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/sdl/lv_sdl_sw.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/sdl/lv_sdl_texture.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/sdl/lv_sdl_window.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/uefi/lv_uefi_context.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/uefi/lv_uefi_display.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/uefi/lv_uefi_indev_keyboard.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/uefi/lv_uefi_indev_pointer.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/uefi/lv_uefi_indev_touch.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/uefi/lv_uefi_private.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/wayland/lv_wayland.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/wayland/lv_wl_egl_backend.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/wayland/lv_wl_g2d_backend.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/wayland/lv_wl_keyboard.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/wayland/lv_wl_pointer.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/wayland/lv_wl_seat.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/wayland/lv_wl_shm_backend.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/wayland/lv_wl_touch.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/wayland/lv_wl_window.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/wayland/lv_wl_xdg_shell.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/windows/lv_windows_context.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/windows/lv_windows_display.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/windows/lv_windows_input.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/x11/lv_x11_display.c.o
Compiling .pio/build/usb/lib1ec/lvgl/drivers/x11/lv_x11_input.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/binfont_loader/lv_binfont_loader.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/fmt_txt/lv_font_fmt_txt.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/font_manager/lv_font_manager.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/font_manager/lv_font_manager_recycle.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/imgfont/lv_imgfont.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_dejavu_16_persian_hebrew.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_10.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_12.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_14.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_14_aligned.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_16.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_18.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_20.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_22.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_24.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_26.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_28.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_28_compressed.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_30.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_32.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_34.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_36.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_38.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_40.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_42.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_44.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_46.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_48.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_montserrat_8.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_source_han_sans_sc_14_cjk.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_source_han_sans_sc_16_cjk.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_unscii_16.c.o
Compiling .pio/build/usb/lib1ec/lvgl/font/lv_font_unscii_8.c.o
Compiling .pio/build/usb/lib1ec/lvgl/indev/lv_gridnav.c.o
Compiling .pio/build/usb/lib1ec/lvgl/indev/lv_indev.c.o
Compiling .pio/build/usb/lib1ec/lvgl/indev/lv_indev_gesture.c.o
Compiling .pio/build/usb/lib1ec/lvgl/indev/lv_indev_scroll.c.o
Compiling .pio/build/usb/lib1ec/lvgl/layouts/flex/lv_flex.c.o
Compiling .pio/build/usb/lib1ec/lvgl/layouts/grid/lv_grid.c.o
Compiling .pio/build/usb/lib1ec/lvgl/layouts/lv_layout.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/FT800-FT813/EVE_commands.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/FT800-FT813/EVE_supplemental.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/barcode/code128.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/barcode/lv_barcode.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/bin_decoder/lv_bin_decoder.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/bmp/lv_bmp.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/ffmpeg/lv_ffmpeg.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/freetype/lv_freetype.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/freetype/lv_freetype_glyph.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/freetype/lv_freetype_image.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/freetype/lv_freetype_outline.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/freetype/lv_ftsystem.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/frogfs/src/decomp_raw.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/frogfs/src/frogfs.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/fsdrv/lv_fs_arduino_esp_littlefs.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/fsdrv/lv_fs_arduino_sd.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/fsdrv/lv_fs_cbfs.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/fsdrv/lv_fs_fatfs.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/fsdrv/lv_fs_frogfs.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/fsdrv/lv_fs_littlefs.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/fsdrv/lv_fs_memfs.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/fsdrv/lv_fs_posix.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/fsdrv/lv_fs_stdio.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/fsdrv/lv_fs_uefi.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/fsdrv/lv_fs_win32.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gif/gif.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_data/lv_gltf_data.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_data/lv_gltf_data_animations.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_data/lv_gltf_data_cache.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_data/lv_gltf_data_injest.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_data/lv_gltf_data_mesh.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_data/lv_gltf_data_primitive.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_data/lv_gltf_data_shader.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_data/lv_gltf_data_skin.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_data/lv_gltf_data_texture.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_data/lv_gltf_model_node.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_data/lv_gltf_uniform_locations.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_environment/lv_gltf_ibl_sampler.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_view/assets/chromatic.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_view/assets/lv_gltf_view_shader.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_view/lv_gltf_view.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_view/lv_gltf_view_render.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/gltf_view/lv_gltf_view_shader.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/math/lv_3dmath.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gltf/math/lv_gltf_math.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/gstreamer/lv_gstreamer.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/libjpeg_turbo/lv_libjpeg_turbo.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/libpng/lv_libpng.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/libwebp/lv_libwebp.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/lodepng/lodepng.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/lodepng/lv_lodepng.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/lz4/lz4.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/nanovg/nanovg.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/qrcode/lv_qrcode.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/qrcode/qrcodegen.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/rle/lv_rle.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/rlottie/lv_rlottie.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/svg/lv_svg.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/svg/lv_svg_decoder.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/svg/lv_svg_parser.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/svg/lv_svg_render.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/svg/lv_svg_token.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgAccessor.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgAnimation.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgCanvas.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgCapi.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgCompressor.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgFill.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgGlCanvas.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgInitializer.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgLoader.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgLottieAnimation.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgLottieBuilder.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgLottieExpressions.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgLottieInterpolator.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgLottieLoader.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgLottieModel.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgLottieModifier.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgLottieParser.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgLottieParserHandler.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgMath.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgPaint.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgPicture.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgRawLoader.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgRender.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSaver.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgScene.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgShape.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgStr.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSvgCssStyle.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSvgLoader.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSvgPath.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSvgSceneBuilder.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSvgUtil.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSwCanvas.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSwFill.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSwImage.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSwMath.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSwMemPool.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSwPostEffect.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSwRaster.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSwRenderer.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSwRle.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSwShape.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgSwStroke.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgTaskScheduler.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgText.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgWgCanvas.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/thorvg/tvgXmlParser.cpp.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/tiny_ttf/lv_tiny_ttf.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/tjpgd/lv_tjpgd.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/tjpgd/tjpgd.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/vg_lite_driver/VGLite/vg_lite.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/vg_lite_driver/VGLite/vg_lite_image.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/vg_lite_driver/VGLite/vg_lite_matrix.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/vg_lite_driver/VGLite/vg_lite_path.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/vg_lite_driver/VGLite/vg_lite_stroke.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/vg_lite_driver/VGLiteKernel/vg_lite_kernel.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/vg_lite_driver/lv_vg_lite_hal/lv_vg_lite_hal.c.o
Compiling .pio/build/usb/lib1ec/lvgl/libs/vg_lite_driver/lv_vg_lite_hal/vg_lite_os.c.o
Compiling .pio/build/usb/lib1ec/lvgl/lv_init.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/cache/class/lv_cache_lru_ll.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/cache/class/lv_cache_lru_rb.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/cache/class/lv_cache_sc_da.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/cache/instance/lv_image_cache.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/cache/instance/lv_image_header_cache.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/cache/lv_cache.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/cache/lv_cache_entry.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_anim.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_anim_timeline.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_area.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_array.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_async.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_bidi.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_circle_buf.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_color.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_color_op.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_event.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_fs.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_grad.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_iter.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_ll.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_log.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_lru.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_math.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_matrix.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_palette.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_pending.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_profiler_builtin.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_profiler_builtin_posix.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_rb.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_style.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_style_gen.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_templ.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_text.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_text_ap.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_timer.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_tree.c.o
Compiling .pio/build/usb/lib1ec/lvgl/misc/lv_utils.c.o
Compiling .pio/build/usb/lib1ec/lvgl/osal/lv_cmsis_rtos2.c.o
Compiling .pio/build/usb/lib1ec/lvgl/osal/lv_freertos.c.o
Compiling .pio/build/usb/lib1ec/lvgl/osal/lv_linux.c.o
Compiling .pio/build/usb/lib1ec/lvgl/osal/lv_mqx.c.o
Compiling .pio/build/usb/lib1ec/lvgl/osal/lv_os.c.o
Compiling .pio/build/usb/lib1ec/lvgl/osal/lv_os_none.c.o
Compiling .pio/build/usb/lib1ec/lvgl/osal/lv_pthread.c.o
Compiling .pio/build/usb/lib1ec/lvgl/osal/lv_rtthread.c.o
Compiling .pio/build/usb/lib1ec/lvgl/osal/lv_sdl2.c.o
Compiling .pio/build/usb/lib1ec/lvgl/osal/lv_windows.c.o
Compiling .pio/build/usb/lib1ec/lvgl/others/file_explorer/lv_file_explorer.c.o
Compiling .pio/build/usb/lib1ec/lvgl/others/fragment/lv_fragment.c.o
Compiling .pio/build/usb/lib1ec/lvgl/others/fragment/lv_fragment_manager.c.o
Compiling .pio/build/usb/lib1ec/lvgl/others/translation/lv_translation.c.o
Compiling .pio/build/usb/lib1ec/lvgl/stdlib/builtin/lv_mem_core_builtin.c.o
Compiling .pio/build/usb/lib1ec/lvgl/stdlib/builtin/lv_sprintf_builtin.c.o
Compiling .pio/build/usb/lib1ec/lvgl/stdlib/builtin/lv_string_builtin.c.o
Compiling .pio/build/usb/lib1ec/lvgl/stdlib/builtin/lv_tlsf.c.o
Compiling .pio/build/usb/lib1ec/lvgl/stdlib/clib/lv_mem_core_clib.c.o
Compiling .pio/build/usb/lib1ec/lvgl/stdlib/clib/lv_sprintf_clib.c.o
Compiling .pio/build/usb/lib1ec/lvgl/stdlib/clib/lv_string_clib.c.o
Compiling .pio/build/usb/lib1ec/lvgl/stdlib/lv_mem.c.o
Compiling .pio/build/usb/lib1ec/lvgl/stdlib/micropython/lv_mem_core_micropython.c.o
Compiling .pio/build/usb/lib1ec/lvgl/stdlib/rtthread/lv_mem_core_rtthread.c.o
Compiling .pio/build/usb/lib1ec/lvgl/stdlib/rtthread/lv_sprintf_rtthread.c.o
Compiling .pio/build/usb/lib1ec/lvgl/stdlib/rtthread/lv_string_rtthread.c.o
Compiling .pio/build/usb/lib1ec/lvgl/stdlib/uefi/lv_mem_core_uefi.c.o
Compiling .pio/build/usb/lib1ec/lvgl/themes/default/lv_theme_default.c.o
Compiling .pio/build/usb/lib1ec/lvgl/themes/lv_theme.c.o
Compiling .pio/build/usb/lib1ec/lvgl/themes/mono/lv_theme_mono.c.o
Compiling .pio/build/usb/lib1ec/lvgl/themes/simple/lv_theme_simple.c.o
Compiling .pio/build/usb/lib1ec/lvgl/tick/lv_tick.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/3dtexture/lv_3dtexture.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/animimage/lv_animimage.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/arc/lv_arc.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/arclabel/lv_arclabel.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/bar/lv_bar.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/button/lv_button.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/buttonmatrix/lv_buttonmatrix.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/calendar/lv_calendar.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/calendar/lv_calendar_chinese.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/calendar/lv_calendar_header_arrow.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/calendar/lv_calendar_header_dropdown.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/canvas/lv_canvas.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/chart/lv_chart.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/checkbox/lv_checkbox.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/dropdown/lv_dropdown.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/gif/lv_gif.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/image/lv_image.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/imagebutton/lv_imagebutton.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/ime/lv_ime_pinyin.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/keyboard/lv_keyboard.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/label/lv_label.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/led/lv_led.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/line/lv_line.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/list/lv_list.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/lottie/lv_lottie.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/menu/lv_menu.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/msgbox/lv_msgbox.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/objx_templ/lv_objx_templ.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_animimage_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_arc_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_bar_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_buttonmatrix_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_chart_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_checkbox_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_dropdown_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_image_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_keyboard_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_label_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_led_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_line_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_menu_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_obj_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_roller_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_scale_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_slider_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_span_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_spinbox_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_spinner_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_style_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_switch_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_table_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_tabview_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/property/lv_textarea_properties.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/roller/lv_roller.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/scale/lv_scale.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/slider/lv_slider.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/span/lv_span.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/spinbox/lv_spinbox.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/spinner/lv_spinner.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/switch/lv_switch.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/table/lv_table.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/tabview/lv_tabview.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/textarea/lv_textarea.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/tileview/lv_tileview.c.o
Compiling .pio/build/usb/lib1ec/lvgl/widgets/win/lv_win.c.o
Compiling .pio/build/usb/lib351/Wire/Wire.cpp.o
Compiling .pio/build/usb/lib00d/U8g2/U8g2lib.cpp.o
Compiling .pio/build/usb/lib00d/U8g2/U8x8lib.cpp.o
Compiling .pio/build/usb/lib00d/U8g2/clib/mui.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/mui_u8g2.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_arc.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_bitmap.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_box.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_buffer.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_button.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_circle.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_cleardisplay.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_d_memory.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_d_setup.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_font.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_fonts.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_hvline.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_input_value.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_intersection.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_kerning.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_line.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_ll_hvline.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_message.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_polygon.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_selection_list.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8g2_setup.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8log.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8log_u8g2.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8log_u8x8.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_8x8.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_byte.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_cad.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_capture.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_a2printer.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ch1120.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_gp1247ai.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_gp1287ai.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_gp1294ai.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_gu800.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_hd44102.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_il3820_296x128.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ist3020.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ist3088.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ist7920.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ks0108.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_lc7981.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ld7032_60x32.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ls013b7dh03.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_max7219.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_pcd8544_84x48.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_pcf8812.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_pcf8814_hx1230.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_s1d15300.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_s1d15721.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_s1d15e06.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_sbn1661.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_sed1330.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_sh1106_64x32.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_sh1106_72x40.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_sh1107.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_sh1108.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_sh1122.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1305.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1306_128x32.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1306_128x64_noname.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1306_2040x16.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1306_48x64.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1306_64x32.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1306_64x48.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1306_72x40.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1306_96x16.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1306_96x40.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1309.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1312.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1316.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1317.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1318.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1320.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1322.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1325.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1326.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1327.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1329.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1362.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1363.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1606_172x72.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_ssd1607_200x200.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st7301.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st7302.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st7511.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st75160.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st75161.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st75256.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st7528.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st75320.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st7539.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st7565.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st7567.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st7571.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st7586s_erc240160.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st7586s_jlx320160.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st7586s_jlx384160.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st7586s_md240128.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st7586s_s028hn118a.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st7586s_ymc240160.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st7588.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_st7920.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_t6963.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_uc1601.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_uc1604.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_uc1608.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_uc1609.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_uc1610.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_uc1611.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_uc1617.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_uc1628.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_uc1638.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_uc1701_dogs102.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_d_uc1701_mini12864.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_debounce.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_display.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_fonts.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_gpio.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_input_value.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_message.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_selection_list.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_setup.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_string.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_u16toa.c.o
Compiling .pio/build/usb/lib00d/U8g2/clib/u8x8_u8toa.c.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/Arduino_DataBus.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/Arduino_G.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/Arduino_GFX.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/Arduino_GFX_Library.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/Arduino_OLED.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/Arduino_TFT.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/Arduino_TFT_18bit.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/canvas/Arduino_Canvas.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/canvas/Arduino_Canvas_3bit.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/canvas/Arduino_Canvas_Indexed.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/canvas/Arduino_Canvas_Mono.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_AVRPAR16.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_AVRPAR8.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_DUEPAR16.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32DSIPanel.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32LCD16.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32LCD8.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32PAR16.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32PAR16Q.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32PAR16QQ.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32PAR8.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32PAR8Q.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32PAR8QQ.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32PAR8QQQ.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32QSPI.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32RGBPanel.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32S2PAR16.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32S2PAR16Q.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32S2PAR8.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32S2PAR8Q.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32SPI.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP32SPIDMA.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_ESP8266SPI.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_HWSPI.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_NRFXSPI.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_RPiPicoPAR16.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_RPiPicoPAR8.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_RPiPicoSPI.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_RTLPAR8.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_STM32PAR8.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_SWPAR16.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_SWPAR8.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_SWSPI.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_UNOPAR8.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_Wire.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_XCA9554SWSPI.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_XL9535SWSPI.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/databus/Arduino_mbedSPI.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_AXS15231B.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_CO5300.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_DSI_Display.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_GC9106.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_GC9107.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_GC9A01.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_GC9C01.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_GC9D01.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_HX8347C.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_HX8347D.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_HX8352C.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_HX8357A.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_HX8357B.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_HX8369A.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_ILI9225.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_ILI9331.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_ILI9341.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_ILI9342.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_ILI9481_18bit.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_ILI9486.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_ILI9486_18bit.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_ILI9488.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_ILI9488_18bit.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_ILI9488_3bit.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_ILI9806.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_JBT6K71.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_JD9613.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_NT35310.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_NT35510.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_NT39125.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_NV3007.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_NV3023.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_NV3041A.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_OTM8009A.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_R61529.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_RGB_Display.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_RM67162.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_RM690B0.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_SEPS525.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_SH1106.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_SH8601.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_SPD2010.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_SSD1283A.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_SSD1306.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_SSD1331.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_SSD1351.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_ST7735.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_ST7789.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_ST77916.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_ST7796.cpp.o
Compiling .pio/build/usb/libb7b/GFX Library for Arduino/display/Arduino_WEA2012.cpp.o
Compiling .pio/build/usb/lib2c4/Adafruit_BusIO/Adafruit_BusIO_Register.cpp.o
Compiling .pio/build/usb/lib2c4/Adafruit_BusIO/Adafruit_GenericDevice.cpp.o
Compiling .pio/build/usb/lib2c4/Adafruit_BusIO/Adafruit_I2CDevice.cpp.o
Compiling .pio/build/usb/lib2c4/Adafruit_BusIO/Adafruit_SPIDevice.cpp.o
Compiling .pio/build/usb/lib8cc/Adafruit XCA9554/Adafruit_XCA9554.cpp.o
Compiling .pio/build/usb/lib9bf/XPowersLib/XPowersLibInterface.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/SensorBHI260AP.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/SensorBHI260AP_Klio.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/SensorBHI360.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/SensorLibExceptionFix.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/SensorRtcHelper.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/SensorWireHelper.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/TouchDrvCHSC5816.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/TouchDrvCSTXXX.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/TouchDrvFT6X36.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/TouchDrvGT911.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/TouchDrvGT9895.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/TouchDrvHI8561.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/BMM150/bmm150.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/BoschParseStatic.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/BoschSensorBase.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/BoschSensorInfo.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/BoschSensorUtils.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi260x/bhi3.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi260x/bhi3_multi_tap.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi260x/bhy2.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi260x/bhy2_bsec.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi260x/bhy2_head_tracker.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi260x/bhy2_hif.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi260x/bhy2_klio.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi260x/bhy2_parse.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi260x/bhy2_swim.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi36x/bhi360_activity_param.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi36x/bhi360_bsec_param.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi36x/bhi360_bsx_algo_param.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi36x/bhi360_event_data.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi36x/bhi360_head_orientation_param.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi36x/bhi360_hif.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi36x/bhi360_multi_tap_param.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi36x/bhi360_phy_sensor_ctrl_param.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi36x/bhi360_system_param.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi36x/bhi360_virtual_sensor_conf_param.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bhi36x/bhi360_virtual_sensor_info_param.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bma4xx/bma4.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bma4xx/bma422_an.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bma4xx/bma423.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bma4xx/bma456_an.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bma4xx/bma456_tablet.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bma4xx/bma456h.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bma4xx/bma456mm.c.o
Compiling .pio/build/usb/lib97e/SensorLib/bosch/bma4xx/bma456w.c.o
Compiling .pio/build/usb/lib97e/SensorLib/platform/SensorCommDebug.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/platform/SensorCommStatic.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/touch/TouchDrvCST226.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/touch/TouchDrvCST3530.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/touch/TouchDrvCST816.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/touch/TouchDrvCST92xx.cpp.o
Compiling .pio/build/usb/lib97e/SensorLib/touch/TouchPoints.cpp.o
Compiling .pio/build/usb/lib68e/Arduino_DriveBus/Arduino_DriveBus.cpp.o
Compiling .pio/build/usb/lib68e/Arduino_DriveBus/Arduino_DriveBus_Library.cpp.o
Compiling .pio/build/usb/lib68e/Arduino_DriveBus/Arduino_IIC.cpp.o
Compiling .pio/build/usb/lib68e/Arduino_DriveBus/Arduino_IIC_Chip.cpp.o
Compiling .pio/build/usb/lib68e/Arduino_DriveBus/hardware/Arduino_HWIIC.cpp.o
Compiling .pio/build/usb/lib68e/Arduino_DriveBus/power_chip/Arduino_ETA4662.cpp.o
Compiling .pio/build/usb/lib68e/Arduino_DriveBus/power_chip/Arduino_SY6970.cpp.o
Compiling .pio/build/usb/lib68e/Arduino_DriveBus/touch_chip/Arduino_CST2xxSE.cpp.o
Compiling .pio/build/usb/lib68e/Arduino_DriveBus/touch_chip/Arduino_CST816x.cpp.o
Compiling .pio/build/usb/lib68e/Arduino_DriveBus/touch_chip/Arduino_FT3x68.cpp.o
Compiling .pio/build/usb/lib8d5/ESP_I2S/ESP_I2S.cpp.o
Compiling .pio/build/usb/lib566/Network/NetworkClient.cpp.o
Compiling .pio/build/usb/lib566/Network/NetworkEvents.cpp.o
Compiling .pio/build/usb/lib566/Network/NetworkInterface.cpp.o
Compiling .pio/build/usb/lib566/Network/NetworkManager.cpp.o
Compiling .pio/build/usb/lib566/Network/NetworkServer.cpp.o
Compiling .pio/build/usb/lib566/Network/NetworkUdp.cpp.o
Compiling .pio/build/usb/lib428/NetworkClientSecure/NetworkClientSecure.cpp.o
Compiling .pio/build/usb/lib428/NetworkClientSecure/ssl_client.cpp.o
Compiling .pio/build/usb/libecf/HTTPClient/HTTPClient.cpp.o
Compiling .pio/build/usb/lib33a/Preferences/Preferences.cpp.o
Compiling .pio/build/usb/lib153/SD_MMC/SD_MMC.cpp.o
Compiling .pio/build/usb/lib945/WiFi/AP.cpp.o
Compiling .pio/build/usb/lib945/WiFi/STA.cpp.o
Compiling .pio/build/usb/lib945/WiFi/WiFi.cpp.o
Compiling .pio/build/usb/lib945/WiFi/WiFiAP.cpp.o
Compiling .pio/build/usb/lib945/WiFi/WiFiGeneric.cpp.o
Compiling .pio/build/usb/lib945/WiFi/WiFiMulti.cpp.o
Compiling .pio/build/usb/lib945/WiFi/WiFiSTA.cpp.o
Compiling .pio/build/usb/lib945/WiFi/WiFiScan.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLE2901.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLE2902.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLE2904.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEAddress.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEAdvertisedDevice.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEAdvertising.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEBeacon.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLECharacteristic.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLECharacteristicMap.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEClient.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEDescriptor.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEDescriptorMap.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEDevice.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEEddystoneTLM.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEEddystoneURL.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEExceptions.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEHIDDevice.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLERemoteCharacteristic.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLERemoteDescriptor.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLERemoteService.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEScan.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLESecurity.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEServer.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEService.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEServiceMap.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEUUID.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEUtils.cpp.o
Compiling .pio/build/usb/lib40b/BLE/BLEValue.cpp.o
Compiling .pio/build/usb/lib40b/BLE/FreeRTOS.cpp.o
Compiling .pio/build/usb/lib40b/BLE/GeneralUtils.cpp.o
Compiling .pio/build/usb/libcbf/Hash/PBKDF2_HMACBuilder.cpp.o
Compiling .pio/build/usb/libcbf/Hash/SHA1Builder.cpp.o
Compiling .pio/build/usb/libcbf/Hash/SHA2Builder.cpp.o
Compiling .pio/build/usb/libcbf/Hash/SHA3Builder.cpp.o
Compiling .pio/build/usb/libfed/Update/HttpsOTAUpdate.cpp.o
Compiling .pio/build/usb/libfed/Update/Updater.cpp.o
Compiling .pio/build/usb/libfed/Update/Updater_Signing.cpp.o
Compiling .pio/build/usb/libf32/ESPmDNS/ESPmDNS.cpp.o
Compiling .pio/build/usb/lib6c8/ArduinoOTA/ArduinoOTA.cpp.o
Compiling .pio/build/usb/src/core/screen_manager.cpp.o
Compiling .pio/build/usb/src/drivers/es8311.c.o
Compiling .pio/build/usb/src/fonts/calc_symbols_24.c.o
Compiling .pio/build/usb/src/fonts/montserrat_bold_128.c.o
Compiling .pio/build/usb/src/main.cpp.o
Compiling .pio/build/usb/src/modules/battery.cpp.o
Compiling .pio/build/usb/src/modules/ble_manager.cpp.o
Compiling .pio/build/usb/src/modules/debug_monitor.cpp.o
Compiling .pio/build/usb/src/modules/imu_module.cpp.o
Compiling .pio/build/usb/src/modules/math_utils.cpp.o
Compiling .pio/build/usb/src/modules/ota_module.cpp.o
Compiling .pio/build/usb/src/modules/storage.cpp.o
Compiling .pio/build/usb/src/modules/wifi_manager.cpp.o
Compiling .pio/build/usb/src/screens/launcher.cpp.o
Compiling .pio/build/usb/src/state/settings_state.cpp.o
Compiling .pio/build/usb/src/support/din_clock_76.c.o
Building .pio/build/usb/bootloader.bin
Generating partitions .pio/build/usb/partitions.bin
Compiling .pio/build/usb/FrameworkArduino/ColorFormat.c.o
esptool v5.1.2
Creating ESP32S3 image...
Merged 2 ELF sections.
Successfully created ESP32S3 image.
Compiling .pio/build/usb/FrameworkArduino/Esp.cpp.o
Compiling .pio/build/usb/FrameworkArduino/FirmwareMSC.cpp.o
Compiling .pio/build/usb/FrameworkArduino/FunctionalInterrupt.cpp.o
Compiling .pio/build/usb/FrameworkArduino/HEXBuilder.cpp.o
Compiling .pio/build/usb/FrameworkArduino/HWCDC.cpp.o
Compiling .pio/build/usb/FrameworkArduino/HardwareSerial.cpp.o
Compiling .pio/build/usb/FrameworkArduino/HashBuilder.cpp.o
Compiling .pio/build/usb/FrameworkArduino/IPAddress.cpp.o
Compiling .pio/build/usb/FrameworkArduino/MD5Builder.cpp.o
Compiling .pio/build/usb/FrameworkArduino/MacAddress.cpp.o
Compiling .pio/build/usb/FrameworkArduino/Print.cpp.o
Compiling .pio/build/usb/FrameworkArduino/Stream.cpp.o
Compiling .pio/build/usb/FrameworkArduino/StreamString.cpp.o
Compiling .pio/build/usb/FrameworkArduino/Tone.cpp.o
Compiling .pio/build/usb/FrameworkArduino/USB.cpp.o
Compiling .pio/build/usb/FrameworkArduino/USBCDC.cpp.o
Compiling .pio/build/usb/FrameworkArduino/USBMSC.cpp.o
Compiling .pio/build/usb/FrameworkArduino/WMath.cpp.o
Compiling .pio/build/usb/FrameworkArduino/WString.cpp.o
Compiling .pio/build/usb/FrameworkArduino/base64.cpp.o
Compiling .pio/build/usb/FrameworkArduino/cbuf.cpp.o
Compiling .pio/build/usb/FrameworkArduino/chip-debug-report.cpp.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-adc.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-bt.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-cpu.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-dac.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-gpio.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-hosted.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-i2c-ng.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-i2c-slave.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-i2c.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-ledc.c.o

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
Found 136 compatible libraries
Scanning dependencies...
Dependency Graph
|-- ArduinoJson @ 6.21.6
|-- lvgl @ 9.5.0
|-- GFX Library for Arduino @ 1.6.5+sha.4c1d0a6
|-- Adafruit XCA9554 @ 1.0.0
|-- XPowersLib @ 0.3.3
|-- SensorLib @ 0.4.0
|-- Driver Bus Library Based on Arduino @ 1.0.1
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
Compiling .pio/build/usb/src/main.cpp.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-log-wrapper.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-matrix.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-misc.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-periman.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-psram.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-rgb-led.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-rmt.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-sigmadelta.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-spi.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-time.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-timer.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-tinyusb.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-touch-ng.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-touch.c.o
Compiling .pio/build/usb/FrameworkArduino/esp32-hal-uart.c.o
Compiling .pio/build/usb/FrameworkArduino/firmware_msc_fat.c.o
Compiling .pio/build/usb/FrameworkArduino/freertos_stats.cpp.o
Compiling .pio/build/usb/FrameworkArduino/idf_openthread_mpool_wrapper.c.o
Compiling .pio/build/usb/FrameworkArduino/libb64/cdecode.c.o
Compiling .pio/build/usb/FrameworkArduino/libb64/cencode.c.o
Compiling .pio/build/usb/FrameworkArduino/main.cpp.o
Compiling .pio/build/usb/FrameworkArduino/stdlib_noniso.c.o
Compiling .pio/build/usb/FrameworkArduino/wiring_pulse.c.o
Compiling .pio/build/usb/FrameworkArduino/wiring_shift.c.o
Archiving .pio/build/usb/libFrameworkArduino.a
Indexing .pio/build/usb/libFrameworkArduino.a
