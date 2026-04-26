#include "usb_msc.h"

#include <Arduino.h>
#include <SD_MMC.h>

#if defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE == 0
#include "USB.h"
#include "USBMSC.h"

namespace
{

USBMSC gMsc;
bool gMscStarted = false;

// Host read request. Each transfer is an integer number of 512-byte
// sectors starting at `lba`; SD_MMC's raw API works on whole sectors
// so we walk the range one sector at a time.
int32_t mscOnRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
  (void)offset;
  const uint32_t sectorSize = 512;
  const uint32_t sectors = bufsize / sectorSize;
  uint8_t *buf = static_cast<uint8_t *>(buffer);
  for (uint32_t i = 0; i < sectors; ++i) {
    if (!SD_MMC.readRAW(buf + i * sectorSize, lba + i)) {
      return -1;
    }
  }
  return static_cast<int32_t>(bufsize);
}

// Host write request. Mirrors the read path over writeRAW. The Arduino
// FS caches may go stale after the host writes to FAT structures; the
// user should refresh by re-opening files or rebooting after bulk copy.
int32_t mscOnWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
  (void)offset;
  const uint32_t sectorSize = 512;
  const uint32_t sectors = bufsize / sectorSize;
  for (uint32_t i = 0; i < sectors; ++i) {
    if (!SD_MMC.writeRAW(buffer + i * sectorSize, lba + i)) {
      return -1;
    }
  }
  return static_cast<int32_t>(bufsize);
}

// SCSI START_STOP_UNIT — the host uses this to eject the disk. We ACK
// and otherwise do nothing; the host has already flushed its caches.
bool mscOnStartStop(uint8_t power_condition, bool start, bool load_eject)
{
  (void)power_condition;
  (void)start;
  (void)load_eject;
  return true;
}

}  // namespace

bool initUsbMsc()
{
  if (gMscStarted) return true;
  if (SD_MMC.cardType() == CARD_NONE) return false;

  const uint32_t sectorCount = static_cast<uint32_t>(SD_MMC.numSectors());
  const uint16_t sectorSize = static_cast<uint16_t>(SD_MMC.sectorSize());
  if (sectorCount == 0 || sectorSize == 0) return false;

  gMsc.vendorID("Locket");
  gMsc.productID("SD Card");
  gMsc.productRevision("1.0");
  gMsc.onRead(mscOnRead);
  gMsc.onWrite(mscOnWrite);
  gMsc.onStartStop(mscOnStartStop);
  gMsc.mediaPresent(true);
  gMsc.isWritable(true);
  gMsc.begin(sectorCount, sectorSize);
  USB.begin();
  gMscStarted = true;
  return true;
}

void setMscMediaPresent(bool present)
{
  if (!gMscStarted) return;
  gMsc.mediaPresent(present);
}

#else  // ARDUINO_USB_MODE != 0 — MSC unavailable in HWCDC mode

bool initUsbMsc() { return false; }
void setMscMediaPresent(bool) {}

#endif
