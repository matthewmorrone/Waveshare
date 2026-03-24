# screens/watch.py - Digital watch with time, date, battery, status icons
import lvgl as lv
import time
from screen_manager import ScreenModule, SCREEN_WATCH

_DAYS   = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"]
_MONTHS = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"]


def _lv_color(r, g, b):
    return lv.color_make(r, g, b)


class WatchScreen(ScreenModule):
    def __init__(self):
        super().__init__(SCREEN_WATCH, "Watch")
        self._scr = None
        self._time_label = None
        self._date_label = None
        self._bat_bar = None
        self._bat_label = None
        self._wifi_icon = None
        self._status_label = None

    def build(self):
        self._scr = lv.obj(None)
        self._scr.set_style_bg_color(_lv_color(0, 0, 0), 0)
        self._scr.set_style_bg_opa(lv.OPA.COVER, 0)
        self._scr.clear_flag(lv.obj.FLAG.SCROLLABLE)

        # Time display (large)
        self._time_label = lv.label(self._scr)
        self._time_label.set_style_text_font(lv.font_montserrat_48, 0)
        self._time_label.set_style_text_color(_lv_color(255, 255, 255), 0)
        self._time_label.set_text("00:00")
        self._time_label.align(lv.ALIGN.TOP_MID, 0, 58)

        # Seconds label
        self._sec_label = lv.label(self._scr)
        self._sec_label.set_style_text_font(lv.font_montserrat_24, 0)
        self._sec_label.set_style_text_color(_lv_color(120, 120, 120), 0)
        self._sec_label.set_text("00")
        self._sec_label.align(lv.ALIGN.TOP_MID, 0, 120)

        # Date label
        self._date_label = lv.label(self._scr)
        self._date_label.set_style_text_font(lv.font_montserrat_18, 0)
        self._date_label.set_style_text_color(_lv_color(160, 160, 180), 0)
        self._date_label.set_text("Mon, Jan 1 2024")
        self._date_label.align(lv.ALIGN.TOP_MID, 0, 172)

        # Battery bar track
        bat_track = lv.obj(self._scr)
        bat_track.set_size(200, 8)
        bat_track.set_style_bg_color(_lv_color(30, 30, 30), 0)
        bat_track.set_style_border_width(0, 0)
        bat_track.set_style_radius(lv.RADIUS_CIRCLE, 0)
        bat_track.align(lv.ALIGN.TOP_MID, 0, 358)

        # Battery bar fill
        self._bat_bar = lv.bar(self._scr)
        self._bat_bar.set_size(200, 8)
        self._bat_bar.set_range(0, 100)
        self._bat_bar.set_value(80, lv.ANIM.OFF)
        self._bat_bar.set_style_bg_color(_lv_color(30, 30, 30), 0)
        self._bat_bar.set_style_bg_color(_lv_color(52, 200, 100), lv.PART.INDICATOR)
        self._bat_bar.set_style_radius(lv.RADIUS_CIRCLE, 0)
        self._bat_bar.align(lv.ALIGN.TOP_MID, 0, 358)

        # Battery percent label
        self._bat_label = lv.label(self._scr)
        self._bat_label.set_style_text_font(lv.font_montserrat_14, 0)
        self._bat_label.set_style_text_color(_lv_color(120, 120, 120), 0)
        self._bat_label.set_text("80%")
        self._bat_label.align(lv.ALIGN.TOP_MID, 0, 370)

        # Status row (WiFi, charging)
        self._status_label = lv.label(self._scr)
        self._status_label.set_style_text_font(lv.font_montserrat_14, 0)
        self._status_label.set_style_text_color(_lv_color(80, 80, 80), 0)
        self._status_label.set_text("No WiFi")
        self._status_label.align(lv.ALIGN.TOP_MID, 0, 390)

        self._root = self._scr
        return True

    def tick(self, elapsed_ms):
        import state
        # Update time
        t = time.localtime()
        h, m, s = t[3], t[4], t[5]
        self._time_label.set_text(f"{h:02d}:{m:02d}")
        self._sec_label.set_text(f"{s:02d}")
        self._date_label.set_text(
            f"{_DAYS[t[6]]}, {_MONTHS[t[1]-1]} {t[2]} {t[0]}"
        )

        # Update battery
        pct = state.battery_pct
        self._bat_bar.set_value(pct, lv.ANIM.OFF)
        self._bat_label.set_text(f"{pct}%")

        # Status
        parts = []
        if state.wifi_ok:
            parts.append("WiFi")
        if state.usb_ok:
            parts.append("USB")
        if state.charging:
            parts.append("CHG")
        self._status_label.set_text("  ".join(parts) if parts else "")

    def root(self):
        return self._root
