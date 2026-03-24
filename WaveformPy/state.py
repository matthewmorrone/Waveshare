# state.py - Shared mutable hardware state
# Modules read from here; main.py writes to it each loop.

battery_pct = 0
battery_mv  = 0
charging    = False
usb_ok      = False
wifi_ok     = False

# IMU last read
ax = ay = az = 0.0
gx = gy = gz = 0.0
pitch = roll = 0.0

# Idle tracking
last_activity_ms = 0   # set by main.py
dimmed = False
sleeping = False
