# screens/sky.py - Procedural starfield based on device orientation
import lvgl as lv
import math
import random
from screen_manager import ScreenModule, SCREEN_SKY

_STAR_COUNT = 20


def _lv_color(r, g, b):
    return lv.color_make(r, g, b)


class SkyScreen(ScreenModule):
    def __init__(self, imu=None):
        super().__init__(SCREEN_SKY, "Sky")
        self._imu = imu
        self._scr = None
        self._star_dots = []
        self._stars = []   # [(ra, dec, magnitude)]
        self._pitch = 0.0
        self._roll  = 0.0
        self._title_label = None

    def build(self):
        self._scr = lv.obj(None)
        self._scr.set_style_bg_color(_lv_color(0, 0, 4), 0)
        self._scr.clear_flag(lv.obj.FLAG.SCROLLABLE)

        self._title_label = lv.label(self._scr)
        self._title_label.set_style_text_font(lv.font_montserrat_14, 0)
        self._title_label.set_style_text_color(_lv_color(60, 60, 80), 0)
        self._title_label.set_text("Sky View")
        self._title_label.align(lv.ALIGN.TOP_MID, 0, 10)

        # Generate stars with (RA 0-360, Dec -90 to 90, magnitude 1-5)
        random.seed(42)
        for _ in range(_STAR_COUNT):
            ra  = random.uniform(0, 360)
            dec = random.uniform(-60, 60)
            mag = random.uniform(1, 5)
            self._stars.append((ra, dec, mag))

        # Create dot objects for each star
        for ra, dec, mag in self._stars:
            size = max(2, int(6 - mag))
            brightness = int(255 * (1 - (mag - 1) / 4))
            dot = lv.obj(self._scr)
            dot.set_size(size, size)
            dot.set_style_bg_color(_lv_color(brightness, brightness, brightness), 0)
            dot.set_style_radius(size // 2, 0)
            dot.set_style_border_width(0, 0)
            dot.set_pos(0, 0)
            self._star_dots.append(dot)

        self._root = self._scr
        return True

    def enter(self):
        self._regenerate(0.0, 0.0)

    def _regenerate(self, pitch, roll):
        """Project stars onto screen based on viewing direction."""
        W, H = 368, 448
        cx, cy = W // 2, H // 2

        # Viewing direction as unit vector from pitch/roll
        pitch_r = math.radians(pitch)
        roll_r  = math.radians(roll)

        # Simple gnomonic projection
        for i, ((ra, dec, mag), dot) in enumerate(zip(self._stars, self._star_dots)):
            ra_r  = math.radians(ra + roll * 2)
            dec_r = math.radians(dec + pitch)

            # Project to screen
            cos_c = (math.sin(pitch_r) * math.sin(dec_r) +
                     math.cos(pitch_r) * math.cos(dec_r) * math.cos(ra_r - roll_r))
            if cos_c <= 0:
                dot.add_flag(lv.obj.FLAG.HIDDEN)
                continue

            dot.clear_flag(lv.obj.FLAG.HIDDEN)
            x = int(cx + cx * math.cos(dec_r) * math.sin(ra_r - roll_r) / cos_c)
            y = int(cy - cy * (math.cos(pitch_r) * math.sin(dec_r) -
                                math.sin(pitch_r) * math.cos(dec_r) * math.cos(ra_r - roll_r)) / cos_c)
            dot.set_pos(x, y)

    def tick(self, elapsed_ms):
        if self._imu:
            try:
                pitch, roll = self._imu.pitch_roll()
                self._pitch = pitch
                self._roll  = roll
            except Exception:
                pass
        self._regenerate(self._pitch, self._roll)

    def root(self):
        return self._root
