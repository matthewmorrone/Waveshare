#!/usr/bin/env bash
# deploy.sh - Upload WaveformPy to device via mpremote
# Usage: ./deploy.sh [PORT]
# Default port: /dev/tty.usbmodem* (macOS) or /dev/ttyUSB0 (Linux)

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

PORT="${1:-}"

if ! command -v mpremote &>/dev/null; then
    echo "Install mpremote: pip install mpremote"
    exit 1
fi

echo "Deploying WaveformPy to $PORT..."

find_port() {
    local candidate=""

    if [[ -n "${PORT}" ]]; then
        local base="${PORT#/dev/}"
        case "${base}" in
            tty.usbmodem*)
                candidate="/dev/cu.${base#tty.}"
                if [[ -e "${candidate}" ]]; then
                    PORT="${candidate}"
                    printf '%s\n' "${PORT}"
                    return 0
                fi
                ;;
        esac

        if [[ -e "${PORT}" ]]; then
            printf '%s\n' "${PORT}"
            return 0
        fi
    fi

    candidate="$(ls /dev/cu.usbmodem* 2>/dev/null | head -1 || true)"
    if [[ -z "${candidate}" ]]; then
        candidate="$(ls /dev/tty.usbmodem* 2>/dev/null | head -1 || true)"
    fi
    if [[ -z "${candidate}" ]]; then
        candidate="$(ls /dev/cu.usbserial* 2>/dev/null | head -1 || true)"
    fi
    if [[ -z "${candidate}" ]]; then
        candidate="$(ls /dev/tty.usbserial* 2>/dev/null | head -1 || true)"
    fi

    if [[ -n "${candidate}" ]]; then
        PORT="${candidate}"
        printf '%s\n' "${PORT}"
        return 0
    fi

    return 1
}

mpremote_retry() {
    local tries="${1}"
    shift
    local i

    for ((i = 0; i < tries; i++)); do
        if find_port >/dev/null && mpremote connect "$PORT" "$@"; then
            return 0
        fi
        sleep 1
    done

    return 1
}

FILES=(
    boot.py
    main.py
    config.py
    prefs.py
    state.py
    screen_manager.py
    ota_config.py
    drivers/__init__.py
    drivers/sh8601.py
    drivers/ft3x68.py
    drivers/xca9554.py
    drivers/axp2101.py
    drivers/pcf85063.py
    drivers/qmi8658.py
    drivers/es8311.py
    screens/__init__.py
    screens/watchface.py
    screens/weather.py
    screens/motion.py
    screens/geo.py
    screens/solar.py
    screens/sky.py
    screens/recorder.py
    screens/qr_code.py
    screens/calculator.py
    modules/__init__.py
    modules/wifi_manager.py
    modules/weather_fetch.py
    modules/geo_fetch.py
)

# Create remote directories
mpremote_retry 10 fs mkdir drivers  2>/dev/null || true
mpremote_retry 10 fs mkdir screens  2>/dev/null || true
mpremote_retry 10 fs mkdir modules  2>/dev/null || true
mpremote_retry 10 fs mkdir cache    2>/dev/null || true
mpremote_retry 10 fs mkdir recordings 2>/dev/null || true

# Upload all files
for f in "${FILES[@]}"; do
    echo "  Uploading $f..."
    mpremote_retry 10 fs cp "$f" ":$f"
done

echo "Done. Resetting device..."
mpremote_retry 10 reset
