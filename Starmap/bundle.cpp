#include <Adafruit_XCA9554.h>
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <lvgl.h>
#include <math.h>

#include "ota_config.h"
#include "pin_config.h"
#include "screen_manager.h"
#include "screen_callbacks.h"

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

Adafruit_XCA9554 expander;

lv_display_t *display = nullptr;
lv_indev_t *touchInput = nullptr;
lv_obj_t *screenRoots[24] = {};

namespace
{
constexpr uint8_t kActiveBrightness = 255;
constexpr uint16_t kBackgroundColor = 0x0000;
constexpr uint32_t kLvglBufferRows = 40;

uint8_t *lvBuffer = nullptr;
bool expanderReady = false;
uint32_t lastMemoryReportAtMs = 0;
constexpr uint32_t kMemoryReportIntervalMs = 10000;

void flushDisplay(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap)
{
  LV_UNUSED(disp);
  uint16_t width = static_cast<uint16_t>(area->x2 - area->x1 + 1);
  uint16_t height = static_cast<uint16_t>(area->y2 - area->y1 + 1);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(pxMap), width, height);
  lv_display_flush_ready(display);
}
} // namespace

lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b)
{
  return lv_color_make(r, g, b);
}

void applyRootStyle(lv_obj_t *obj)
{
  lv_obj_set_size(obj, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_style_bg_color(obj, lvColor(0, 0, 0), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_outline_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void styleLine(lv_obj_t *line, lv_color_t color, int width)
{
  lv_obj_set_style_line_color(line, color, 0);
  lv_obj_set_style_line_width(line, width, 0);
  lv_obj_set_style_line_rounded(line, true, 0);
}

bool otaUpdateInProgress()
{
  return false;
}

bool lightSleepActive()
{
  return false;
}

void initExpander()
{
  expanderReady = false;
  for (int attempt = 0; attempt < 3 && !expanderReady; ++attempt) {
    if (attempt > 0) {
      Wire.end();
      delay(10);
      Wire.begin(IIC_SDA, IIC_SCL);
      delay(10);
    }
    expanderReady = expander.begin(0x20);
  }
  if (!expanderReady) {
    Serial.println("I2C expander not found");
    return;
  }

  expander.pinMode(PMU_IRQ_PIN, INPUT);
  expander.pinMode(TOP_BUTTON_PIN, INPUT);
  expander.pinMode(0, OUTPUT);
  expander.pinMode(1, OUTPUT);
  expander.pinMode(2, OUTPUT);
  expander.pinMode(6, OUTPUT);
  expander.pinMode(7, OUTPUT);
  expander.digitalWrite(0, LOW);
  expander.digitalWrite(1, LOW);
  expander.digitalWrite(2, LOW);
  expander.digitalWrite(6, LOW);
  delay(20);
  expander.digitalWrite(0, HIGH);
  expander.digitalWrite(1, HIGH);
  expander.digitalWrite(2, HIGH);
  expander.digitalWrite(6, HIGH);
  expander.digitalWrite(7, HIGH);
  delay(20);
}

void setupLvgl()
{
  lv_init();
  lv_tick_set_cb(millis);

  display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
  size_t bufferPixelCount = LCD_WIDTH * kLvglBufferRows;
  lvBuffer = static_cast<uint8_t *>(malloc(bufferPixelCount * sizeof(lv_color16_t)));
  if (!lvBuffer) {
    Serial.println("LVGL buffer allocation failed");
    while (true) {
      delay(1000);
    }
  }

  lv_display_set_buffers(display, lvBuffer, nullptr, bufferPixelCount * sizeof(lv_color16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flushDisplay);
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  uint32_t serialWaitStartedAt = millis();
  while (!Serial && millis() - serialWaitStartedAt < 2000) {
    delay(10);
  }
  delay(250);
  Serial.printf("Starmap boot. reset_reason=%d\n", static_cast<int>(esp_reset_reason()));

  Wire.begin(IIC_SDA, IIC_SCL);
  initExpander();

  if (!gfx->begin()) {
    Serial.println("Display initialization failed");
    while (true) {
      delay(1000);
    }
  }

  gfx->setBrightness(kActiveBrightness);
  gfx->fillScreen(kBackgroundColor);
  gfx->displayOn();

  setupLvgl();

  if (!waveformBuildStarmapScreen()) {
    Serial.println("Starmap screen build failed");
    while (true) {
      delay(1000);
    }
  }

  lv_screen_load(screenRoots[static_cast<size_t>(ScreenId::Starmap)]);
  waveformEnterStarmapScreen();
  waveformRefreshStarmapScreen();
  lv_refr_now(display);
}

void loop()
{
  uint32_t now = millis();
  if (now - lastMemoryReportAtMs >= kMemoryReportIntervalMs) {
    lastMemoryReportAtMs = now;
    Serial.printf("Memory: free_heap=%u min_free_heap=%u largest_block=%u\n",
                  ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(),
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  }

  waveformTickStarmapScreen(now);
  lv_timer_handler();
  delay(5);
}
