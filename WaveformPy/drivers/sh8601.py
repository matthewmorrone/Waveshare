# drivers/sh8601.py - SH8601 AMOLED display driver (QSPI)
# This wraps the lv_micropython display driver for SH8601.
# Requires lv_micropython firmware with sh8601 QSPI support compiled in,
# or an equivalent display driver provided by the board vendor.
#
# If using Waveshare's MicroPython firmware, this driver may already be
# included. Otherwise, use the generic QSPI display init sequence below.

import lvgl as lv
from config import (LCD_WIDTH, LCD_HEIGHT,
                    PIN_LCD_SDIO0, PIN_LCD_SDIO1, PIN_LCD_SDIO2, PIN_LCD_SDIO3,
                    PIN_LCD_SCLK, PIN_LCD_CS, PIN_LCD_RST)

_DRAW_BUF_ROWS = 40  # Partial render buffer height


class SH8601Display:
    """
    Initializes the SH8601 via QSPI and registers it as an LVGL display.

    Requires lv_micropython (https://github.com/lvgl/lv_micropython) with
    the esp32 QSPI display driver enabled, or equivalent vendor firmware.
    """

    def __init__(self):
        self._disp = None
        self._buf1 = None
        self._buf2 = None

    def init(self):
        """Initialize display hardware and register with LVGL."""
        try:
            # Attempt vendor-specific display init (Waveshare firmware)
            import waveshare_amoled  # type: ignore
            waveshare_amoled.init()
            self._register_lvgl_display(waveshare_amoled.flush, getattr(waveshare_amoled, "rounder", None))
        except ImportError:
            # Fall back to generic lv_micropython QSPI display
            self._init_generic_qspi()

    def _init_generic_qspi(self):
        """Generic QSPI init using lv_micropython esp32 display driver."""
        try:
            from espidf import QSPI_2LAN_SH8601  # type: ignore
            drv = QSPI_2LAN_SH8601(
                miso=PIN_LCD_SDIO1,
                mosi=PIN_LCD_SDIO0,
                sclk=PIN_LCD_SCLK,
                cs=PIN_LCD_CS,
                dc=-1,   # SH8601 uses 4-wire QSPI, no DC pin
                rst=PIN_LCD_RST,
                width=LCD_WIDTH,
                height=LCD_HEIGHT,
                freq=20_000_000,
            )
            drv.init()
            self._register_lvgl_display(drv.flush)
        except Exception as e:
            print("SH8601 QSPI init failed:", e)
            # Fall back to stub display for UI testing
            self._register_stub_display()

    def _register_lvgl_display(self, flush_cb, rounder_cb=None):
        buf_size = LCD_WIDTH * _DRAW_BUF_ROWS
        self._buf1 = lv.draw_buf_create(buf_size, 1, lv.COLOR_FORMAT.RGB565, 0)
        self._buf2 = lv.draw_buf_create(buf_size, 1, lv.COLOR_FORMAT.RGB565, 0)

        self._disp = lv.display_create(LCD_WIDTH, LCD_HEIGHT)
        self._disp.set_flush_cb(flush_cb)
        self._disp.set_draw_buffers(self._buf1, self._buf2)
        self._disp.set_render_mode(lv.DISPLAY_RENDER_MODE.PARTIAL)
        self._disp.set_color_format(lv.COLOR_FORMAT.RGB565)
        if rounder_cb:
            try:
                self._disp.add_event_cb(rounder_cb, lv.EVENT.INVALIDATE_AREA, None)
            except Exception as e:
                print("Display rounder registration failed:", e)

    def _register_stub_display(self):
        """Software-only display for UI testing without hardware."""
        buf_size = LCD_WIDTH * _DRAW_BUF_ROWS
        self._buf1 = lv.draw_buf_create(buf_size, 1, lv.COLOR_FORMAT.RGB565, 0)

        self._disp = lv.display_create(LCD_WIDTH, LCD_HEIGHT)

        def stub_flush(disp, area, buf):
            disp.flush_ready()

        self._disp.set_flush_cb(stub_flush)
        self._disp.set_draw_buffers(self._buf1, None)
        self._disp.set_render_mode(lv.DISPLAY_RENDER_MODE.PARTIAL)

    def set_brightness(self, level):
        """level: 0-255"""
        pass  # Handled by AXP2101 DLDO
