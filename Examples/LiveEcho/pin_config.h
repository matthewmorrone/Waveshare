#pragma once

#define XPOWERS_CHIP_AXP2101

#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 11
#define LCD_CS 12

#define LCD_WIDTH 368
#define LCD_HEIGHT 448

#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 21
#define IMU_IRQ 33

#define TOP_BUTTON_PIN 4
#define PMU_IRQ_PIN 5

constexpr int SDMMC_CLK = 2;
constexpr int SDMMC_CMD = 1;
constexpr int SDMMC_DATA = 3;

constexpr int AUDIO_I2S_MCLK = 16;
constexpr int AUDIO_I2S_BCLK = 9;
constexpr int AUDIO_I2S_DIN = 10;
constexpr int AUDIO_I2S_WS = 45;
constexpr int AUDIO_I2S_DOUT = 8;

#define PA 46
