# modules/weather_fetch.py - Weather data fetching and caching
# Uses Open-Meteo (free, no API key required) mirroring the C++ weather module
import urequests
import ujson
import time
import math

_FETCH_INTERVAL_MS  = 15 * 60 * 1000
_RETRY_INTERVAL_MS  = 60 * 1000
_CACHE_FILE         = "/cache/weather.json"

# Open-Meteo API (free, no key needed)
_URL_TEMPLATE = (
    "https://api.open-meteo.com/v1/forecast"
    "?latitude={lat}&longitude={lon}"
    "&current=temperature_2m,weathercode,windspeed_10m,relative_humidity_2m"
    "&hourly=temperature_2m,weathercode"
    "&daily=temperature_2m_max,temperature_2m_min,weathercode,sunrise,sunset"
    "&temperature_unit=fahrenheit"
    "&wind_speed_unit=mph"
    "&forecast_days=7"
    "&timezone=auto"
)

# WMO weather code to condition name
def _wmo_to_condition(code):
    if code == 0:              return "Clear"
    if code in (1, 2):         return "Partly Cloudy"
    if code == 3:              return "Cloudy"
    if code in (51,53,55,61,63,65,80,81,82): return "Rain"
    if code in (71,73,75,77,85,86): return "Snow"
    if code in (95,96,99):    return "Storm"
    if code in (45,48):        return "Fog"
    return "Cloudy"

_MOON_EPOCH_JD  = 2451550.1    # Jan 6 2000 known new moon (Julian Day)
_SYNODIC_MONTH  = 29.53058867

def _moon_phase(year, month, day):
    """Returns moon phase 0.0-1.0 (0=new, 0.5=full)."""
    # Approximate Julian Day
    a = (14 - month) // 12
    y = year + 4800 - a
    m = month + 12 * a - 3
    jd = day + (153*m+2)//5 + 365*y + y//4 - y//100 + y//400 - 32045
    return ((jd - _MOON_EPOCH_JD) % _SYNODIC_MONTH) / _SYNODIC_MONTH


_state = {
    "ok": False,
    "last_fetch_ms": -_FETCH_INTERVAL_MS,
    "condition": "Clear",
    "temp_f": 0.0,
    "humidity": 0,
    "wind_mph": 0.0,
    "hourly_temps": [],
    "hourly_codes": [],
    "daily_max": [],
    "daily_min": [],
    "daily_codes": [],
    "sunrise": "",
    "sunset": "",
    "moon_phase": 0.0,
}


def get_state():
    return _state


def update():
    """Call from main loop."""
    from modules import wifi_manager, geo_fetch
    if not wifi_manager.is_connected():
        return

    now = time.ticks_ms()
    interval = _RETRY_INTERVAL_MS if not _state["ok"] else _FETCH_INTERVAL_MS
    if time.ticks_diff(now, _state["last_fetch_ms"]) < interval:
        return

    _state["last_fetch_ms"] = now
    geo = geo_fetch.get_state()
    if not geo["ok"]:
        return
    _fetch(geo["lat"], geo["lon"])


def _fetch(lat, lon):
    url = _URL_TEMPLATE.format(lat=lat, lon=lon)
    try:
        r = urequests.get(url, timeout=5)
        if r.status_code == 200:
            j = r.json()
            _parse(j)
            _cache_save(j)
        r.close()
    except Exception as e:
        print("Weather fetch error:", e)
        _cache_load()


def _parse(j):
    cur = j.get("current", {})
    daily = j.get("daily", {})
    hourly = j.get("hourly", {})

    import time as t
    now = t.localtime()
    _state.update({
        "ok": True,
        "condition": _wmo_to_condition(cur.get("weathercode", 0)),
        "temp_f": cur.get("temperature_2m", 0.0),
        "humidity": cur.get("relative_humidity_2m", 0),
        "wind_mph": cur.get("windspeed_10m", 0.0),
        "hourly_temps": (hourly.get("temperature_2m") or [])[:24],
        "hourly_codes": (hourly.get("weathercode") or [])[:24],
        "daily_max":   daily.get("temperature_2m_max") or [],
        "daily_min":   daily.get("temperature_2m_min") or [],
        "daily_codes": daily.get("weathercode") or [],
        "sunrise":     (daily.get("sunrise") or [""])[0][11:16],
        "sunset":      (daily.get("sunset") or [""])[0][11:16],
        "moon_phase":  _moon_phase(now[0], now[1], now[2]),
    })
    print("Weather: ok,", _state["condition"], _state["temp_f"], "°F")


def _cache_save(j):
    try:
        with open(_CACHE_FILE, "w") as f:
            ujson.dump(j, f)
    except Exception as e:
        print("Weather cache save error:", e)


def _cache_load():
    try:
        with open(_CACHE_FILE, "r") as f:
            j = ujson.load(f)
        _parse(j)
        print("Weather: loaded from cache")
    except Exception:
        pass
