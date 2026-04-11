#include "Wire.h"
#include "ESP_I2S.h"
I2SClass i2s;
#include "pin_config.h"
#include "esp_check.h"
#include "es8311.h"
#include "Arduino_GFX_Library.h"
#include "Arduino_DriveBus_Library.h"
#include <Adafruit_XCA9554.h>

#define EXAMPLE_SAMPLE_RATE 16000
#define EXAMPLE_VOICE_VOLUME 85
#define EXAMPLE_MIC_GAIN (es8311_mic_gain_t)(3)

#define I2C_NUM 0

#define I2S_MCK_IO 16
#define I2S_BCK_IO 9
#define I2S_DI_IO 10
#define I2S_WS_IO 45
#define I2S_DO_IO 8

#define ECHO_BUF_SIZE 4000

const char *TAG = "es8311_live_echo";

Adafruit_XCA9554 expander;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_SH8601 *gfx = new Arduino_SH8601(
  bus, GFX_NOT_DEFINED, 0, LCD_WIDTH, LCD_HEIGHT);

std::shared_ptr<Arduino_IIC_DriveBus> IIC_Bus =
  std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);

void Arduino_IIC_Touch_Interrupt(void);
std::unique_ptr<Arduino_IIC> FT3168(new Arduino_FT3x68(
  IIC_Bus, FT3168_DEVICE_ADDRESS, DRIVEBUS_DEFAULT_VALUE, TP_INT, Arduino_IIC_Touch_Interrupt));

volatile bool touchFlag = false;
void Arduino_IIC_Touch_Interrupt(void) { touchFlag = true; }

static uint8_t echo_buf[ECHO_BUF_SIZE];
static bool active = false;

esp_err_t es8311_codec_init(void) {
  es8311_handle_t es_handle = es8311_create(I2C_NUM, ES8311_ADDRRES_0);
  ESP_RETURN_ON_FALSE(es_handle, ESP_FAIL, TAG, "es8311 create failed");
  const es8311_clock_config_t es_clk = {
    .mclk_inverted = false,
    .sclk_inverted = false,
    .mclk_from_mclk_pin = true,
    .mclk_frequency = EXAMPLE_SAMPLE_RATE * 256,
    .sample_frequency = EXAMPLE_SAMPLE_RATE
  };
  ESP_ERROR_CHECK(es8311_init(es_handle, &es_clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16));
  ESP_RETURN_ON_ERROR(es8311_sample_frequency_config(es_handle, es_clk.mclk_frequency, es_clk.sample_frequency), TAG, "set sample freq failed");
  ESP_RETURN_ON_ERROR(es8311_microphone_config(es_handle, false), TAG, "set mic failed");
  ESP_RETURN_ON_ERROR(es8311_voice_volume_set(es_handle, EXAMPLE_VOICE_VOLUME, NULL), TAG, "set volume failed");
  ESP_RETURN_ON_ERROR(es8311_microphone_gain_set(es_handle, EXAMPLE_MIC_GAIN), TAG, "set mic gain failed");
  return ESP_OK;
}

void setup() {
  Serial.begin(115200);

  Wire.begin(IIC_SDA, IIC_SCL);

  if (!expander.begin(0x20)) {
    Serial.println("XCA9554 not found");
    while (1);
  }
  expander.pinMode(0, OUTPUT);
  expander.pinMode(1, OUTPUT);
  expander.pinMode(2, OUTPUT);
  expander.digitalWrite(0, LOW);
  expander.digitalWrite(1, LOW);
  expander.digitalWrite(2, LOW);
  delay(20);
  expander.digitalWrite(0, HIGH);
  expander.digitalWrite(1, HIGH);
  expander.digitalWrite(2, HIGH);

  while (FT3168->begin() == false) {
    Serial.println("FT3168 init fail");
    delay(2000);
  }
  FT3168->IIC_Write_Device_State(
    FT3168->Arduino_IIC_Touch::Device::TOUCH_POWER_MODE,
    FT3168->Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_MONITOR);

  gfx->begin();
  gfx->setBrightness(200);
  gfx->fillScreen(0x0000);  // black = idle

  pinMode(PA, OUTPUT);
  digitalWrite(PA, HIGH);
  i2s.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO, I2S_MCK_IO);
  if (!i2s.begin(I2S_MODE_STD, EXAMPLE_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("I2S init failed");
    return;
  }

  es8311_codec_init();

  Serial.println("Ready — touch screen for live echo");
}

void loop() {
  bool nowTouching = touchFlag;
  if (touchFlag) touchFlag = false;

  if (nowTouching && !active) {
    active = true;
    gfx->fillScreen(0xF800);  // red = live echo active
    Serial.println("Echo ON");
  } else if (!nowTouching && active) {
    active = false;
    gfx->fillScreen(0x0000);  // black = idle
    Serial.println("Echo OFF");
  }

  if (active) {
    size_t bytes_read = i2s.readBytes((char *)echo_buf, ECHO_BUF_SIZE);
    if (bytes_read > 0) {
      i2s.write(echo_buf, bytes_read);
    }
  } else {
    delay(10);
  }
}
