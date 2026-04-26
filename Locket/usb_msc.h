#pragma once

// USB Mass Storage (MSC) over TinyUSB. Exposes the SD card as a disk to
// the host while the device is plugged in, running alongside the CDC
// serial interface as a composite device. Raw sectors are served from
// SD_MMC.readRAW / writeRAW so the Arduino FS can stay mounted for the
// app's own reads. Requires ARDUINO_USB_MODE=0 (set in platformio.ini).

// Register the MSC interface and start the USB stack. No-op if no SD
// card is mounted — the device is expected to reboot after a card is
// inserted, because TinyUSB descriptors are fixed at enumeration time.
bool initUsbMsc();

// Tell the host whether the card is currently readable. Call on
// detected insert / remove events.
void setMscMediaPresent(bool present);
