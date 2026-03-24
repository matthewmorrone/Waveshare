#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#include "config/pin_config.h"

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS,
    LCD_SCLK,
    LCD_SDIO0,
    LCD_SDIO1,
    LCD_SDIO2,
    LCD_SDIO3);

Arduino_SH8601 *gfx = new Arduino_SH8601(
    bus,
    GFX_NOT_DEFINED,
    0,
    LCD_WIDTH,
    LCD_HEIGHT);

void setup()
{
  Serial.begin(115200);
  delay(250);

  if (!gfx->begin()) {
    Serial.println("panel_diag: display init failed");
    while (true) {
      delay(1000);
    }
  }

  gfx->setBrightness(255);
  gfx->displayOn();
  gfx->fillScreen(0xFFFF);
  Serial.println("panel_diag: white screen");
}

void loop()
{
  delay(100);
}
