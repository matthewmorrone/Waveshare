# drivers/xca9554.py - XCA9554 I2C GPIO expander driver

_REG_INPUT  = 0x00
_REG_OUTPUT = 0x01
_REG_POLARITY = 0x02
_REG_CONFIG = 0x03


class XCA9554:
    def __init__(self, i2c, addr=0x20):
        self._i2c = i2c
        self._addr = addr
        # Set all as inputs by default
        self._config = 0xFF
        self._output = 0x00
        self._write_reg(_REG_CONFIG, self._config)

    def _write_reg(self, reg, val):
        self._i2c.writeto_mem(self._addr, reg, bytes([val]))

    def _read_reg(self, reg):
        return self._i2c.readfrom_mem(self._addr, reg, 1)[0]

    def pin_mode(self, pin, mode):
        """mode: 0=output, 1=input"""
        if mode:
            self._config |= (1 << pin)
        else:
            self._config &= ~(1 << pin)
        self._write_reg(_REG_CONFIG, self._config)

    def digital_read(self, pin):
        val = self._read_reg(_REG_INPUT)
        return bool(val & (1 << pin))

    def digital_write(self, pin, value):
        if value:
            self._output |= (1 << pin)
        else:
            self._output &= ~(1 << pin)
        self._write_reg(_REG_OUTPUT, self._output)

    def read_all(self):
        return self._read_reg(_REG_INPUT)
