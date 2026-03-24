# drivers/es8311.py - ES8311 audio codec I2C control
# I2S audio data handled separately via machine.I2S

_ADDR = 0x18

# Key registers
_REG_RESET     = 0x00
_REG_CLK_MAN1  = 0x01
_REG_CLK_MAN2  = 0x02
_REG_CLK_MAN3  = 0x03
_REG_CLK_MAN4  = 0x04
_REG_CLK_MAN5  = 0x05
_REG_CLK_MAN6  = 0x06
_REG_CLK_MAN7  = 0x07
_REG_ADC_CTRL  = 0x40
_REG_ADC_MUTE  = 0x17
_REG_ADC_VOL   = 0x17
_REG_MIC_GAIN  = 0x16
_REG_DAC_VOL   = 0x32
_REG_SYS1      = 0x0D
_REG_SYS2      = 0x0E
_REG_SYS3      = 0x0F
_REG_CHD1      = 0xFD
_REG_CHD2      = 0xFE


class ES8311:
    def __init__(self, i2c, addr=_ADDR):
        self._i2c = i2c
        self._addr = addr

    def _read(self, reg):
        return self._i2c.readfrom_mem(self._addr, reg, 1)[0]

    def _write(self, reg, val):
        self._i2c.writeto_mem(self._addr, reg, bytes([val]))

    def begin(self, sample_rate=16000):
        """Initialize codec for microphone input at given sample rate."""
        # Software reset
        self._write(_REG_RESET, 0x1F)
        import time; time.sleep_ms(10)
        self._write(_REG_RESET, 0x00)

        # Clock: MCLK=256*Fs, internal clock divider
        self._write(_REG_CLK_MAN1, 0x30)  # MCLK from pin
        self._write(_REG_CLK_MAN2, 0x10)
        self._write(_REG_CLK_MAN3, 0x10)  # Fsync = LRCK
        self._write(_REG_CLK_MAN4, 0x02)  # BCLK divider
        self._write(_REG_CLK_MAN5, 0x00)
        self._write(_REG_CLK_MAN6, 0x03)
        self._write(_REG_CLK_MAN7, 0x00)

        # System: power on ADC
        self._write(_REG_SYS1, 0x3F)
        self._write(_REG_SYS2, 0x00)
        self._write(_REG_SYS3, 0x20)

        # ADC: enable, select MIC
        self._write(_REG_ADC_CTRL, 0x00)

        # Default gain: 18 dB
        self.set_mic_gain(18)

    def set_mic_gain(self, db):
        """Set microphone gain 0-42 dB in 3 dB steps."""
        steps = min(14, max(0, db // 3))
        self._write(_REG_MIC_GAIN, steps | 0x00)

    def set_dac_volume(self, vol):
        """vol: 0-255"""
        self._write(_REG_DAC_VOL, vol)

    def mute_adc(self, mute):
        val = self._read(_REG_ADC_MUTE)
        if mute:
            self._write(_REG_ADC_MUTE, val | 0x40)
        else:
            self._write(_REG_ADC_MUTE, val & ~0x40)

    def chip_id(self):
        return (self._read(_REG_CHD1) << 8) | self._read(_REG_CHD2)
