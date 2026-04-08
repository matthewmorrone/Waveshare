#!/usr/bin/env python3

import argparse
import pathlib
import sys
import time

import serial


def read_reply(port: serial.Serial, timeout_s: float) -> str:
    deadline = time.monotonic() + timeout_s
    line = bytearray()
    while time.monotonic() < deadline:
      chunk = port.read(1)
      if not chunk:
        continue
      if chunk == b"\n":
        return line.decode("utf-8", errors="replace").strip()
      if chunk != b"\r":
        line.extend(chunk)
    raise TimeoutError("Timed out waiting for device reply")


def main() -> int:
    parser = argparse.ArgumentParser(description="Copy a local file to Locket SD over USB serial.")
    parser.add_argument("--port", required=True, help="Serial port, e.g. /dev/cu.usbmodem101")
    parser.add_argument("--source", required=True, help="Local source file")
    parser.add_argument("--dest", required=True, help="Destination path on SD, e.g. /locket/audio/file.wav")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    source = pathlib.Path(args.source)
    if not source.is_file():
        print(f"Source file not found: {source}", file=sys.stderr)
        return 1

    size = source.stat().st_size
    with serial.Serial(args.port, args.baud, timeout=0.25, write_timeout=5) as port:
        time.sleep(1.0)
        port.reset_input_buffer()
        port.reset_output_buffer()

        port.write(f"PUT {args.dest} {size}\n".encode("utf-8"))
        port.flush()

        reply = read_reply(port, 5.0)
        if not reply.startswith("READY "):
            print(f"Unexpected device reply: {reply}", file=sys.stderr)
            return 2

        sent = 0
        with source.open("rb") as handle:
            while True:
                chunk = handle.read(1024)
                if not chunk:
                    break
                port.write(chunk)
                sent += len(chunk)
            port.flush()

        reply = read_reply(port, 30.0)
        if not reply.startswith("OK "):
            print(f"Transfer failed: {reply}", file=sys.stderr)
            return 3

        print(f"Uploaded {sent} bytes to {args.dest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
