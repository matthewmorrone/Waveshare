# modules/wifi_manager.py - WiFi connection management
import network
import time

# Edit these or provide via ota_config.py
try:
    from ota_config import WIFI_SSID, WIFI_PASS, WIFI_SSID2, WIFI_PASS2
except ImportError:
    WIFI_SSID  = "The Y-Files"
    WIFI_PASS  = "quartz21wrench10crown"
    WIFI_SSID2 = "Monoceros"
    WIFI_PASS2 = "12345678"

_CONNECT_TIMEOUT_MS = 12_000
_RETRY_INTERVAL_MS  = 30_000

_wlan = network.WLAN(network.STA_IF)
_last_attempt_ms = -_RETRY_INTERVAL_MS
_connecting = False


def is_connected():
    return _wlan.isconnected()


def status():
    return _wlan.ifconfig() if _wlan.isconnected() else None


def ip():
    if _wlan.isconnected():
        return _wlan.ifconfig()[0]
    return None


def _try_connect(ssid, password, timeout_ms):
    if not ssid:
        return False
    print(f"WiFi: connecting to {ssid}...")
    _wlan.active(True)
    _wlan.connect(ssid, password)
    t0 = time.ticks_ms()
    while not _wlan.isconnected():
        if time.ticks_diff(time.ticks_ms(), t0) > timeout_ms:
            return False
        time.sleep_ms(100)
    print("WiFi: connected, IP =", _wlan.ifconfig()[0])
    return True


def update():
    """Call from main loop. Handles reconnection with retry interval."""
    global _last_attempt_ms, _connecting

    if _wlan.isconnected():
        return True

    now = time.ticks_ms()
    if time.ticks_diff(now, _last_attempt_ms) < _RETRY_INTERVAL_MS:
        return False

    _last_attempt_ms = now
    if _try_connect(WIFI_SSID, WIFI_PASS, _CONNECT_TIMEOUT_MS):
        return True
    if _try_connect(WIFI_SSID2, WIFI_PASS2, _CONNECT_TIMEOUT_MS):
        return True

    print("WiFi: all credentials failed")
    return False


def disconnect():
    _wlan.disconnect()
    _wlan.active(False)
