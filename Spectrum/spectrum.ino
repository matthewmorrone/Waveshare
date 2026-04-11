#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "Arduino_DriveBus_Library.h"
#include "../Shared/pin_config.h"
#include "HWCDC.h"
#include <Adafruit_XCA9554.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>
#include "../Shared/font.h"

extern "C" {
  #include "es8311.h"
}

HWCDC USBSerial;

// ===================== Display =====================
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3
);

Arduino_SH8601 *gfx = new Arduino_SH8601(
  bus, GFX_NOT_DEFINED, 0, 368, 448
);

Adafruit_XCA9554 expander;

// ===================== USER CONTROLS =====================
int gg=3;
bool deb=0;

unsigned short dark=0x538E;
unsigned short darker=0x3A6A;
unsigned short lineCol=0x6D97;
unsigned short peekLine=0x2B6F;

float FMIN_HZ = 300.0f;
float NOISE_GATE = 0.003f;
float COMP_EXP = 1.35f;   // v1 je 1.5  v2 1.1 v3 1.3
float TREBLE_BOOST = 1.4; // v1 je 1.3  v3 1.8 1.8

static constexpr int BANDS = 24;
float peakHold[BANDS] = {0};
static constexpr int BAR_W = 8;
static constexpr int GAP_W = 5;

static constexpr int GRAPH_W = (BANDS * BAR_W) + ((BANDS - 1) * GAP_W);
static constexpr int GRAPH_H = 230;
static constexpr int GRAPH_Y_TOP = 120;

static constexpr uint16_t BG = RGB565_BLACK;

GFXcanvas16 *canvas;
GFXcanvas16 *canvas2;
GFXcanvas16 *canvas3;

// ===================== ES8311 / I2C =====================
static constexpr int PIN_I2C_SCL = 14;
static constexpr int PIN_I2C_SDA = 15;
static constexpr uint32_t I2C_FREQ = 400000;
static constexpr uint8_t ES8311_ADDR = ES8311_ADDRESS_0;
es8311_handle_t g_codec = nullptr;

// ===================== I2S pins =====================
static constexpr int PIN_I2S_BCLK = 9;
static constexpr int PIN_I2S_WS   = 45;
static constexpr int PIN_I2S_DIN  = 10;
static constexpr int PIN_I2S_DOUT = 8;
static constexpr int PIN_I2S_MCLK = 16;

// ===================== FFT =====================
static constexpr uint32_t SAMPLE_RATE = 16000;
static constexpr uint16_t N = 128;

int16_t i2sBuf[N];
float vReal[N];
float vImag[N];

ArduinoFFT<float> FFT(vReal, vImag, N, SAMPLE_RATE);

uint8_t bands24[BANDS];

uint16_t barColors[24] = {
  0xF800,0xF940,0xFBE0,0xFD20,0xFFE0,0xBFE0,
  0x87E0,0x07E0,0x07F0,0x07FF,0x07DF,0x03BF,
  0x001F,0x401F,0x781F,0xA01F,0xF81F,0xF81B,
  0xF819,0xF818,0xF9A0,0xFA00,0xF400,0xE000
};

// ===================== VU METER =====================
float vu_rms = 0;
float vu_db = -40;

void initExpander() {
  bool expanderReady = false;
  for (int attempt = 0; attempt < 3 && !expanderReady; ++attempt) {
    if (attempt > 0) {
      Wire.end();
      delay(10);
      Wire.begin(IIC_SDA, IIC_SCL, I2C_FREQ);
      delay(10);
    }
    expanderReady = expander.begin(0x20);
  }

  if (!expanderReady) {
    USBSerial.println("I2C expander not found");
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

// =====================================================
// I2S INIT
// =====================================================
void i2s_init_rx() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = 128;
  cfg.use_apll = true;
  cfg.fixed_mclk = 12288000;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = PIN_I2S_BCLK;
  pins.ws_io_num  = PIN_I2S_WS;
  pins.data_in_num = PIN_I2S_DIN;
  pins.data_out_num = PIN_I2S_DOUT;
  pins.mck_io_num  = PIN_I2S_MCLK;

  i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_set_clk(I2S_NUM_0, SAMPLE_RATE,
              I2S_BITS_PER_SAMPLE_16BIT,
              I2S_CHANNEL_MONO);
}

// =====================================================
// ES8311 INIT
// =====================================================
bool es8311_init_for_mic() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ);
  g_codec = es8311_create(0, ES8311_ADDR);

  es8311_clock_config_t clk = {};
  clk.mclk_from_mclk_pin = true;
  clk.mclk_frequency     = 12288000;
  clk.sample_frequency   = SAMPLE_RATE;

  if (es8311_init(g_codec, &clk,
                  ES8311_RESOLUTION_16,
                  ES8311_RESOLUTION_16) != ESP_OK)
    return false;

  if (es8311_microphone_config(g_codec, false) != ESP_OK)
    return false;

  es8311_microphone_gain_set(g_codec, ES8311_MIC_GAIN_18DB);
  return true;
}

// =====================================================
// FFT → 24 LOG BANDS
// =====================================================
void fftToBands24(const float *mag, uint8_t *out) {

  float fMax = SAMPLE_RATE / 2.0f;
  float binHz = (float)SAMPLE_RATE / (float)N;
  static float smooth[BANDS] = {0};
  for (int b = 0; b < BANDS; b++) {
    float t0 = (float)b / (float)BANDS;
    float t1 = (float)(b + 1) / (float)BANDS;

    float f0 = powf(10.0f, log10f(FMIN_HZ) + t0 * (log10f(fMax) - log10f(FMIN_HZ)));
    float f1 = powf(10.0f, log10f(FMIN_HZ) + t1 * (log10f(fMax) - log10f(FMIN_HZ)));

    int k0 = max(2, (int)(f0 / binHz));
    int k1 = min((N/2) - 1, (int)(f1 / binHz));
    if (k1 <= k0) k1 = k0 + 1;

    float m = 0;
    for (int k = k0; k <= k1; k++) m += mag[k];
    m /= (float)(k1 - k0 + 1);

    float v = m / 900.0f;
    v = constrain(v, 0.0f, 1.0f);
    v = powf(v, 0.60f);

    float px = v * 255.0f;

    // Smoothing (attack + release)
  if (px > smooth[b])
{
    //  (attack)
    smooth[b] = smooth[b] * 0.15f + px * 0.85f;
}
else
{
    // (release)
    smooth[b] = smooth[b] * 0.88f + px * 0.12f;
}


    out[b] = constrain((int)smooth[b], 0, 255);
  }
}

// =====================================================
// DRAW BARS
// =====================================================
void drawBarsCanvas(GFXcanvas16 *cv, const uint8_t *vals)
{
  cv->fillScreen(BG);

  int x = 0;
  for (int b = 0; b < BANDS; b++) {

    int v = vals[b];
    int bh = (v * GRAPH_H) / 255;
    if (bh < 7) bh = 7;

    // === DRAW MAIN BAR ===
    int barTop = GRAPH_H/2 - bh/2;
    cv->fillRect(x, barTop, BAR_W, bh, barColors[b]);

    // === PEAK HOLD UPDATE ===
    float peak = peakHold[b];

    if (bh > peak)
      peak = bh;                 // instant rise
    else
      peak -= 2;                 // slow fall (2 px per frame)

    if (peak < 0) peak = 0;
    peakHold[b] = peak;

    // === DRAW PEAK HOLD LINE ===
    int peakY = GRAPH_H/2 - peak/2 - 3;   // 3 px above
    cv->fillRect(x, peakY, BAR_W, 3, peekLine);

    x += BAR_W + GAP_W;
  }
}


void processBands24(uint8_t *vals)
{
  for (int i = 0; i < BANDS; i++) {
    float x = vals[i] / 255.0f;
    if (x < NOISE_GATE) x = 0.0f;
    x = powf(x, COMP_EXP);
    x *= (1.0f + (float)i / (float)BANDS * TREBLE_BOOST);
    if (x > 1.0f) x = 1.0f;
    vals[i] = (uint8_t)(x * 255.0f);
  }
}


void drawHeader() {
  canvas2->fillScreen(BG);

  // === TEXT ===
  canvas2->setCursor(0, 16);
  canvas2->print("INPUT GAIN:");

  canvas2->setCursor(98, 16);
  canvas2->print(gg);

  // === DRAW TO SCREEN ===
  gfx->draw16bitRGBBitmap(
    20, 415,
    canvas2->getBuffer(),
    140, 18
  );
}

float vu_smooth = 0;

void drawVu() {
    canvas3->fillScreen(BG);

    // Attack / Release smoothing
    if (vu_rms > vu_smooth) {
        vu_smooth = vu_smooth * 0.1f + vu_rms * 0.9f;   // fast attack
    } else {
        vu_smooth = vu_smooth * 0.7f + vu_rms * 0.3f;   // slow release
    }

    // Normalizacija
    float vu = vu_smooth / 300.0f;
    if (vu > 1.0f) vu = 1.0f;

    int seg = (int)(vu * 12.0f);


    for (int i = 0; i < 12; i++) {
        int x =  i * 9;
        canvas3->fillRect(x, 4, 6, 10, 0x29C7);
    }

    // Crtanje segmenata
    for (int i = 0; i < seg; i++) {
        int x =  i * 9;
        canvas3->fillRect(x, 4, 6, 10, 0x2D6D);
    }

    // === GLOW NA ZADNJEM SEGMENTU ===
    if (seg > 0) {
        int last = seg - 1;
        int x =  last * 9;

        // unutarnji glow (još svjetliji)
        canvas3->fillRect(x, 4, 6, 10, 0x5EEF);
    }

    gfx->draw16bitRGBBitmap(
        20, 400,
        canvas3->getBuffer(),
        120, 18
    );
}


// SETUP
// =====================================================
void setup() {
  USBSerial.begin(115200);
  pinMode(0,INPUT_PULLUP);
  Wire.begin(IIC_SDA, IIC_SCL, I2C_FREQ);
  initExpander();

  gfx->begin();
  gfx->setBrightness(200);
  gfx->fillScreen(BG);

  canvas = new GFXcanvas16(GRAPH_W, GRAPH_H);
  canvas2 = new GFXcanvas16(140, 18);
  canvas3 = new GFXcanvas16(120, 18);
  canvas2->setFont(&Monospaced_plain_12);
  canvas2->setTextColor(lineCol);

  bool ok = es8311_init_for_mic();
  i2s_init_rx();
  i2s_zero_dma_buffer(I2S_NUM_0);

gfx->setTextSize(1);
gfx->setTextColor(0x865A);   // vidi se 100%
gfx->setCursor(20, GRAPH_Y_TOP - 32);
gfx->setFont(&Open_Sans_Condensed_Bold_18);
gfx->print("SPECTRUM ANALYZER");


 gfx->setTextColor(lineCol);   // vidi se 100%
 gfx->setFont(&Monospaced_plain_12);

    int x0 = (368 - GRAPH_W) / 2;
    gfx->drawFastHLine(20, GRAPH_Y_TOP -16, 330, dark);
    gfx->drawFastHLine(20, GRAPH_Y_TOP -17, 330, dark);
    gfx->drawFastHLine(20, GRAPH_Y_TOP + GRAPH_H + 16, 330, dark);
    gfx->drawFastHLine(20, GRAPH_Y_TOP + GRAPH_H + 17, 330, dark);
    // === BAND 1 (320 Hz) ===
    int bx = x0 + 0*(BAR_W + GAP_W);
    gfx->fillRect(bx + BAR_W/2 - 1, GRAPH_Y_TOP + GRAPH_H + 10, 4, 14, lineCol); // vertical 
    gfx->fillRect(bx + BAR_W/2 - 1, GRAPH_Y_TOP-20, 3, 14, lineCol);
    gfx->setCursor(bx - 8, GRAPH_Y_TOP + GRAPH_H + 38);
    gfx->print("300");

    // === BAND 6 (520 Hz) ===
    bx = x0 + 5*(BAR_W + GAP_W);
    gfx->fillRect(bx + BAR_W/2 - 1, GRAPH_Y_TOP + GRAPH_H + 10, 3, 14, lineCol);
    gfx->fillRect(bx + BAR_W/2 - 1, GRAPH_Y_TOP-20, 3, 14, lineCol);
    gfx->setCursor(bx - 8, GRAPH_Y_TOP + GRAPH_H + 38);
    gfx->print("520");

    // === BAND 12 (930 Hz) ===
    bx = x0 + 11*(BAR_W + GAP_W);
    gfx->fillRect(bx + BAR_W/2 - 1, GRAPH_Y_TOP + GRAPH_H + 10, 3, 14, lineCol);
     gfx->fillRect(bx + BAR_W/2 - 1, GRAPH_Y_TOP-20, 3, 14, lineCol);
    gfx->setCursor(bx - 8, GRAPH_Y_TOP + GRAPH_H + 38);
    gfx->print("930");

    // === BAND 18 (1660 Hz) ===
    bx = x0 + 17*(BAR_W + GAP_W);
    gfx->fillRect(bx + BAR_W/2 - 1, GRAPH_Y_TOP + GRAPH_H + 10, 3, 14, lineCol);
     gfx->fillRect(bx + BAR_W/2 - 1, GRAPH_Y_TOP-20, 3, 14, lineCol);
    gfx->setCursor(bx - 8, GRAPH_Y_TOP + GRAPH_H + 38);
    gfx->print("1.6k");

    // === BAND 24 (2960 Hz) ===
    bx = x0 + 23*(BAR_W + GAP_W);
    gfx->fillRect(bx + BAR_W/2 - 1, GRAPH_Y_TOP + GRAPH_H + 10, 3, 14, lineCol);
     gfx->fillRect(bx + BAR_W/2 - 1, GRAPH_Y_TOP-20, 3, 14, lineCol);
    gfx->setCursor(bx - 8, GRAPH_Y_TOP + GRAPH_H + 38);
    gfx->print("2.9k");

     gfx->setTextColor(dark);
     gfx->setCursor(240, GRAPH_Y_TOP -32);
     gfx->print("VOLOS PROJECTS");
}


bool started=0;
void loop() {

   if(!started)
   {
    started=1;
    drawHeader();
   }

  if(digitalRead(0)==0)
  {
  if(deb==0) {
    deb=1;
    gg++;
    if(gg>8) gg=0;
    if(gg==0) es8311_microphone_gain_set(g_codec, ES8311_MIC_GAIN_0DB);
    if(gg==1) es8311_microphone_gain_set(g_codec, ES8311_MIC_GAIN_6DB);
    if(gg==2) es8311_microphone_gain_set(g_codec, ES8311_MIC_GAIN_12DB);
    if(gg==3) es8311_microphone_gain_set(g_codec, ES8311_MIC_GAIN_18DB);
    if(gg==4) es8311_microphone_gain_set(g_codec, ES8311_MIC_GAIN_24DB);
    if(gg==5) es8311_microphone_gain_set(g_codec, ES8311_MIC_GAIN_30DB);
    if(gg==6) es8311_microphone_gain_set(g_codec, ES8311_MIC_GAIN_36DB);
    if(gg==7) es8311_microphone_gain_set(g_codec, ES8311_MIC_GAIN_42DB);
    if(gg==8) es8311_microphone_gain_set(g_codec, ES8311_MIC_GAIN_MAX);
    drawHeader();
  }
  }else deb=0;

  size_t bytesRead = 0;
  if (i2s_read(I2S_NUM_0, i2sBuf, sizeof(i2sBuf),
               &bytesRead, portMAX_DELAY) != ESP_OK)
    return;

  if (bytesRead < N * sizeof(int16_t))
    return;

  float sum = 0;
  for (int i = 0; i < N; i++) sum += i2sBuf[i];
  float mean = sum / N;

  for (int i = 0; i < N; i++) {
    vReal[i] = (float)i2sBuf[i] - mean;
    vImag[i] = 0;
  }

  // === VU METER ===
  float sumsq = 0;
  for (int i = 0; i < N; i++) {
    float s = vReal[i];
    sumsq += s * s;
  }

  vu_rms = sqrtf(sumsq / N);


  // === FFT ===
  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(FFT_FORWARD);
  FFT.complexToMagnitude();

  fftToBands24(vReal, bands24);
  processBands24(bands24);
  drawBarsCanvas(canvas, bands24);

  int x0 = (368 - GRAPH_W) / 2;

  gfx->draw16bitRGBBitmap(x0, GRAPH_Y_TOP,canvas->getBuffer(),GRAPH_W, GRAPH_H);
  drawVu();
}
