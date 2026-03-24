# screens/recorder.py - Audio recorder with waveform visualization
import lvgl as lv
import time
from screen_manager import ScreenModule, SCREEN_RECORDER

_BAR_COUNT     = 32
_SAMPLE_RATE   = 16000
_RECORD_PATH   = "/recordings/last_take.pcm"


def _lv_color(r, g, b):
    return lv.color_make(r, g, b)


class RecorderScreen(ScreenModule):
    def __init__(self, codec=None):
        super().__init__(SCREEN_RECORDER, "Recorder")
        self._codec = codec
        self._scr = None
        self._bars = []
        self._status_label = None
        self._time_label = None
        self._record_btn = None
        self._play_btn = None
        self._recording = False
        self._playing = False
        self._start_time = 0
        self._i2s = None
        self._file = None
        self._bar_heights = [0] * _BAR_COUNT

    def build(self):
        self._scr = lv.obj(None)
        self._scr.set_style_bg_color(_lv_color(4, 6, 10), 0)
        self._scr.clear_flag(lv.obj.FLAG.SCROLLABLE)

        title = lv.label(self._scr)
        title.set_style_text_font(lv.font_montserrat_18, 0)
        title.set_style_text_color(_lv_color(220, 80, 80), 0)
        title.set_text("Recorder")
        title.align(lv.ALIGN.TOP_MID, 0, 12)

        # Waveform bars
        bar_w = 8
        gap   = 2
        total_w = _BAR_COUNT * (bar_w + gap)
        start_x = (368 - total_w) // 2

        for i in range(_BAR_COUNT):
            bar = lv.obj(self._scr)
            bar.set_size(bar_w, 2)
            bar.set_style_bg_color(_lv_color(220, 80, 80), 0)
            bar.set_style_radius(2, 0)
            bar.set_style_border_width(0, 0)
            bar.set_pos(start_x + i * (bar_w + gap), 224)
            self._bars.append(bar)

        # Timer
        self._time_label = lv.label(self._scr)
        self._time_label.set_style_text_font(lv.font_montserrat_24, 0)
        self._time_label.set_style_text_color(_lv_color(200, 200, 200), 0)
        self._time_label.set_text("0:00")
        self._time_label.align(lv.ALIGN.CENTER, 0, 70)

        # Status
        self._status_label = lv.label(self._scr)
        self._status_label.set_style_text_font(lv.font_montserrat_14, 0)
        self._status_label.set_style_text_color(_lv_color(120, 120, 120), 0)
        self._status_label.set_text("Ready")
        self._status_label.align(lv.ALIGN.CENTER, 0, 100)

        # Record button
        self._record_btn = lv.btn(self._scr)
        self._record_btn.set_size(120, 48)
        self._record_btn.set_style_bg_color(_lv_color(180, 30, 30), 0)
        self._record_btn.set_style_radius(24, 0)
        self._record_btn.align(lv.ALIGN.BOTTOM_MID, -70, -30)
        rec_lbl = lv.label(self._record_btn)
        rec_lbl.set_text("Record")
        rec_lbl.center()
        self._record_btn.add_event_cb(
            lambda e: self._toggle_record(), lv.EVENT.CLICKED, None
        )

        # Playback button
        self._play_btn = lv.btn(self._scr)
        self._play_btn.set_size(120, 48)
        self._play_btn.set_style_bg_color(_lv_color(30, 100, 180), 0)
        self._play_btn.set_style_radius(24, 0)
        self._play_btn.align(lv.ALIGN.BOTTOM_MID, 70, -30)
        play_lbl = lv.label(self._play_btn)
        play_lbl.set_text("Play")
        play_lbl.center()
        self._play_btn.add_event_cb(
            lambda e: self._toggle_play(), lv.EVENT.CLICKED, None
        )

        self._root = self._scr
        return True

    def _toggle_record(self):
        if self._playing:
            return
        if self._recording:
            self._stop_record()
        else:
            self._start_record()

    def _start_record(self):
        try:
            import machine
            from config import PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DIN, PIN_I2S_MCLK
            self._i2s = machine.I2S(
                0,
                sck=machine.Pin(PIN_I2S_BCLK),
                ws=machine.Pin(PIN_I2S_WS),
                sd=machine.Pin(PIN_I2S_DIN),
                mode=machine.I2S.RX,
                bits=16,
                format=machine.I2S.MONO,
                rate=_SAMPLE_RATE,
                ibuf=4096,
            )
            import os
            try:
                os.mkdir("/recordings")
            except OSError:
                pass
            self._file = open(_RECORD_PATH, "wb")
            self._recording = True
            self._start_time = time.ticks_ms()
            self._status_label.set_text("Recording...")
            self._record_btn.set_style_bg_color(_lv_color(220, 50, 50), 0)
        except Exception as e:
            self._status_label.set_text(f"Error: {e}")

    def _stop_record(self):
        self._recording = False
        if self._file:
            self._file.close()
            self._file = None
        if self._i2s:
            self._i2s.deinit()
            self._i2s = None
        self._status_label.set_text("Saved")
        self._record_btn.set_style_bg_color(_lv_color(180, 30, 30), 0)

    def _toggle_play(self):
        if self._recording:
            return
        # Playback stub – would need I2S TX + ES8311 DAC
        self._status_label.set_text("Playback not yet implemented")

    def tick(self, elapsed_ms):
        import random

        if self._recording:
            # Read audio chunk and visualize
            elapsed_s = time.ticks_diff(time.ticks_ms(), self._start_time) // 1000
            self._time_label.set_text(f"{elapsed_s // 60}:{elapsed_s % 60:02d}")

            if self._i2s and self._file:
                try:
                    buf = bytearray(512)
                    n = self._i2s.readinto(buf)
                    self._file.write(buf[:n])
                    # Compute bar levels from samples
                    for b in range(_BAR_COUNT):
                        off = (n // _BAR_COUNT) * b
                        if off + 1 < n:
                            sample = (buf[off+1] << 8 | buf[off]) - 32768
                            level = abs(sample) // 256
                            self._bar_heights[b] = max(2, min(100, level))
                        else:
                            self._bar_heights[b] = 2
                except Exception:
                    pass
        else:
            # Idle decay
            for i in range(_BAR_COUNT):
                if self._bar_heights[i] > 2:
                    self._bar_heights[i] = max(2, self._bar_heights[i] - 3)

        # Update bar widgets
        for i, (bar, h) in enumerate(zip(self._bars, self._bar_heights)):
            bar.set_size(8, h)
            bar.set_y(224 - h // 2)

    def root(self):
        return self._root
