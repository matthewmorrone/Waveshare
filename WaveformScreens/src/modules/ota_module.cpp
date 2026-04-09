#include "modules/ota_module.h"

#include "config/ota_config.h"
#include "config/pin_config.h"
#include "modules/wifi_manager.h"

#include <ArduinoOTA.h>
#include <WiFi.h>
#include <lvgl.h>

extern lv_display_t *display;

namespace
{
lv_obj_t *otaOverlay = nullptr;
lv_obj_t *otaStatusLabel = nullptr;
lv_obj_t *otaPercentLabel = nullptr;
lv_obj_t *otaFooterLabel = nullptr;
lv_obj_t *otaBar = nullptr;

bool otaReady = false;
bool otaInProgress = false;
bool otaOverlaySticky = false;
uint32_t otaOverlayHideAtMs = 0;
IPAddress otaReadyIp;
} // namespace

void otaModuleBuildOverlay()
{
  otaOverlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(otaOverlay, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_style_bg_color(otaOverlay, lv_color_make(0, 0, 0), 0);
  lv_obj_set_style_bg_opa(otaOverlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(otaOverlay, 0, 0);
  lv_obj_clear_flag(otaOverlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *card = lv_obj_create(otaOverlay);
  lv_obj_set_size(card, 280, 212);
  lv_obj_center(card);
  lv_obj_set_style_radius(card, 24, 0);
  lv_obj_set_style_bg_color(card, lv_color_make(8, 12, 18), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lv_color_make(34, 60, 96), 0);
  lv_obj_set_style_pad_all(card, 18, 0);

  lv_obj_t *title = lv_label_create(card);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_label_set_text(title, "Firmware Update");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

  otaStatusLabel = lv_label_create(card);
  lv_obj_set_width(otaStatusLabel, 244);
  lv_obj_set_style_text_font(otaStatusLabel, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_align(otaStatusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(otaStatusLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(otaStatusLabel, "Ready");
  lv_obj_align(otaStatusLabel, LV_ALIGN_TOP_MID, 0, 48);

  otaPercentLabel = lv_label_create(card);
  lv_obj_set_style_text_font(otaPercentLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(otaPercentLabel, lv_color_make(255, 255, 255), 0);
  lv_label_set_text(otaPercentLabel, "0%");
  lv_obj_align(otaPercentLabel, LV_ALIGN_CENTER, 0, -8);

  otaBar = lv_bar_create(card);
  lv_obj_set_size(otaBar, 224, 10);
  lv_obj_align(otaBar, LV_ALIGN_CENTER, 0, 44);
  lv_bar_set_range(otaBar, 0, 100);
  lv_bar_set_value(otaBar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(otaBar, lv_color_make(28, 34, 40), 0);
  lv_obj_set_style_bg_color(otaBar, lv_color_make(52, 132, 255), LV_PART_INDICATOR);
  lv_obj_set_style_radius(otaBar, LV_RADIUS_CIRCLE, 0);

  otaFooterLabel = lv_label_create(card);
  lv_obj_set_width(otaFooterLabel, 244);
  lv_obj_set_style_text_font(otaFooterLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(otaFooterLabel, lv_color_make(168, 178, 194), 0);
  lv_obj_set_style_text_align(otaFooterLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(otaFooterLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(otaFooterLabel, "do not power off");
  lv_obj_align(otaFooterLabel, LV_ALIGN_BOTTOM_MID, 0, -6);

  otaModuleHideOverlay();
}

void otaModuleStart()
{
  if (!networkIsOnline()) {
    return;
  }

  IPAddress currentIp = WiFi.localIP();
  if (otaReady && otaReadyIp == currentIp) {
    return;
  }

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  if (strlen(OTA_PASSWORD) > 0) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }

  ArduinoOTA
      .onStart([]() {
        Serial.println("OTA start");
        otaInProgress = true;
        otaOverlaySticky = false;
        otaOverlayHideAtMs = 0;
        otaModuleUpdateOverlay("Receiving firmware", "do not power off", 0);
      })
      .onEnd([]() {
        Serial.println("\nOTA complete");
        otaModuleUpdateOverlay("Finishing update", "restarting", 100);
        delay(350);
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        int percent = static_cast<int>((progress * 100U) / total);
        otaModuleUpdateOverlay("Receiving firmware", "do not power off", percent);
      })
      .onError([](ota_error_t error) {
        Serial.printf("OTA error[%u]\n", error);
        otaInProgress = false;

        String status = "Update failed";
        switch (error) {
          case OTA_AUTH_ERROR:
            status = "Auth failed";
            break;
          case OTA_BEGIN_ERROR:
            status = "Begin failed";
            break;
          case OTA_CONNECT_ERROR:
            status = "Connect failed";
            break;
          case OTA_RECEIVE_ERROR:
            status = "Receive failed";
            break;
          case OTA_END_ERROR:
            status = "Finalize failed";
            break;
        }
        otaModuleUpdateOverlay(status, "returning to app", 0);
        otaModuleScheduleHide(1600);
      });

  ArduinoOTA.begin();
  otaReady = true;
  otaReadyIp = currentIp;
  Serial.printf("OTA ready at %s.local (%s)\n", OTA_HOSTNAME, currentIp.toString().c_str());
}

void otaModuleReset()
{
  otaReady = false;
  otaReadyIp = IPAddress();
  if (!otaInProgress) {
    otaModuleHideOverlay();
  }
}

void otaModuleHandle()
{
  if (otaReady && networkIsOnline()) {
    ArduinoOTA.handle();
  }
}

void otaModuleUpdateOverlay(const String &status, const String &footer, int percent)
{
  if (!otaOverlay) {
    return;
  }

  lv_obj_clear_flag(otaOverlay, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(otaStatusLabel, status.c_str());
  lv_label_set_text(otaFooterLabel, footer.c_str());
  String percentText = String(percent) + "%";
  lv_label_set_text(otaPercentLabel, percentText.c_str());
  lv_bar_set_value(otaBar, percent, LV_ANIM_OFF);
  if (display) {
    lv_timer_handler();
  }
}

void otaModuleHideOverlay()
{
  if (!otaOverlay) {
    return;
  }

  lv_obj_add_flag(otaOverlay, LV_OBJ_FLAG_HIDDEN);
  otaOverlaySticky = false;
  otaOverlayHideAtMs = 0;
}

void otaModuleScheduleHide(uint32_t delayMs)
{
  otaOverlaySticky = true;
  otaOverlayHideAtMs = millis() + delayMs;
}

bool otaModuleIsInProgress()
{
  return otaInProgress;
}

bool otaModuleIsReady()
{
  return otaReady && networkIsOnline() && otaReadyIp == WiFi.localIP();
}

void otaModuleUpdate()
{
  if (!otaInProgress && otaOverlaySticky && static_cast<int32_t>(millis() - otaOverlayHideAtMs) >= 0) {
    otaModuleHideOverlay();
  }
}
