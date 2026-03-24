# main.py - WaveformPy entry point
# MicroPython firmware for Waveshare ESP32-S3 Touch AMOLED 1.8"
# Requires: lv_micropython (https://github.com/lvgl/lv_micropython)
#
# Flash with:
#   esptool.py --chip esp32s3 write_flash 0x0 lv_micropython_esp32s3.bin
# Then upload all .py files via mpremote or Thonny.

import gc
import machine
import time
import lvgl as lv

import config
import state
import prefs
from screen_manager import manager

# ──────────────────────────────────────────────────────────────────────────────
# Hardware init helpers
# ──────────────────────────────────────────────────────────────────────────────

def _init_i2c():
    return machine.SoftI2C(
        scl=machine.Pin(config.PIN_SCL),
        sda=machine.Pin(config.PIN_SDA),
        freq=400_000,
    )


def _init_display():
    from drivers.sh8601 import SH8601Display
    disp = SH8601Display()
    disp.init()
    return disp


def _init_touch(i2c):
    from drivers.ft3x68 import FT3x68
    touch = FT3x68(i2c, config.TOUCH_ADDR)
    indev = lv.indev_create()
    indev.set_type(lv.INDEV_TYPE.POINTER)
    indev.set_read_cb(touch.lvgl_read_cb)
    return touch


def _init_expander(i2c):
    from drivers.xca9554 import XCA9554
    exp = XCA9554(i2c, config.EXPANDER_ADDR)
    exp.pin_mode(config.EXP_PIN_SIDE_BTN, 1)  # input
    exp.pin_mode(config.EXP_PIN_PMU_IRQ,  1)  # input
    # Mirror the working Arduino bring-up sequence for board-side rails/resets.
    for pin in (0, 1, 2, 6, 7):
        exp.pin_mode(pin, 0)  # output
    for pin in (0, 1, 2, 6):
        exp.digital_write(pin, 0)
    time.sleep_ms(20)
    for pin in (0, 1, 2, 6, 7):
        exp.digital_write(pin, 1)
    time.sleep_ms(20)
    return exp


def _init_pmu(i2c):
    from drivers.axp2101 import AXP2101
    pmu = AXP2101(i2c, config.PMU_ADDR)
    pmu.set_brightness(config.BRIGHTNESS_ACTIVE)
    return pmu


def _init_rtc(i2c):
    from drivers.pcf85063 import PCF85063
    rtc = PCF85063(i2c, config.RTC_ADDR)
    if rtc.is_valid():
        dt = rtc.datetime()
        machine.RTC().datetime(dt)
        print("RTC time loaded:", dt)
    else:
        print("RTC: invalid time, waiting for NTP")
    return rtc


def _init_imu(i2c):
    from drivers.qmi8658 import QMI8658
    imu = QMI8658(i2c, config.IMU_ADDR)
    print("IMU WHO_AM_I:", hex(imu.who_am_i()))
    return imu


def _init_codec(i2c):
    from drivers.es8311 import ES8311
    codec = ES8311(i2c, config.CODEC_ADDR)
    codec.begin(16000)
    return codec


def _mount_sd():
    try:
        import uos
        sd = machine.SDCard(
            slot=1,
            clk=machine.Pin(config.PIN_SD_CLK),
            cmd=machine.Pin(config.PIN_SD_CMD),
            data0=machine.Pin(config.PIN_SD_DATA),
        )
        uos.mount(sd, "/sdcard")
        # Create standard directories
        for d in ("assets", "config", "cache", "recordings", "update"):
            try:
                uos.mkdir(f"/sdcard/{d}")
            except OSError:
                pass
        # Symlink cache to root for weather module
        try:
            uos.mkdir("/cache")
        except OSError:
            pass
        print("SD card mounted at /sdcard")
        return sd
    except Exception as e:
        print("SD card not found:", e)
        return None


def _setup_wifi_ntp(rtc):
    """Attempt WiFi + NTP sync (non-blocking: returns immediately)."""
    from modules import wifi_manager
    wifi_manager.update()
    if wifi_manager.is_connected():
        _ntp_sync(rtc)


def _ntp_sync(rtc):
    try:
        import ntptime
        ntptime.host = "pool.ntp.org"
        ntptime.settime()
        print("NTP sync OK:", time.localtime())
        if rtc:
            t = time.localtime()
            rtc.set_datetime((t[0], t[1], t[2], t[6], t[3], t[4], t[5]))
    except Exception as e:
        print("NTP sync failed:", e)


# ──────────────────────────────────────────────────────────────────────────────
# Tick callbacks for LVGL
# ──────────────────────────────────────────────────────────────────────────────

_lv_tick_timer = None

def _start_lv_tick():
    """Feed LVGL tick via a micropython timer."""
    global _lv_tick_timer
    _lv_tick_timer = machine.Timer(0)
    _lv_tick_timer.init(
        period=5,
        mode=machine.Timer.PERIODIC,
        callback=lambda t: lv.tick_inc(5),
    )


# ──────────────────────────────────────────────────────────────────────────────
# Button & idle handling
# ──────────────────────────────────────────────────────────────────────────────

_last_btn_ms = 0
_BTN_DEBOUNCE_MS = 180
_power_hold_start = 0
_POWER_HOLD_MS    = 4000

def _note_activity():
    state.last_activity_ms = time.ticks_ms()
    if state.dimmed and _pmu:
        _pmu.set_brightness(config.BRIGHTNESS_ACTIVE)
        state.dimmed = False

def _update_button(expander):
    global _last_btn_ms
    now = time.ticks_ms()
    if time.ticks_diff(now, _last_btn_ms) < _BTN_DEBOUNCE_MS:
        return
    if expander and not expander.digital_read(config.EXP_PIN_SIDE_BTN):
        _last_btn_ms = now
        _note_activity()
        manager.next_screen()


def _update_idle(pmu):
    now = time.ticks_ms()
    elapsed = time.ticks_diff(now, state.last_activity_ms)

    if not state.dimmed and elapsed > config.IDLE_DIM_MS:
        if pmu:
            pmu.set_brightness(config.BRIGHTNESS_DIM)
        state.dimmed = True
        print("Screen dimmed")

    if elapsed > config.IDLE_SLEEP_MS:
        print("Going to sleep...")
        if pmu:
            pmu.set_brightness(0)
        machine.deepsleep(0)  # Wake by touch interrupt


# ──────────────────────────────────────────────────────────────────────────────
# Periodic status update interval tracking
# ──────────────────────────────────────────────────────────────────────────────

_interval_trackers = {}

def _every(key, interval_ms):
    now = time.ticks_ms()
    if key not in _interval_trackers:
        _interval_trackers[key] = now - interval_ms
    if time.ticks_diff(now, _interval_trackers[key]) >= interval_ms:
        _interval_trackers[key] = now
        return True
    return False


# ──────────────────────────────────────────────────────────────────────────────
# Global hardware objects (set during setup)
# ──────────────────────────────────────────────────────────────────────────────
_pmu      = None
_rtc      = None
_imu      = None
_expander = None
_codec    = None


def setup():
    global _pmu, _rtc, _imu, _expander, _codec

    print("WaveformPy: setup start")
    machine.freq(240_000_000)  # Max CPU

    # LVGL init first
    lv.init()

    # I2C bus
    i2c = _init_i2c()

    # Peripherals
    try: _expander = _init_expander(i2c)
    except Exception as e: print("Expander init failed:", e)

    try: _pmu = _init_pmu(i2c)
    except Exception as e: print("PMU init failed:", e)

    try: _rtc = _init_rtc(i2c)
    except Exception as e: print("RTC init failed:", e)

    try: _imu = _init_imu(i2c)
    except Exception as e: print("IMU init failed:", e)

    try: _codec = _init_codec(i2c)
    except Exception as e: print("Codec init failed:", e)

    # Display
    try:
        _init_display()
    except Exception as e:
        print("Display init failed:", e)
        # Cannot continue without display
        raise

    # Touch (registers LVGL indev)
    try:
        _init_touch(i2c)
    except Exception as e:
        print("Touch init failed:", e)

    # LVGL tick
    _start_lv_tick()

    # SD
    _mount_sd()

    # Build screens
    _register_screens()
    manager.build_all()
    manager.restore_saved()

    # Initial state
    state.last_activity_ms = time.ticks_ms()

    # WiFi + NTP (non-blocking attempt)
    try:
        _setup_wifi_ntp(_rtc)
    except Exception as e:
        print("Initial WiFi/NTP attempt failed:", e)

    gc.collect()
    print("WaveformPy: setup complete, free mem:", gc.mem_free())


def _register_screens():
    from screens.watch   import WatchScreen
    from screens.weather     import WeatherScreen
    from screens.motion      import MotionScreen
    from screens.geo         import GeoScreen
    from screens.solar       import SolarScreen
    from screens.sky         import SkyScreen
    from screens.recorder    import RecorderScreen
    from screens.qr_code     import QRScreen
    from screens.calculator  import CalculatorScreen

    manager.register(WatchScreen())
    manager.register(WeatherScreen())
    manager.register(MotionScreen(_imu))
    manager.register(GeoScreen())
    manager.register(SolarScreen())
    manager.register(SkyScreen(_imu))
    manager.register(RecorderScreen(_codec))
    manager.register(QRScreen())
    manager.register(CalculatorScreen())


# ──────────────────────────────────────────────────────────────────────────────
# Main loop
# ──────────────────────────────────────────────────────────────────────────────

_RTC_WRITE_INTERVAL_MS = 60_000
_last_rtc_write_ms = 0

def loop():
    global _last_rtc_write_ms

    # WiFi management
    if _every("wifi", 1000):
        try:
            from modules import wifi_manager
            wifi_manager.update()
            state.wifi_ok = wifi_manager.is_connected()
        except Exception:
            pass

    # Data fetches (only when WiFi up)
    if state.wifi_ok:
        if _every("geo", 5000):
            try:
                from modules import geo_fetch
                geo_fetch.update()
            except Exception:
                pass

        if _every("weather", 5000):
            try:
                from modules import weather_fetch
                weather_fetch.update()
            except Exception:
                pass

    # PMU status
    if _every("pmu", 5000) and _pmu:
        try:
            state.battery_pct = _pmu.battery_percent()
            state.battery_mv  = _pmu.battery_voltage_mv()
            state.charging    = _pmu.is_charging()
            state.usb_ok      = _pmu.is_usb_present()
        except Exception:
            pass

    # RTC write-back
    now_ms = time.ticks_ms()
    if _rtc and time.ticks_diff(now_ms, _last_rtc_write_ms) > _RTC_WRITE_INTERVAL_MS:
        try:
            t = time.localtime()
            _rtc.set_datetime((t[0], t[1], t[2], t[6], t[3], t[4], t[5]))
            _last_rtc_write_ms = now_ms
        except Exception:
            pass

    # Button handling
    if _expander:
        try:
            _update_button(_expander)
        except Exception:
            pass

    # Idle / sleep management
    _update_idle(_pmu)

    # Screen tick
    if _every("screen", config.INTERVAL_MOTION):
        manager.tick_current(config.INTERVAL_MOTION)

    # LVGL task handler
    lv.timer_handler()
    time.sleep_ms(5)


# ──────────────────────────────────────────────────────────────────────────────
# Entry point
# ──────────────────────────────────────────────────────────────────────────────

setup()

while True:
    try:
        loop()
    except KeyboardInterrupt:
        print("Interrupted")
        break
    except Exception as e:
        import sys
        sys.print_exception(e)
        time.sleep_ms(500)
