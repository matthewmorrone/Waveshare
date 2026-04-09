/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "es8311.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp32-hal-i2c.h"
#include "es8311_reg.h"

typedef struct {
    unsigned int port;
    uint16_t dev_addr;
} es8311_dev_t;

struct coeff_div_t {
    uint32_t mclk;
    uint32_t rate;
    uint8_t pre_div;
    uint8_t pre_multi;
    uint8_t adc_div;
    uint8_t dac_div;
    uint8_t fs_mode;
    uint8_t lrck_h;
    uint8_t lrck_l;
    uint8_t bclk_div;
    uint8_t adc_osr;
    uint8_t dac_osr;
};

static const struct coeff_div_t coeff_div[] = {
    {12288000, 8000, 0x06, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 8000, 0x03, 0x01, 0x03, 0x03, 0x00, 0x05, 0xff, 0x18, 0x10, 0x10},
    {16384000, 8000, 0x08, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000, 8000, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 8000, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {4096000, 8000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 8000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2048000, 8000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 8000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1024000, 8000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {11289600, 11025, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800, 11025, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400, 11025, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200, 11025, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {12288000, 12000, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 12000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 12000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 12000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {12288000, 16000, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 16000, 0x03, 0x01, 0x03, 0x03, 0x00, 0x02, 0xff, 0x0c, 0x10, 0x10},
    {16384000, 16000, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000, 16000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 16000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {4096000, 16000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 16000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2048000, 16000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 16000, 0x03, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1024000, 16000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {11289600, 22050, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800, 22050, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400, 22050, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200, 22050, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {705600, 22050, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {12288000, 24000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 24000, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 24000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 24000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 24000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {12288000, 32000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 32000, 0x03, 0x02, 0x03, 0x03, 0x00, 0x02, 0xff, 0x0c, 0x10, 0x10},
    {16384000, 32000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000, 32000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 32000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {4096000, 32000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 32000, 0x03, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2048000, 32000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 32000, 0x03, 0x03, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},
    {1024000, 32000, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {11289600, 44100, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800, 44100, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400, 44100, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200, 44100, 0x01, 0x03, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},
    {12288000, 48000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 48000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 48000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 48000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 48000, 0x01, 0x03, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},
};

static const char *TAG = "ES8311";

static esp_err_t es8311_write_reg(es8311_handle_t dev, uint8_t reg_addr, uint8_t data)
{
    es8311_dev_t *es = (es8311_dev_t *)dev;
    const uint8_t write_buf[2] = {reg_addr, data};
    return i2cWrite(es->port, es->dev_addr, write_buf, sizeof(write_buf), 1000);
}

static esp_err_t es8311_read_reg(es8311_handle_t dev, uint8_t reg_addr, uint8_t *reg_value)
{
    es8311_dev_t *es = (es8311_dev_t *)dev;
    size_t read_count = 0;
    return i2cWriteReadNonStop(es->port, es->dev_addr, &reg_addr, 1, reg_value, 1, 1000, &read_count);
}

static int get_coeff(uint32_t mclk, uint32_t rate)
{
    for (int i = 0; i < (int)(sizeof(coeff_div) / sizeof(coeff_div[0])); ++i) {
        if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk) {
            return i;
        }
    }
    return -1;
}

esp_err_t es8311_sample_frequency_config(es8311_handle_t dev, int mclk_frequency, int sample_frequency)
{
    int coeff = get_coeff((uint32_t)mclk_frequency, (uint32_t)sample_frequency);
    if (coeff < 0) {
        ESP_LOGE(TAG, "Unable to configure sample rate %dHz with %dHz MCLK", sample_frequency, mclk_frequency);
        return ESP_ERR_INVALID_ARG;
    }

    const struct coeff_div_t *selected_coeff = &coeff_div[coeff];
    uint8_t regv = 0;

    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_CLK_MANAGER_REG02, &regv), TAG, "I2C read/write error");
    regv &= 0x07;
    regv |= (selected_coeff->pre_div - 1) << 5;
    regv |= selected_coeff->pre_multi << 3;
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG02, regv), TAG, "I2C read/write error");

    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG03, (selected_coeff->fs_mode << 6) | selected_coeff->adc_osr), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG04, selected_coeff->dac_osr), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG05, ((selected_coeff->adc_div - 1) << 4) | (selected_coeff->dac_div - 1)), TAG, "I2C read/write error");

    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_CLK_MANAGER_REG06, &regv), TAG, "I2C read/write error");
    regv &= 0xE0;
    regv |= (selected_coeff->bclk_div < 19) ? (selected_coeff->bclk_div - 1) : selected_coeff->bclk_div;
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG06, regv), TAG, "I2C read/write error");

    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_CLK_MANAGER_REG07, &regv), TAG, "I2C read/write error");
    regv &= 0xC0;
    regv |= selected_coeff->lrck_h;
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG07, regv), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG08, selected_coeff->lrck_l), TAG, "I2C read/write error");

    return ESP_OK;
}

static esp_err_t es8311_clock_config(es8311_handle_t dev, const es8311_clock_config_t *clk_cfg, es8311_resolution_t res)
{
    uint8_t reg06 = 0;
    uint8_t reg01 = 0x3F;
    int mclk_hz = 0;

    if (clk_cfg->mclk_from_mclk_pin) {
        mclk_hz = clk_cfg->mclk_frequency;
    } else {
        mclk_hz = clk_cfg->sample_frequency * (int)res * 2;
        reg01 |= BIT(7);
    }

    if (clk_cfg->mclk_inverted) {
        reg01 |= BIT(6);
    }
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG01, reg01), TAG, "I2C read/write error");

    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_CLK_MANAGER_REG06, &reg06), TAG, "I2C read/write error");
    if (clk_cfg->sclk_inverted) {
        reg06 |= BIT(5);
    } else {
        reg06 &= ~BIT(5);
    }
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_CLK_MANAGER_REG06, reg06), TAG, "I2C read/write error");

    return es8311_sample_frequency_config(dev, mclk_hz, clk_cfg->sample_frequency);
}

esp_err_t es8311_init(es8311_handle_t dev, const es8311_clock_config_t *clk_cfg, es8311_resolution_t res_in, es8311_resolution_t res_out)
{
    if (dev == NULL || clk_cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_RESET_REG00, 0x3F), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_RESET_REG00, 0x03), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_clock_config(dev, clk_cfg, res_out), TAG, "clock config failed");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SDPIN_REG09, 0x0C), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SDPOUT_REG0A, 0x0C), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SYSTEM_REG0B, 0x00), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SYSTEM_REG0C, 0x00), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SYSTEM_REG10, 0x1F), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SYSTEM_REG11, 0x7F), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SYSTEM_REG12, 0x00), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SYSTEM_REG13, 0x10), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SYSTEM_REG14, 0x18), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_ADC_REG15, 0x00), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_ADC_REG16, 0x24), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_ADC_REG17, 0xD0), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_ADC_REG18, 0x00), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_ADC_REG19, 0x00), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_ADC_REG1A, 0xA0), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_ADC_REG1B, 0x0A), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_ADC_REG1C, 0x6A), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_DAC_REG31, 0x00), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_DAC_REG32, 0xC0), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_DAC_REG33, 0x20), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_DAC_REG34, 0x00), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_DAC_REG35, 0xA0), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_DAC_REG37, 0x08), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_GP_REG45, 0x00), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SYSTEM_REG0D, 0x01), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SYSTEM_REG0E, 0x02), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(dev, ES8311_SYSTEM_REG0F, 0x00), TAG, "I2C read/write error");

    (void)res_in;
    return ESP_OK;
}

esp_err_t es8311_voice_volume_set(es8311_handle_t dev, int volume, int *volume_set)
{
    if (volume < 0) {
        volume = 0;
    }
    if (volume > 100) {
        volume = 100;
    }
    if (volume_set != NULL) {
        *volume_set = volume;
    }

    uint8_t reg = (uint8_t)(volume * 255 / 100);
    return es8311_write_reg(dev, ES8311_DAC_REG32, reg);
}

esp_err_t es8311_voice_volume_get(es8311_handle_t dev, int *volume)
{
    uint8_t reg = 0;
    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_DAC_REG32, &reg), TAG, "I2C read/write error");
    if (volume != NULL) {
        *volume = (reg * 100) / 255;
    }
    return ESP_OK;
}

void es8311_register_dump(es8311_handle_t dev)
{
    for (int i = 0; i <= ES8311_MAX_REGISTER; ++i) {
        uint8_t value = 0;
        if (es8311_read_reg(dev, (uint8_t)i, &value) == ESP_OK) {
            ESP_LOGI(TAG, "Reg 0x%02X = 0x%02X", i, value);
        }
    }
}

esp_err_t es8311_voice_mute(es8311_handle_t dev, bool mute)
{
    uint8_t reg = 0;
    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_DAC_REG31, &reg), TAG, "I2C read/write error");
    if (mute) {
        reg |= BIT(5);
    } else {
        reg &= (uint8_t)~BIT(5);
    }
    return es8311_write_reg(dev, ES8311_DAC_REG31, reg);
}

esp_err_t es8311_microphone_config(es8311_handle_t dev, bool digital_mic)
{
    uint8_t reg = 0;
    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_SYSTEM_REG14, &reg), TAG, "I2C read/write error");
    if (digital_mic) {
        reg |= BIT(6);
    } else {
        reg &= (uint8_t)~BIT(6);
    }
    return es8311_write_reg(dev, ES8311_SYSTEM_REG14, reg);
}

esp_err_t es8311_microphone_gain_set(es8311_handle_t dev, es8311_mic_gain_t gain_db)
{
    if (gain_db <= ES8311_MIC_GAIN_MIN || gain_db >= ES8311_MIC_GAIN_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t reg = 0;
    ESP_RETURN_ON_ERROR(es8311_read_reg(dev, ES8311_SYSTEM_REG14, &reg), TAG, "I2C read/write error");
    reg &= 0xE3;
    reg |= ((uint8_t)(gain_db - 1) << 2);
    return es8311_write_reg(dev, ES8311_SYSTEM_REG14, reg);
}

esp_err_t es8311_voice_fade(es8311_handle_t dev, es8311_fade_t fade)
{
    if (fade > ES8311_FADE_65536LRCK) {
        return ESP_ERR_INVALID_ARG;
    }
    return es8311_write_reg(dev, ES8311_DAC_REG37, (uint8_t)fade);
}

esp_err_t es8311_microphone_fade(es8311_handle_t dev, es8311_fade_t fade)
{
    if (fade > ES8311_FADE_65536LRCK) {
        return ESP_ERR_INVALID_ARG;
    }
    return es8311_write_reg(dev, ES8311_ADC_REG15, (uint8_t)fade);
}

es8311_handle_t es8311_create(unsigned int port, uint16_t dev_addr)
{
    es8311_dev_t *dev = (es8311_dev_t *)calloc(1, sizeof(es8311_dev_t));
    if (dev == NULL) {
        return NULL;
    }
    dev->port = port;
    dev->dev_addr = dev_addr;
    return dev;
}

void es8311_delete(es8311_handle_t dev)
{
    free(dev);
}
