#include "Wire.h"
#include <driver/i2s.h>
#include "pin_config.h"
#include "esp_check.h"
#include "es8311.h"
#include "Arduino_GFX_Library.h"
#include "Arduino_DriveBus_Library.h"
#include <Adafruit_XCA9554.h>

#define EXAMPLE_SAMPLE_RATE 16000
#define EXAMPLE_VOICE_VOLUME 85
#define EXAMPLE_MIC_GAIN ES8311_MIC_GAIN_18DB

#define I2C_NUM 0

#define I2S_MCK_IO 16
#define I2S_BCK_IO 9
#define I2S_DI_IO 10
#define I2S_WS_IO 45
#define I2S_DO_IO 8

// Max recording: 10 seconds of 16-bit mono @ 16kHz = 320KB
#define MAX_RECORD_SIZE (EXAMPLE_SAMPLE_RATE * 2 * 10)
#define READ_CHUNK 4000

const char *TAG = "es8311_recorder";

Adafruit_XCA9554 expander;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_SH8601 *gfx = new Arduino_SH8601(bus, GFX_NOT_DEFINED, 0, LCD_WIDTH, LCD_HEIGHT);

std::shared_ptr<Arduino_IIC_DriveBus> IIC_Bus =
  std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);

void Arduino_IIC_Touch_Interrupt(void);
std::unique_ptr<Arduino_IIC> FT3168(new Arduino_FT3x68(IIC_Bus, FT3168_DEVICE_ADDRESS, DRIVEBUS_DEFAULT_VALUE, TP_INT, Arduino_IIC_Touch_Interrupt));

volatile bool touchFlag = false;
void Arduino_IIC_Touch_Interrupt(void) { touchFlag = true; }

static uint8_t *rec_buf = nullptr;
static size_t rec_len = 0;
static bool touching = false;
static bool touchPressed = false;

uint32_t playbackDurationMs(size_t bytes)
{
  const uint32_t bytesPerSecond = EXAMPLE_SAMPLE_RATE * 2;
  if (bytesPerSecond == 0) {
    return 0;
  }
  return static_cast<uint32_t>((static_cast<uint64_t>(bytes) * 1000ULL) / bytesPerSecond);
}

void initI2s()
{
  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  cfg.sample_rate = EXAMPLE_SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = 128;
  cfg.use_apll = true;
  cfg.fixed_mclk = 12288000;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = I2S_BCK_IO;
  pins.ws_io_num = I2S_WS_IO;
  pins.data_in_num = I2S_DI_IO;
  pins.data_out_num = I2S_DO_IO;
  pins.mck_io_num = I2S_MCK_IO;

  i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_set_clk(I2S_NUM_0, EXAMPLE_SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

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
  ESP_RETURN_ON_ERROR(es8311_voice_mute(es_handle, false), TAG, "unmute failed");
  return ESP_OK;
}

bool isTouching() {
  bool shouldRead = touchPressed || touchFlag || digitalRead(TP_INT) == LOW;
  if (shouldRead) {
    int fingers = static_cast<int>(FT3168->IIC_Read_Device_Value(
      FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER));
    touchPressed = fingers > 0;
    touchFlag = false;
  }
  return touchPressed;
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
  gfx->fillScreen(0x0000);

  pinMode(PA, OUTPUT);
  digitalWrite(PA, HIGH);
  initI2s();

  es8311_codec_init();

  rec_buf = (uint8_t *)ps_malloc(MAX_RECORD_SIZE);
  if (!rec_buf) {
    rec_buf = (uint8_t *)malloc(MAX_RECORD_SIZE);
  }
  if (!rec_buf) {
    Serial.println("Record buffer alloc failed");
  }

  Serial.println("Ready — touch to record, release to play");
}

void loop() {
  bool nowTouching = isTouching();

  // Touch just started — begin recording
  if (nowTouching && !touching) {
    touching = true;
    rec_len = 0;
    gfx->fillScreen(0xF800);
  }

  // Still touching — keep recording
  if (touching) {
    if (nowTouching) {
      if (rec_buf && rec_len + READ_CHUNK <= MAX_RECORD_SIZE) {
        size_t bytes_read = 0;
        if (i2s_read(I2S_NUM_0, rec_buf + rec_len, READ_CHUNK, &bytes_read, portMAX_DELAY) == ESP_OK) {
          if (bytes_read > 0) rec_len += bytes_read;
        }
      }
    } else {
      touching = false;
      gfx->fillScreen(0xFFFF);
      if (rec_buf && rec_len > 0) {
        size_t written = 0;
        while (written < rec_len) {
          size_t chunk = rec_len - written;
          if (chunk > READ_CHUNK) {
            chunk = READ_CHUNK;
          }
          size_t justWritten = 0;
          if (i2s_write(I2S_NUM_0, rec_buf + written, chunk, &justWritten, portMAX_DELAY) != ESP_OK || justWritten == 0) {
            break;
          }
          written += justWritten;
        }
        delay(playbackDurationMs(rec_len) + 50);
      }
      gfx->fillScreen(0x0000);
    }
  }

  delay(5);
}
