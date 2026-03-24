# boot.py - Runs before main.py on MicroPython startup
import gc
import sys

# Free up as much memory as possible before main
gc.collect()

# Print startup info
import machine
print("WaveformPy starting...")
print("Freq:", machine.freq() // 1_000_000, "MHz")
print("Free mem:", gc.mem_free())
