# screens/geo.py - Geolocation display
import lvgl as lv
from screen_manager import ScreenModule, SCREEN_GEO


def _lv_color(r, g, b):
    return lv.color_make(r, g, b)


class GeoScreen(ScreenModule):
    def __init__(self):
        super().__init__(SCREEN_GEO, "Geo")
        self._scr = None
        self._labels = {}

    def build(self):
        self._scr = lv.obj(None)
        self._scr.set_style_bg_color(_lv_color(0, 8, 16), 0)
        self._scr.clear_flag(lv.obj.FLAG.SCROLLABLE)

        title = lv.label(self._scr)
        title.set_style_text_font(lv.font_montserrat_24, 0)
        title.set_style_text_color(_lv_color(100, 200, 255), 0)
        title.set_text("Location")
        title.align(lv.ALIGN.TOP_MID, 0, 20)

        rows = [
            ("city",    "City",      40),
            ("region",  "Region",    80),
            ("country", "Country",  120),
            ("ip",      "IP",       160),
            ("tz",      "Timezone", 200),
            ("lat",     "Lat",      240),
            ("lon",     "Lon",      280),
            ("zip",     "ZIP",      320),
        ]
        for key, name, y in rows:
            lbl = lv.label(self._scr)
            lbl.set_style_text_font(lv.font_montserrat_14, 0)
            lbl.set_style_text_color(_lv_color(120, 120, 120), 0)
            lbl.set_text(f"{name}: --")
            lbl.align(lv.ALIGN.TOP_MID, 0, y)
            self._labels[key] = lbl

        self._root = self._scr
        return True

    def tick(self, elapsed_ms):
        from modules import geo_fetch
        g = geo_fetch.get_state()
        if not g["ok"]:
            self._labels["city"].set_text("Locating...")
            return
        self._labels["city"].set_text(f"City: {g['city']}")
        self._labels["region"].set_text(f"Region: {g['region']}")
        self._labels["country"].set_text(f"Country: {g['country']} ({g['country_code']})")
        self._labels["ip"].set_text(f"IP: {g['ip']}")
        self._labels["tz"].set_text(f"Timezone: {g['timezone']}")
        self._labels["lat"].set_text(f"Lat: {g['lat']:.4f}")
        self._labels["lon"].set_text(f"Lon: {g['lon']:.4f}")
        self._labels["zip"].set_text(f"ZIP: {g['zip']}")

    def root(self):
        return self._root
