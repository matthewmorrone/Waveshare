# drivers/pcf85063.py - PCF85063 RTC driver

_REG_CTRL1   = 0x00
_REG_CTRL2   = 0x01
_REG_SECONDS = 0x04
_REG_MINUTES = 0x05
_REG_HOURS   = 0x06
_REG_DAYS    = 0x07
_REG_WEEKDAYS= 0x08
_REG_MONTHS  = 0x09
_REG_YEARS   = 0x0A


def _bcd_to_dec(b):
    return (b >> 4) * 10 + (b & 0x0F)

def _dec_to_bcd(d):
    return ((d // 10) << 4) | (d % 10)


class PCF85063:
    def __init__(self, i2c, addr=0x51):
        self._i2c = i2c
        self._addr = addr
        # Clear oscillator stop flag
        ctrl1 = self._read(_REG_CTRL1)
        if ctrl1 & 0x20:  # OS bit
            self._write(_REG_CTRL1, ctrl1 & ~0x20)

    def _read(self, reg):
        return self._i2c.readfrom_mem(self._addr, reg, 1)[0]

    def _write(self, reg, val):
        self._i2c.writeto_mem(self._addr, reg, bytes([val]))

    def datetime(self):
        """Return (year, month, day, weekday, hour, minute, second) tuple."""
        buf = self._i2c.readfrom_mem(self._addr, _REG_SECONDS, 7)
        sec  = _bcd_to_dec(buf[0] & 0x7F)
        min_ = _bcd_to_dec(buf[1] & 0x7F)
        hr   = _bcd_to_dec(buf[2] & 0x3F)
        day  = _bcd_to_dec(buf[3] & 0x3F)
        wday = _bcd_to_dec(buf[4] & 0x07)
        mon  = _bcd_to_dec(buf[5] & 0x1F)
        yr   = _bcd_to_dec(buf[6]) + 2000
        return (yr, mon, day, wday, hr, min_, sec, 0)

    def set_datetime(self, dt):
        """dt: (year, month, day, weekday, hour, minute, second)"""
        yr, mon, day, wday, hr, min_, sec = dt[0], dt[1], dt[2], dt[3], dt[4], dt[5], dt[6]
        buf = bytes([
            _dec_to_bcd(sec),
            _dec_to_bcd(min_),
            _dec_to_bcd(hr),
            _dec_to_bcd(day),
            _dec_to_bcd(wday),
            _dec_to_bcd(mon),
            _dec_to_bcd(yr - 2000),
        ])
        self._i2c.writeto_mem(self._addr, _REG_SECONDS, buf)

    def is_valid(self):
        """Check if RTC time is within valid range (2024-2099)."""
        try:
            dt = self.datetime()
            return 2024 <= dt[0] <= 2099
        except Exception:
            return False
