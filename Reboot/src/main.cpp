#include <Adafruit_XCA9554.h>
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <BLEDevice.h>
#include <FS.h>
#include <Network.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <Wire.h>

#include "pin_config.h"

namespace
{
constexpr uint8_t kExpanderAddress = 0x20;
constexpr uint8_t kPmuAddress = 0x34;
constexpr uint8_t kTouchAddress = 0x38;
constexpr uint8_t kRtcAddress = 0x51;
constexpr uint8_t kImuAddress = 0x6B;
constexpr uint8_t kAudioCodecAddress = 0x18;

constexpr uint16_t kColorBackground = 0x0000;
constexpr uint16_t kColorTitle = 0xFFFF;
constexpr uint16_t kColorOk = 0x07E0;
constexpr uint16_t kColorBad = 0xF800;
constexpr uint16_t kColorMuted = 0xAD55;

struct StatusRow
{
  const char *label = "";
  bool ok = false;
  String detail;
};

constexpr size_t kMaxRows = 12;
StatusRow gRows[kMaxRows];
size_t gRowCount = 0;

Adafruit_XCA9554 gExpander;
bool gExpanderReady = false;

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

void addStatus(const char *label, bool ok, const String &detail)
{
  if (gRowCount >= kMaxRows) {
    return;
  }

  gRows[gRowCount].label = label;
  gRows[gRowCount].ok = ok;
  gRows[gRowCount].detail = detail;
  ++gRowCount;
}

bool probeI2c(uint8_t address)
{
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool initExpanderAndRails()
{
  bool ready = false;
  for (int attempt = 0; attempt < 3 && !ready; ++attempt) {
    if (attempt > 0) {
      Wire.end();
      delay(10);
      Wire.begin(IIC_SDA, IIC_SCL);
      Wire.setClock(400000);
      delay(10);
    }
    ready = gExpander.begin(kExpanderAddress);
  }

  if (!ready) {
    return false;
  }

  gExpander.pinMode(PMU_IRQ_PIN, INPUT);
  gExpander.pinMode(TOP_BUTTON_PIN, INPUT);
  gExpander.pinMode(0, OUTPUT);
  gExpander.pinMode(1, OUTPUT);
  gExpander.pinMode(2, OUTPUT);
  gExpander.pinMode(6, OUTPUT);
  gExpander.pinMode(7, OUTPUT);

  gExpander.digitalWrite(0, LOW);
  gExpander.digitalWrite(1, LOW);
  gExpander.digitalWrite(2, LOW);
  gExpander.digitalWrite(6, LOW);
  delay(20);
  gExpander.digitalWrite(0, HIGH);
  gExpander.digitalWrite(1, HIGH);
  gExpander.digitalWrite(2, HIGH);
  gExpander.digitalWrite(6, HIGH);
  gExpander.digitalWrite(7, HIGH);
  delay(20);

  return true;
}

String cardTypeText(uint8_t cardType)
{
  switch (cardType) {
    case CARD_MMC:
      return "MMC";
    case CARD_SD:
      return "SDSC";
    case CARD_SDHC:
      return "SDHC";
    default:
      return "unknown";
  }
}

bool initSdCard(String &detail)
{
  SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);

  if (!SD_MMC.begin("/sdcard", true)) {
    detail = "mount failed";
    return false;
  }

  const uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    detail = "no card";
    return false;
  }

  detail = cardTypeText(cardType);
  const uint64_t sizeMb = SD_MMC.cardSize() / (1024ULL * 1024ULL);
  if (sizeMb > 0) {
    detail += " ";
    detail += String(static_cast<unsigned long>(sizeMb));
    detail += " MB";
  }

  return true;
}

bool initWifi(String &detail)
{
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  delay(20);
  WiFi.mode(WIFI_STA);
  delay(20);

  if (WiFi.getMode() != WIFI_STA) {
    detail = "STA mode failed";
    return false;
  }

  detail = "radio ready";
  return true;
}

bool initBle(String &detail)
{
  if (!BLEDevice::getInitialized() && !BLEDevice::init("waveform-hardware-check")) {
    detail = "init failed";
    return false;
  }

  detail = "radio ready";
  return true;
}

void drawStatusScreen()
{
  gfx->fillScreen(kColorBackground);
  gfx->setTextWrap(false);

  int16_t y = 18;

  gfx->setTextColor(kColorTitle);
  gfx->setTextSize(2);
  gfx->setCursor(12, y);
  gfx->println("Reboot");

  y += 24;
  gfx->setTextColor(kColorMuted);
  gfx->setTextSize(1);
  gfx->setCursor(12, y);
  gfx->println("One-time probes only. No joins, no scans.");

  y += 20;
  for (size_t index = 0; index < gRowCount; ++index) {
    const StatusRow &row = gRows[index];

    gfx->setTextSize(2);
    gfx->setTextColor(kColorTitle);
    gfx->setCursor(12, y);
    gfx->print(row.label);

    gfx->setCursor(250, y);
    gfx->setTextColor(row.ok ? kColorOk : kColorBad);
    gfx->print(row.ok ? "OK" : "MISS");

    y += 16;
    gfx->setTextSize(1);
    gfx->setTextColor(kColorMuted);
    gfx->setCursor(18, y);
    gfx->println(row.detail);
    y += 16;
  }
}

void collectStatuses()
{
  gRowCount = 0;
  addStatus("Display", true, "SH8601 panel online");

  addStatus("I2C Rails", gExpanderReady, gExpanderReady ? "XCA9554 online" : "XCA9554 missing @ 0x20");

  addStatus("PMU", probeI2c(kPmuAddress), "AXP2101 @ 0x34");
  addStatus("RTC", probeI2c(kRtcAddress), "PCF85063 @ 0x51");
  addStatus("Touch", probeI2c(kTouchAddress), "FT3168 @ 0x38");
  addStatus("IMU", probeI2c(kImuAddress), "QMI8658 @ 0x6B");
  addStatus("Audio", probeI2c(kAudioCodecAddress), "ES8311 @ 0x18");

  String sdDetail;
  addStatus("SD Card", initSdCard(sdDetail), sdDetail);

  String wifiDetail;
  addStatus("Wi-Fi", initWifi(wifiDetail), wifiDetail);

  String bleDetail;
  addStatus("BLE", initBle(bleDetail), bleDetail);
}
} // namespace

void setup()
{
  Serial.begin(115200);
  delay(250);

  Wire.begin(IIC_SDA, IIC_SCL);
  Wire.setClock(400000);
  pinMode(TP_INT, INPUT_PULLUP);
  pinMode(IMU_IRQ, INPUT_PULLUP);

  // The panel stays dark unless the expander-controlled rails are enabled first.
  gExpanderReady = initExpanderAndRails();
  if (!gExpanderReady) {
    Serial.println("XCA9554 not found; display may stay dark");
  }

  if (!gfx->begin()) {
    Serial.println("Display init failed");
    while (true) {
      delay(1000);
    }
  }

  gfx->setBrightness(255);
  gfx->displayOn();

  collectStatuses();
  drawStatusScreen();

  for (size_t index = 0; index < gRowCount; ++index) {
    Serial.printf("%s: %s (%s)\n",
                  gRows[index].label,
                  gRows[index].ok ? "OK" : "MISS",
                  gRows[index].detail.c_str());
  }
}

void loop()
{
  delay(1000);
}
