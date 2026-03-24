# screen_manager.py - Modular screen lifecycle management
# Mirrors Waveform/src/screen_manager.cpp

import lvgl as lv
import prefs

# Screen IDs (order determines navigation order)
SCREEN_WATCH   = 0
SCREEN_WEATHER     = 1
SCREEN_MOTION      = 2
SCREEN_GEO         = 3
SCREEN_SOLAR       = 4
SCREEN_SKY         = 5
SCREEN_RECORDER    = 6
SCREEN_QR          = 7
SCREEN_CALCULATOR  = 8
SCREEN_COUNT       = 9

SCREEN_NAMES = [
    "Watch", 
    "Weather", 
    "Motion", 
    "Geo",
    "Solar", 
    "Sky", 
    "Recorder", 
    "QR", 
    "Calculator",
]


class ScreenModule:
    """Base class for all screen modules."""

    def __init__(self, screen_id, name):
        self.screen_id = screen_id
        self.name = name
        self._root = None
        self._built = False

    def build(self):
        """Create LVGL objects. Returns True on success."""
        return False

    def refresh(self):
        """Update data and UI. Called on each module tick."""
        pass

    def enter(self):
        """Called when this screen becomes active."""
        pass

    def leave(self):
        """Called when navigating away from this screen."""
        pass

    def tick(self, elapsed_ms):
        """Periodic callback with milliseconds since last tick."""
        pass

    def root(self):
        """Return the root LVGL object."""
        return self._root


class ScreenManager:
    def __init__(self):
        self._modules = [None] * SCREEN_COUNT
        self._current_id = SCREEN_WATCH
        self._error_screen = None

    def register(self, module):
        self._modules[module.screen_id] = module

    def build_all(self):
        for m in self._modules:
            if m is not None:
                try:
                    ok = m.build()
                    if ok:
                        m._built = True
                    else:
                        self._show_error(m, "build() returned False")
                except Exception as e:
                    self._show_error(m, str(e))

    def _show_error(self, module, reason):
        print(f"[ScreenManager] Error in {module.name}: {reason}")
        module._built = False

    def go_to(self, screen_id, save=True):
        if screen_id < 0 or screen_id >= SCREEN_COUNT:
            return
        module = self._modules[screen_id]
        if module is None or not module._built:
            return

        # Leave current
        cur = self._modules[self._current_id]
        if cur and cur._built:
            try:
                cur.leave()
            except Exception as e:
                print("leave error:", e)

        self._current_id = screen_id
        if save:
            prefs.put_int("last_screen", screen_id)

        # Enter new
        try:
            module.enter()
        except Exception as e:
            print("enter error:", e)

        if module.root() is not None:
            lv.scr_load(module.root())

    def next_screen(self):
        nxt = (self._current_id + 1) % SCREEN_COUNT
        # Skip screens that failed to build
        for _ in range(SCREEN_COUNT):
            m = self._modules[nxt]
            if m and m._built:
                self.go_to(nxt)
                return
            nxt = (nxt + 1) % SCREEN_COUNT

    def prev_screen(self):
        prv = (self._current_id - 1) % SCREEN_COUNT
        for _ in range(SCREEN_COUNT):
            m = self._modules[prv]
            if m and m._built:
                self.go_to(prv)
                return
            prv = (prv - 1) % SCREEN_COUNT

    def current(self):
        return self._modules[self._current_id]

    def current_id(self):
        return self._current_id

    def tick_current(self, elapsed_ms):
        m = self.current()
        if m and m._built:
            try:
                m.tick(elapsed_ms)
            except Exception as e:
                print(f"tick error [{m.name}]:", e)

    def restore_saved(self):
        saved = prefs.get_int("last_screen", SCREEN_WATCH)
        if 0 <= saved < SCREEN_COUNT:
            self.go_to(saved, save=False)
        else:
            self.go_to(SCREEN_WATCH, save=False)


# Global singleton
manager = ScreenManager()
