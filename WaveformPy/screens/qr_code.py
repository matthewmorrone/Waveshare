# screens/qr_code.py - QR code generator with multiple entries
import lvgl as lv
from screen_manager import ScreenModule, SCREEN_QR

# Entries to display as QR codes (customize these)
_ENTRIES = [
    ("GitHub",    "https://github.com"),
    ("Homepage",  "https://example.com"),
    ("WiFi",      "WIFI:S:MySSID;T:WPA;P:MyPassword;;"),
    ("Email",     "mailto:user@example.com"),
    ("Contact",   "BEGIN:VCARD\nFN:Your Name\nEND:VCARD"),
]


def _lv_color(r, g, b):
    return lv.color_make(r, g, b)


class QRScreen(ScreenModule):
    def __init__(self):
        super().__init__(SCREEN_QR, "QR Code")
        self._scr = None
        self._qr_obj = None
        self._title_label = None
        self._idx = 0

    def build(self):
        self._scr = lv.obj(None)
        self._scr.set_style_bg_color(_lv_color(0, 0, 0), 0)
        self._scr.clear_flag(lv.obj.FLAG.SCROLLABLE)

        # Title
        self._title_label = lv.label(self._scr)
        self._title_label.set_style_text_font(lv.font_montserrat_18, 0)
        self._title_label.set_style_text_color(_lv_color(255, 255, 255), 0)
        self._title_label.set_text(_ENTRIES[0][0])
        self._title_label.align(lv.ALIGN.TOP_MID, 0, 16)

        # QR code widget
        self._qr_obj = lv.qrcode(self._scr, 240, _lv_color(255, 255, 255), _lv_color(0, 0, 0))
        self._qr_obj.align(lv.ALIGN.CENTER, 0, 10)
        self._qr_obj.update(_ENTRIES[0][1].encode())

        # Nav hint
        nav = lv.label(self._scr)
        nav.set_style_text_font(lv.font_montserrat_12, 0)
        nav.set_style_text_color(_lv_color(80, 80, 80), 0)
        nav.set_text("Swipe to change")
        nav.align(lv.ALIGN.BOTTOM_MID, 0, -16)

        # Swipe detection
        self._scr.add_event_cb(self._on_gesture, lv.EVENT.GESTURE, None)

        self._root = self._scr
        return True

    def _on_gesture(self, e):
        indev = lv.indev_active()
        if indev is None:
            return
        gesture = indev.get_gesture_dir()
        if gesture == lv.DIR.LEFT:
            self._next_entry()
        elif gesture == lv.DIR.RIGHT:
            self._prev_entry()

    def _next_entry(self):
        self._idx = (self._idx + 1) % len(_ENTRIES)
        self._update_qr()

    def _prev_entry(self):
        self._idx = (self._idx - 1) % len(_ENTRIES)
        self._update_qr()

    def _update_qr(self):
        name, data = _ENTRIES[self._idx]
        self._title_label.set_text(name)
        self._qr_obj.update(data.encode())

    def root(self):
        return self._root
