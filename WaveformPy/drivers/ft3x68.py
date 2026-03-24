# drivers/ft3x68.py - FT3x68 capacitive touch controller driver

import lvgl as lv

_REG_TD_STATUS = 0x02  # Touch point count
_REG_TOUCH1    = 0x03  # First touch point data (4 bytes)
_MAX_TOUCH_PTS = 5


class TouchPoint:
    def __init__(self, x, y, event, tid):
        self.x = x
        self.y = y
        self.event = event  # 0=down, 1=up, 2=contact
        self.id = tid


class FT3x68:
    def __init__(self, i2c, addr=0x38):
        self._i2c = i2c
        self._addr = addr
        self._last_x = 0
        self._last_y = 0
        self._last_pressed = False

    def _read(self, reg, n=1):
        return self._i2c.readfrom_mem(self._addr, reg, n)

    def get_touches(self):
        """Returns list of TouchPoint objects."""
        try:
            status = self._read(_REG_TD_STATUS)[0]
            count = status & 0x0F
            if count == 0 or count > _MAX_TOUCH_PTS:
                return []
            buf = self._read(_REG_TOUCH1, count * 6)
            touches = []
            for i in range(count):
                o = i * 6
                event = (buf[o] >> 6) & 0x03
                x = ((buf[o] & 0x0F) << 8) | buf[o + 1]
                tid = (buf[o + 2] >> 4) & 0x0F
                y = ((buf[o + 2] & 0x0F) << 8) | buf[o + 3]
                touches.append(TouchPoint(x, y, event, tid))
            return touches
        except Exception:
            return []

    def get_point(self):
        """Returns (x, y, pressed) for single-touch use."""
        touches = self.get_touches()
        if touches:
            t = touches[0]
            self._last_x = t.x
            self._last_y = t.y
            self._last_pressed = (t.event != 1)
        else:
            self._last_pressed = False
        return self._last_x, self._last_y, self._last_pressed

    def lvgl_read_cb(self, indev, data):
        """LVGL input device read callback."""
        x, y, pressed = self.get_point()
        data.point = lv.point_t({'x': x, 'y': y})
        data.state = lv.INDEV_STATE.PRESSED if pressed else lv.INDEV_STATE.RELEASED
