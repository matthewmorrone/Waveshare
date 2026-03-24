# prefs.py - Persistent key/value storage
# Uses a JSON file on the filesystem as NVS replacement

import json
import os

_PREFS_FILE = "/prefs.json"
_cache = {}

def _load():
    global _cache
    try:
        with open(_PREFS_FILE, "r") as f:
            _cache = json.load(f)
    except Exception:
        _cache = {}

def _save():
    try:
        with open(_PREFS_FILE, "w") as f:
            json.dump(_cache, f)
    except Exception as e:
        print("prefs save error:", e)

def get_int(key, default=0):
    _load()
    return int(_cache.get(key, default))

def get_str(key, default=""):
    _load()
    return str(_cache.get(key, default))

def put_int(key, value):
    _load()
    _cache[key] = int(value)
    _save()

def put_str(key, value):
    _load()
    _cache[key] = str(value)
    _save()

def remove(key):
    _load()
    _cache.pop(key, None)
    _save()
