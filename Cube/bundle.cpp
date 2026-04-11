#include <Adafruit_XCA9554.h>
#include <Arduino.h>
#include <Arduino_DriveBus_Library.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <lvgl.h>
#include <memory>

#include "pin_config.h"
#include "screen_constants.h"
#include "screen_manager.h"
#include "screen_callbacks.h"
#include "imu_module.h"

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
std::shared_ptr<Arduino_IIC_DriveBus> touchBus = std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);
std::unique_ptr<Arduino_IIC> touchController(
    new Arduino_FT3x68(touchBus, FT3168_DEVICE_ADDRESS, DRIVEBUS_DEFAULT_VALUE, TP_INT, nullptr));

lv_display_t *display = nullptr;
lv_indev_t *touchInput = nullptr;
lv_obj_t *screenRoots[24] = {};

bool inLightSleep = false;
bool touchPressed = false;

namespace
{
constexpr uint8_t kActiveBrightness = 255;
constexpr uint16_t kBackgroundColor = 0x0000;
constexpr uint32_t kLvglBufferRows = 40;

uint8_t *lvBuffer = nullptr;
bool expanderReady = false;
bool touchReady = false;
int16_t touchLastX = LCD_WIDTH / 2;
int16_t touchLastY = LCD_HEIGHT / 2;
uint32_t lastRefreshAtMs = 0;

void handleTouchInterrupt()
{
  if (touchController) {
    touchController->IIC_Interrupt_Flag = true;
  }
}

void handleMotionScreenClick(lv_event_t *event)
{
  LV_UNUSED(event);
  cubeHandleTap();
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

void noteActivity()
{
}

void setObjYOffset(void *obj, int32_t value)
{
  lv_obj_set_y(static_cast<lv_obj_t *>(obj), static_cast<lv_coord_t>(value));
}

void enableTapBubbling(lv_obj_t *obj)
{
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
}

void styleLine(lv_obj_t *line, lv_color_t color, int width)
{
  lv_obj_set_style_line_color(line, color, 0);
  lv_obj_set_style_line_width(line, width, 0);
  lv_obj_set_style_line_rounded(line, true, 0);
}

bool showScreenById(ScreenId id)
{
  return id == ScreenId::Cube;
}

void flushDisplay(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap)
{
  LV_UNUSED(disp);
  uint16_t width = static_cast<uint16_t>(area->x2 - area->x1 + 1);
  uint16_t height = static_cast<uint16_t>(area->y2 - area->y1 + 1);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(pxMap), width, height);
  lv_display_flush_ready(display);
}

void readTouch(lv_indev_t *indev, lv_indev_data_t *data)
{
  LV_UNUSED(indev);

  if (!touchReady) {
    data->state = LV_INDEV_STATE_RELEASED;
    data->point.x = touchLastX;
    data->point.y = touchLastY;
    return;
  }

  bool shouldRead = touchPressed || touchController->IIC_Interrupt_Flag || digitalRead(TP_INT) == LOW;
  if (shouldRead) {
    int fingers = static_cast<int>(touchController->IIC_Read_Device_Value(
        touchController->Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER));
    touchPressed = fingers > 0;
    if (touchPressed) {
      touchLastX = static_cast<int16_t>(touchController->IIC_Read_Device_Value(
          touchController->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X));
      touchLastY = static_cast<int16_t>(touchController->IIC_Read_Device_Value(
          touchController->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y));
    }
    touchController->IIC_Interrupt_Flag = false;
  }

  data->point.x = touchLastX;
  data->point.y = touchLastY;
  data->state = touchPressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
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

bool initTouch()
{
  if (!touchController) {
    return false;
  }

  touchReady = touchController->begin();
  if (!touchReady) {
    Serial.println("Touch controller not found");
    return false;
  }

  touchController->IIC_Write_Device_State(
      touchController->Arduino_IIC_Touch::Device::TOUCH_POWER_MODE,
      touchController->Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_MONITOR);
  touchController->IIC_Interrupt_Flag = false;
  touchPressed = false;
  return true;
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

  touchInput = lv_indev_create();
  lv_indev_set_type(touchInput, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(touchInput, readTouch);
}

void setup()
{
  Serial.begin(115200);
  delay(250);

  Wire.begin(IIC_SDA, IIC_SCL);
  pinMode(TP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TP_INT), handleTouchInterrupt, FALLING);

  initExpander();

  gfx->begin();
  gfx->fillScreen(kBackgroundColor);
  gfx->setBrightness(kActiveBrightness);

  setupLvgl();
  initTouch();
  imuModuleInit();

  waveformBuildCubeScreen();
  waveformEnterCubeScreen();

  if (lv_obj_t *root = waveformCubeScreenRoot()) {
    lv_scr_load(root);
    lv_obj_t *tapTarget = waveformCubeTapTarget();
    if (!tapTarget) {
      tapTarget = root;
    }
    lv_obj_add_event_cb(tapTarget, handleMotionScreenClick, LV_EVENT_CLICKED, nullptr);
  }

  lastRefreshAtMs = millis();
}

void loop()
{
  uint32_t now = millis();
  imuModuleUpdate();

  if (now - lastRefreshAtMs >= kMotionRefreshMs) {
    lastRefreshAtMs = now;
    waveformTickCubeScreen(now);
  }

  lv_timer_handler();
  delay(5);
}
