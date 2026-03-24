# drivers/qmi8658.py - QMI8658 IMU driver (Accelerometer + Gyroscope)
import struct
import math

_REG_WHO_AM_I  = 0x00
_REG_CTRL1     = 0x02
_REG_CTRL2     = 0x03  # Accel ODR/range
_REG_CTRL3     = 0x04  # Gyro ODR/range
_REG_CTRL7     = 0x08  # Enable sensors
_REG_STATUS0   = 0x2E
_REG_AX_L      = 0x35
_REG_GX_L      = 0x3B

# Accelerometer full-scale range (±g)
ACCEL_RANGE_2G  = 0x00
ACCEL_RANGE_4G  = 0x10
ACCEL_RANGE_8G  = 0x20
ACCEL_RANGE_16G = 0x30

# Output data rate
ACCEL_ODR_1000 = 0x06
GYRO_ODR_1000  = 0x06


class QMI8658:
    def __init__(self, i2c, addr=0x6B):
        self._i2c = i2c
        self._addr = addr
        self._accel_scale = 8192.0   # LSB/g for ±4G
        self._gyro_scale  = 16.384   # LSB/dps for ±2000 dps
        self._init()

    def _read(self, reg):
        return self._i2c.readfrom_mem(self._addr, reg, 1)[0]

    def _write(self, reg, val):
        self._i2c.writeto_mem(self._addr, reg, bytes([val]))

    def _read_bytes(self, reg, n):
        return self._i2c.readfrom_mem(self._addr, reg, n)

    def _init(self):
        # Set BIG endian, auto-increment
        self._write(_REG_CTRL1, 0x60)
        # Accel: ±4G, 1kHz ODR
        self._write(_REG_CTRL2, ACCEL_RANGE_4G | ACCEL_ODR_1000)
        # Gyro: ±2000dps, 1kHz ODR
        self._write(_REG_CTRL3, 0x00 | GYRO_ODR_1000)
        # Enable accel + gyro
        self._write(_REG_CTRL7, 0x03)

    def read_accel(self):
        """Returns (ax, ay, az) in g."""
        buf = self._read_bytes(_REG_AX_L, 6)
        ax, ay, az = struct.unpack('<hhh', buf)
        return (ax / self._accel_scale,
                ay / self._accel_scale,
                az / self._accel_scale)

    def read_gyro(self):
        """Returns (gx, gy, gz) in dps."""
        buf = self._read_bytes(_REG_GX_L, 6)
        gx, gy, gz = struct.unpack('<hhh', buf)
        return (gx / self._gyro_scale,
                gy / self._gyro_scale,
                gz / self._gyro_scale)

    def pitch_roll(self):
        """Compute pitch and roll in degrees from accelerometer."""
        ax, ay, az = self.read_accel()
        pitch = math.atan2(-ax, math.sqrt(ay*ay + az*az)) * 180 / math.pi
        roll  = math.atan2(ay, az) * 180 / math.pi
        return pitch, roll

    def who_am_i(self):
        return self._read(_REG_WHO_AM_I)
