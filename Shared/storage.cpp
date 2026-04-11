#include <Adafruit_XCA9554.h>
#include <SD_MMC.h>

#include "pin_config.h"
#include "screen_constants.h"
#include "storage.h"

extern Adafruit_XCA9554 expander;
extern bool sdMounted;
extern bool expanderReady;

namespace
{
uint8_t gStorageCardType = CARD_NONE;
uint64_t gStorageCardSizeMb = 0;
}

const char *sdCardTypeLabel(uint8_t cardType)
{
  switch (cardType) {
    case CARD_MMC:
      return "MMC";
    case CARD_SD:
      return "SDSC";
    case CARD_SDHC:
      return "SDHC";
    default:
      return "UNKNOWN";
  }
}

bool storageIsMounted()
{
  return sdMounted;
}

uint8_t storageCardType()
{
  return gStorageCardType;
}

uint64_t storageCardSizeMb()
{
  return gStorageCardSizeMb;
}

void ensureSdDirectories()
{
  for (const char *dir : kSdStartupDirs) {
    if (!SD_MMC.exists(dir) && !SD_MMC.mkdir(dir)) {
      Serial.printf("Failed to create SD directory: %s\n", dir);
    }
  }
}

void initSdCard()
{
  if (sdMounted) {
#ifdef SCREEN_RECORDER
    refreshRecorderClipState();
#endif
    return;
  }

  sdMounted = false;
  gStorageCardType = CARD_NONE;
  gStorageCardSizeMb = 0;

  if (expanderReady) {
    expander.digitalWrite(7, HIGH);
    delay(20);
  }

  for (int attempt = 1; attempt <= 3; ++attempt) {
    SD_MMC.end();
    delay(20);
    SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
    if (!SD_MMC.begin(kSdMountPath, true)) {
      Serial.printf("SD mount attempt %d failed\n", attempt);
      delay(80);
      continue;
    }

    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
      Serial.printf("SD cardType attempt %d returned CARD_NONE\n", attempt);
      SD_MMC.end();
      delay(80);
      continue;
    }

    sdMounted = true;
    gStorageCardType = cardType;
    gStorageCardSizeMb = SD_MMC.cardSize() / (1024 * 1024);
    ensureSdDirectories();

    Serial.printf("SD mounted: %s, %llu MB\n", sdCardTypeLabel(gStorageCardType), gStorageCardSizeMb);
    return;
  }

  Serial.println("SD card not mounted after retries");
}
