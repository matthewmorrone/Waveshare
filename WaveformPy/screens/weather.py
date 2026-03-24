# screens/weather.py - Weather display with animated conditions
import lvgl as lv
import time
import math
from screen_manager import ScreenModule, SCREEN_WEATHER


def _lv_color(r, g, b):
    return lv.color_make(r, g, b)

# Condition -> (sky top color, sky bottom color, accent)
_CONDITION_STYLES = {
    "Clear":        (_lv_color(10,20,80),  _lv_color(30,100,220), _lv_color(255,220,80)),
    "Partly Cloudy":(_lv_color(20,30,80),  _lv_color(60,100,180), _lv_color(200,200,200)),
    "Cloudy":       (_lv_color(40,40,50),  _lv_color(70,70,80),   _lv_color(180,180,180)),
    "Rain":         (_lv_color(20,30,50),  _lv_color(40,50,70),   _lv_color(120,160,220)),
    "Snow":         (_lv_color(140,160,200),_lv_color(200,210,230),_lv_color(255,255,255)),
    "Storm":        (_lv_color(15,15,25),  _lv_color(30,30,50),   _lv_color(255,200,0)),
    "Fog":          (_lv_color(90,90,100), _lv_color(150,150,160),_lv_color(200,200,200)),
}

_MOON_PHASES = ["New", "Waxing Crescent", "First Quarter", "Waxing Gibbous",
                "Full", "Waning Gibbous", "Last Quarter", "Waning Crescent"]


class WeatherScreen(ScreenModule):
    def __init__(self):
        super().__init__(SCREEN_WEATHER, "Weather")
        self._scr = None
        self._cond_label = None
        self._temp_label = None
        self._detail_label = None
        self._forecast_labels = []
        self._moon_label = None
        self._anim_tick = 0
        self._particles = []  # [(x, y, dy)] for rain/snow

    def build(self):
        self._scr = lv.obj(None)
        self._scr.set_style_bg_color(_lv_color(10, 20, 80), 0)
        self._scr.set_style_bg_opa(lv.OPA.COVER, 0)
        self._scr.clear_flag(lv.obj.FLAG.SCROLLABLE)

        # Condition label
        self._cond_label = lv.label(self._scr)
        self._cond_label.set_style_text_font(lv.font_montserrat_24, 0)
        self._cond_label.set_style_text_color(_lv_color(255, 255, 255), 0)
        self._cond_label.set_text("--")
        self._cond_label.align(lv.ALIGN.TOP_MID, 0, 20)

        # Temperature
        self._temp_label = lv.label(self._scr)
        self._temp_label.set_style_text_font(lv.font_montserrat_48, 0)
        self._temp_label.set_style_text_color(_lv_color(255, 255, 255), 0)
        self._temp_label.set_text("--°")
        self._temp_label.align(lv.ALIGN.TOP_MID, 0, 56)

        # Detail row (humidity, wind)
        self._detail_label = lv.label(self._scr)
        self._detail_label.set_style_text_font(lv.font_montserrat_14, 0)
        self._detail_label.set_style_text_color(_lv_color(160, 200, 255), 0)
        self._detail_label.set_text("--% RH  --mph")
        self._detail_label.align(lv.ALIGN.TOP_MID, 0, 118)

        # Sunrise/sunset
        self._sun_label = lv.label(self._scr)
        self._sun_label.set_style_text_font(lv.font_montserrat_14, 0)
        self._sun_label.set_style_text_color(_lv_color(255, 200, 80), 0)
        self._sun_label.set_text("Rise -- / Set --")
        self._sun_label.align(lv.ALIGN.TOP_MID, 0, 140)

        # Moon phase
        self._moon_label = lv.label(self._scr)
        self._moon_label.set_style_text_font(lv.font_montserrat_14, 0)
        self._moon_label.set_style_text_color(_lv_color(180, 180, 220), 0)
        self._moon_label.set_text("Moon: --")
        self._moon_label.align(lv.ALIGN.TOP_MID, 0, 162)

        # 5-day forecast row
        self._build_forecast_row()

        # Particle container for rain/snow animations
        self._particle_container = lv.obj(self._scr)
        self._particle_container.set_size(368, 200)
        self._particle_container.align(lv.ALIGN.TOP_MID, 0, 190)
        self._particle_container.set_style_bg_opa(lv.OPA.TRANSP, 0)
        self._particle_container.set_style_border_width(0, 0)
        self._particle_container.clear_flag(lv.obj.FLAG.SCROLLABLE)
        self._particle_dots = []

        self._root = self._scr
        return True

    def _build_forecast_row(self):
        DAYS = ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"]
        today_wd = time.localtime()[6]
        for i in range(5):
            day_name = DAYS[(today_wd + i) % 7]
            col = lv.obj(self._scr)
            col.set_size(64, 80)
            col.align(lv.ALIGN.BOTTOM_MID, -128 + i * 64, -20)
            col.set_style_bg_color(_lv_color(10, 15, 30), 0)
            col.set_style_radius(8, 0)
            col.set_style_border_width(0, 0)
            col.clear_flag(lv.obj.FLAG.SCROLLABLE)

            day_l = lv.label(col)
            day_l.set_style_text_font(lv.font_montserrat_12, 0)
            day_l.set_style_text_color(_lv_color(120, 120, 120), 0)
            day_l.set_text(day_name)
            day_l.align(lv.ALIGN.TOP_MID, 0, 4)

            hi_l = lv.label(col)
            hi_l.set_style_text_font(lv.font_montserrat_14, 0)
            hi_l.set_style_text_color(_lv_color(255, 160, 80), 0)
            hi_l.set_text("--")
            hi_l.align(lv.ALIGN.CENTER, 0, -6)

            lo_l = lv.label(col)
            lo_l.set_style_text_font(lv.font_montserrat_12, 0)
            lo_l.set_style_text_color(_lv_color(100, 160, 255), 0)
            lo_l.set_text("--")
            lo_l.align(lv.ALIGN.CENTER, 0, 12)

            self._forecast_labels.append((hi_l, lo_l))

    def enter(self):
        self._init_particles()

    def _init_particles(self):
        import random
        from modules import weather_fetch
        w = weather_fetch.get_state()
        cond = w.get("condition", "Clear")
        n = 0
        if cond == "Rain":   n = 20
        if cond == "Snow":   n = 16
        if cond == "Storm":  n = 24

        # Clear old
        for dot in self._particle_dots:
            dot.del_async()
        self._particle_dots.clear()
        self._particles.clear()

        for _ in range(n):
            x = random.randint(0, 367)
            y = random.randint(0, 200)
            dy = random.uniform(1.5, 4.0) if cond != "Snow" else random.uniform(0.5, 1.5)
            self._particles.append([x, y, dy])
            dot = lv.obj(self._particle_container)
            dot.set_size(3 if cond == "Snow" else 2, 8 if cond == "Rain" else 3)
            dot.set_style_bg_color(
                _lv_color(120,160,220) if cond in ("Rain","Storm")
                else _lv_color(220,230,255), 0
            )
            dot.set_style_radius(lv.RADIUS_CIRCLE, 0)
            dot.set_style_border_width(0, 0)
            dot.set_pos(x, y)
            self._particle_dots.append(dot)

    def tick(self, elapsed_ms):
        from modules import weather_fetch
        w = weather_fetch.get_state()
        if not w["ok"]:
            return

        cond = w["condition"]
        style = _CONDITION_STYLES.get(cond, _CONDITION_STYLES["Clear"])
        self._scr.set_style_bg_color(style[1], 0)

        self._cond_label.set_text(cond)
        self._temp_label.set_text(f"{w['temp_f']:.0f}°")
        self._detail_label.set_text(f"{w['humidity']}% RH  {w['wind_mph']:.0f}mph")
        self._sun_label.set_text(f"Rise {w['sunrise']} / Set {w['sunset']}")

        # Moon phase name
        phase_idx = int(w["moon_phase"] * 8) % 8
        self._moon_label.set_text(f"Moon: {_MOON_PHASES[phase_idx]}")

        # Forecast
        for i, (hi_l, lo_l) in enumerate(self._forecast_labels):
            if i < len(w["daily_max"]):
                hi_l.set_text(f"{w['daily_max'][i]:.0f}°")
                lo_l.set_text(f"{w['daily_min'][i]:.0f}°")

        # Animate particles
        self._anim_tick += elapsed_ms
        for i, (p, dot) in enumerate(zip(self._particles, self._particle_dots)):
            p[1] += p[2] * elapsed_ms / 16
            if p[1] > 200:
                p[1] = 0
            dot.set_pos(int(p[0]), int(p[1]))

    def root(self):
        return self._root
