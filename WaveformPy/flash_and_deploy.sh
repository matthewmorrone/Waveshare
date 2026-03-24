#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

PORT="${1:-}"
PYDEPS_DIR="${ROOT_DIR}/.pydeps"
PIP_CACHE_DIR="${ROOT_DIR}/.pip-cache"
BUILD_DIR="${ROOT_DIR}/lv_micropython/ports/esp32/build-WAVESHARE_ESP32_S3_TOUCH_AMOLED_18"

BOOTLOADER_BIN="${BUILD_DIR}/bootloader/bootloader.bin"
PARTITION_BIN="${BUILD_DIR}/partition_table/partition-table.bin"
FIRMWARE_BIN="${BUILD_DIR}/micropython.bin"

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

wait_for_port() {
    local tries="${1:-30}"
    local delay="${2:-1}"
    local i

    for ((i = 0; i < tries; i++)); do
        if find_port >/dev/null; then
            return 0
        fi
        sleep "${delay}"
    done

    return 1
}

if [[ ! -f "${FIRMWARE_BIN}" ]]; then
    echo "Firmware not found: ${FIRMWARE_BIN}" >&2
    exit 1
fi

if ! wait_for_port 10 1; then
    echo "No serial port found. Connect or power-cycle the board and try again." >&2
    exit 1
fi

mpremote_retry() {
    local tries="${1}"
    shift
    local i

    for ((i = 0; i < tries; i++)); do
        if wait_for_port 1 1 && PYTHONPATH="${PYDEPS_DIR}" python3 -m mpremote connect "${PORT}" "$@"; then
            return 0
        fi
        sleep 1
    done

    return 1
}

mkdir -p "${PYDEPS_DIR}" "${PIP_CACHE_DIR}"

if ! PYTHONPATH="${PYDEPS_DIR}" python3 -c 'import esptool, mpremote' >/dev/null 2>&1; then
    PIP_CACHE_DIR="${PIP_CACHE_DIR}" python3 -m pip install \
        --target "${PYDEPS_DIR}" \
        esptool \
        mpremote
fi

PORT="$(find_port)"
echo "Flashing MicroPython firmware to ${PORT}..."
PYTHONPATH="${PYDEPS_DIR}" python3 -m esptool \
    --chip esp32s3 \
    --port "${PORT}" \
    --baud 460800 \
    write-flash -z \
    --flash-mode dio \
    --flash-freq 80m \
    --flash-size 16MB \
    0x0 "${BOOTLOADER_BIN}" \
    0x8000 "${PARTITION_BIN}" \
    0x10000 "${FIRMWARE_BIN}"

echo "Waiting for USB serial to return..."
wait_for_port 30 1

echo "Deploying WaveformPy files to ${PORT}..."
cd "${SCRIPT_DIR}"

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
    screens/watch.py
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

for dir in drivers screens modules cache recordings; do
    mpremote_retry 10 fs mkdir "${dir}" 2>/dev/null || true
done

for f in "${FILES[@]}"; do
    echo "  Uploading ${f}..."
    mpremote_retry 10 fs cp "${f}" ":${f}"
done

echo "Resetting device..."
mpremote_retry 10 reset

echo "Done."
