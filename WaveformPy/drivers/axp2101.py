# drivers/axp2101.py - AXP2101 Power Management Unit driver

# Key registers
_REG_STATUS0    = 0x00  # IRQ status
_REG_STATUS1    = 0x01
_REG_CHIP_ID    = 0x03
_REG_BAT_PERCENT= 0xA4  # Battery percentage (0-100)
_REG_BAT_VOL_H  = 0x34  # Battery voltage high byte
_REG_BAT_VOL_L  = 0x35
_REG_VBUS_VOL_H = 0x38
_REG_VBUS_VOL_L = 0x39
_REG_IRQ_EN0    = 0x40
_REG_IRQ_EN1    = 0x41
_REG_IRQ_EN2    = 0x42
_REG_IRQ_STAT0  = 0x48
_REG_IRQ_STAT1  = 0x49
_REG_IRQ_STAT2  = 0x4A
_REG_DCDC1_CTRL = 0x82
_REG_DCDC2_CTRL = 0x83
_REG_BLDO1_CTRL = 0x90
_REG_BLDO2_CTRL = 0x91
_REG_DL_LDO_CTRL= 0x99
_REG_PWROFF     = 0x10
_REG_CHGSTATUS  = 0x01


class AXP2101:
    def __init__(self, i2c, addr=0x34):
        self._i2c = i2c
        self._addr = addr

    def _read(self, reg):
        return self._i2c.readfrom_mem(self._addr, reg, 1)[0]

    def _write(self, reg, val):
        self._i2c.writeto_mem(self._addr, reg, bytes([val]))

    def _read16(self, reg_h, reg_l):
        h = self._read(reg_h)
        l = self._read(reg_l)
        return (h << 4) | (l & 0x0F)

    def battery_percent(self):
        return self._read(_REG_BAT_PERCENT)

    def battery_voltage_mv(self):
        raw = self._read16(_REG_BAT_VOL_H, _REG_BAT_VOL_L)
        return raw  # mV

    def vbus_voltage_mv(self):
        raw = self._read16(_REG_VBUS_VOL_H, _REG_VBUS_VOL_L)
        return raw

    def is_charging(self):
        status = self._read(_REG_STATUS1)
        return bool(status & 0x08)

    def is_usb_present(self):
        return self.vbus_voltage_mv() > 3000

    def power_off(self):
        val = self._read(_REG_PWROFF)
        self._write(_REG_PWROFF, val | 0x01)

    def clear_irq(self):
        self._write(_REG_IRQ_STAT0, 0xFF)
        self._write(_REG_IRQ_STAT1, 0xFF)
        self._write(_REG_IRQ_STAT2, 0xFF)

    def set_brightness(self, level):
        """Set display backlight brightness 0-255 via DLDO."""
        # Map 0-255 to voltage register range
        v = max(0, min(level, 255))
        # DLDO voltage range: 0.5V - 3.4V in 0.1V steps (0-29)
        reg_val = int(v * 29 / 255)
        self._write(_REG_DL_LDO_CTRL, (self._read(_REG_DL_LDO_CTRL) & 0xE0) | reg_val)
