import time

import lvgl as lv
import machine

import config
from drivers.axp2101 import AXP2101
from drivers.sh8601 import SH8601Display
from drivers.xca9554 import XCA9554


def _init_i2c():
    return machine.SoftI2C(
        scl=machine.Pin(config.PIN_SCL),
        sda=machine.Pin(config.PIN_SDA),
        freq=400_000,
    )


def _init_expander(i2c):
    exp = XCA9554(i2c, config.EXPANDER_ADDR)
    exp.pin_mode(config.EXP_PIN_SIDE_BTN, 1)
    exp.pin_mode(config.EXP_PIN_PMU_IRQ, 1)
    for pin in (0, 1, 2, 6, 7):
        exp.pin_mode(pin, 0)
    for pin in (0, 1, 2, 6):
        exp.digital_write(pin, 0)
    time.sleep_ms(20)
    for pin in (0, 1, 2, 6, 7):
        exp.digital_write(pin, 1)
    time.sleep_ms(20)


def main():
    print("SMOKE: start")
    i2c = _init_i2c()
    _init_expander(i2c)
    print("SMOKE: expander ok")

    pmu = AXP2101(i2c, config.PMU_ADDR)
    pmu.set_brightness(255)
    print("SMOKE: pmu ok")

    lv.init()
    print("SMOKE: lvgl ok")

    disp = SH8601Display()
    disp.init()
    print("SMOKE: display init ok")

    scr = lv.obj(None)
    scr.set_style_bg_color(lv.color_make(255, 0, 0), 0)
    scr.set_style_bg_opa(lv.OPA.COVER, 0)
    lv.scr_load(scr)
    print("SMOKE: screen loaded")

    for _ in range(100):
        lv.timer_handler()
        time.sleep_ms(20)

    print("SMOKE: done")


main()
