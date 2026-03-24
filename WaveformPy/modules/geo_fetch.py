# modules/geo_fetch.py - IP-based geolocation
import urequests
import time

_URL = "http://ip-api.com/json/?fields=status,country,countryCode,region,regionName,city,zip,lat,lon,timezone,isp,query"
_TIMEOUT = 2500  # ms

_state = {
    "lat": 0.0,
    "lon": 0.0,
    "city": "",
    "region": "",
    "country": "",
    "country_code": "",
    "timezone": "",
    "zip": "",
    "isp": "",
    "ip": "",
    "last_fetch_ms": -(15 * 60 * 1000),
    "fetching": False,
    "ok": False,
}

_FETCH_INTERVAL_MS = 15 * 60 * 1000
_RETRY_INTERVAL_MS = 60 * 1000
_last_fail_ms = -_RETRY_INTERVAL_MS


def get_state():
    return _state


def update():
    """Call from main loop. Fetches if interval elapsed and WiFi available."""
    global _last_fail_ms
    from modules import wifi_manager
    if not wifi_manager.is_connected():
        return

    now = time.ticks_ms()
    if _state["ok"]:
        interval = _FETCH_INTERVAL_MS
    else:
        interval = _RETRY_INTERVAL_MS

    if time.ticks_diff(now, _state["last_fetch_ms"]) < interval:
        return

    _state["last_fetch_ms"] = now
    _fetch()


def _fetch():
    try:
        r = urequests.get(_URL, timeout=2.5)
        if r.status_code == 200:
            j = r.json()
            if j.get("status") == "success":
                _state.update({
                    "lat": j.get("lat", 0.0),
                    "lon": j.get("lon", 0.0),
                    "city": j.get("city", ""),
                    "region": j.get("regionName", ""),
                    "country": j.get("country", ""),
                    "country_code": j.get("countryCode", ""),
                    "timezone": j.get("timezone", ""),
                    "zip": j.get("zip", ""),
                    "isp": j.get("isp", ""),
                    "ip": j.get("query", ""),
                    "ok": True,
                })
                print("Geo: ok,", _state["city"], _state["country"])
            else:
                _state["ok"] = False
        r.close()
    except Exception as e:
        print("Geo fetch error:", e)
        _state["ok"] = False
