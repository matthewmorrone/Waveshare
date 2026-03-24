#include <Adafruit_XCA9554.h>
#include <SD_MMC.h>

#include "config/pin_config.h"
#include "config/screen_constants.h"
#include "modules/storage.h"

extern Adafruit_XCA9554 expander;
extern bool sdMounted;
extern uint8_t sdCardType;
extern uint64_t sdCardSizeMb;
extern bool expanderReady;
void refreshRecorderClipState();

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

void ensureSdDirectories()
{
  for (const char *dir : kSdStartupDirs) {
    String fullPath = String(kSdMountPath) + dir;
    if (!SD_MMC.exists(fullPath.c_str()) && !SD_MMC.mkdir(fullPath.c_str())) {
      Serial.printf("Failed to create SD directory: %s\n", fullPath.c_str());
    }
  }
}

void initSdCard()
{
  if (sdMounted) {
    refreshRecorderClipState();
    return;
  }

  sdMounted = false;
  sdCardType = CARD_NONE;
  sdCardSizeMb = 0;

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
    sdCardType = cardType;
    sdCardSizeMb = SD_MMC.cardSize() / (1024 * 1024);
    ensureSdDirectories();
    refreshRecorderClipState();

    Serial.printf("SD mounted: %s, %llu MB\n", sdCardTypeLabel(sdCardType), sdCardSizeMb);
    return;
  }

  Serial.println("SD card not mounted after retries");
}
