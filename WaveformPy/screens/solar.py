# screens/solar.py - Solar system visualization with orbital mechanics
import lvgl as lv
import math
import time
from screen_manager import ScreenModule, SCREEN_SOLAR


def _lv_color(r, g, b):
    return lv.color_make(r, g, b)


# Planet data: (name, orbital_period_days, color_rgb, size_px, orbit_radius_px)
_PLANETS = [
    ("Mercury",  87.97, (180, 160, 140),  4,  50),
    ("Venus",   224.70, (220, 190, 100),  7,  75),
    ("Earth",   365.25, ( 80, 130, 200),  8, 100),
    ("Mars",    686.97, (200,  80,  60),  6, 125),
    ("Jupiter", 4332.6, (200, 160, 120), 14, 165),
    ("Saturn",  10759., (220, 200, 140), 12, 210),
    ("Uranus",  30688., (150, 210, 220),  9, 250),
    ("Neptune", 60182., ( 60, 100, 200),  9, 285),
]

# J2000 epoch Jan 1.5 2000 = Julian Day 2451545.0
_J2000_UNIX = 946728000  # Unix timestamp of J2000.0


def _planet_angle(period_days, unix_now):
    """Return current orbital angle in radians."""
    days_since_j2000 = (unix_now - _J2000_UNIX) / 86400
    return (2 * math.pi * days_since_j2000 / period_days) % (2 * math.pi)


class SolarScreen(ScreenModule):
    def __init__(self):
        super().__init__(SCREEN_SOLAR, "Solar")
        self._scr = None
        self._canvas = None
        self._planet_dots = []
        self._planet_labels = []
        self._orbit_rings = []

    def build(self):
        self._scr = lv.obj(None)
        self._scr.set_style_bg_color(_lv_color(0, 0, 8), 0)
        self._scr.clear_flag(lv.obj.FLAG.SCROLLABLE)

        title = lv.label(self._scr)
        title.set_style_text_font(lv.font_montserrat_18, 0)
        title.set_style_text_color(_lv_color(180, 180, 255), 0)
        title.set_text("Solar System")
        title.align(lv.ALIGN.TOP_MID, 0, 10)

        # Center X/Y for the solar system (slightly above screen center)
        cx, cy = 184, 210

        # Sun
        sun = lv.obj(self._scr)
        sun.set_size(20, 20)
        sun.set_style_bg_color(_lv_color(255, 220, 60), 0)
        sun.set_style_radius(10, 0)
        sun.set_style_border_width(0, 0)
        sun.set_pos(cx - 10, cy - 10)

        # Orbit rings
        for name, period, color, size, orbit_r in _PLANETS:
            ring = lv.obj(self._scr)
            ring.set_size(orbit_r * 2, orbit_r * 2)
            ring.set_style_bg_opa(lv.OPA.TRANSP, 0)
            ring.set_style_border_color(_lv_color(30, 30, 50), 0)
            ring.set_style_border_width(1, 0)
            ring.set_style_radius(orbit_r, 0)
            ring.set_pos(cx - orbit_r, cy - orbit_r)
            self._orbit_rings.append(ring)

            # Planet dot
            dot = lv.obj(self._scr)
            dot.set_size(size, size)
            dot.set_style_bg_color(_lv_color(*color), 0)
            dot.set_style_radius(size // 2, 0)
            dot.set_style_border_width(0, 0)
            dot.set_pos(cx - size // 2, cy - orbit_r - size // 2)
            self._planet_dots.append((dot, orbit_r, size, cx, cy))

        self._root = self._scr
        return True

    def tick(self, elapsed_ms):
        now = time.time()
        for i, (planet, (dot, orbit_r, size, cx, cy)) in enumerate(
            zip(_PLANETS, self._planet_dots)
        ):
            name, period, color, _, _ = planet
            angle = _planet_angle(period, now)
            px = cx + int(orbit_r * math.cos(angle)) - size // 2
            py = cy + int(orbit_r * math.sin(angle)) - size // 2
            dot.set_pos(px, py)

    def root(self):
        return self._root
