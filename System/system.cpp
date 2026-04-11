#include "screen_manager.h"

#ifdef SCREEN_SYSTEM

#include "pin_config.h"
#include "battery.h"
#include "ble_manager.h"
#include "storage.h"
#include "wifi_manager.h"
#include "screen_callbacks.h"
#include <WiFi.h>
#include <inttypes.h>
#include <time.h>

extern lv_obj_t *screenRoots[];
extern ConnectivityState connectivityState;

namespace
{
struct SystemUi
{
  lv_obj_t *screen = nullptr;
  lv_obj_t *powerValue = nullptr;
  lv_obj_t *networkValue = nullptr;
  lv_obj_t *storageValue = nullptr;
  lv_obj_t *timeValue = nullptr;
  lv_obj_t *firmwareValue = nullptr;
  lv_obj_t *controlsValue = nullptr;
};

const ScreenModule kModule = {
    ScreenId::System,
    "System",
    waveformBuildSystemScreen,
    waveformRefreshSystemScreen,
    waveformEnterSystemScreen,
    waveformLeaveSystemScreen,
    waveformTickSystemScreen,
    waveformSystemScreenRoot,
};

SystemUi gUi;
bool gBuilt = false;

lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b)
{
  return lv_color_make(r, g, b);
}

void applyRootStyle(lv_obj_t *obj)
{
  lv_obj_set_size(obj, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_style_bg_color(obj, lvColor(6, 10, 16), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_outline_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

const char *connectivityLabel()
{
  switch (connectivityState) {
    case ConnectivityState::Online:
      return "Online";
    case ConnectivityState::Connecting:
      return "Connecting";
    case ConnectivityState::Offline:
    default:
      return "Offline";
  }
}

String localClockText()
{
  time_t now = time(nullptr);
  if (now < 1704067200) {
    return String("Waiting for RTC or Wi-Fi");
  }

  struct tm localTime = {};
  localtime_r(&now, &localTime);
  char buffer[40];
  strftime(buffer, sizeof(buffer), "%a %b %d  %H:%M:%S", &localTime);
  return String(buffer);
}

String formatUptime()
{
  uint32_t totalSeconds = millis() / 1000U;
  uint32_t hours = totalSeconds / 3600U;
  uint32_t minutes = (totalSeconds / 60U) % 60U;
  uint32_t seconds = totalSeconds % 60U;
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu", hours, minutes, seconds);
  return String(buffer);
}

void setLabelText(lv_obj_t *label, const char *text)
{
  if (label) {
    lv_label_set_text(label, text);
  }
}

void updateSystemValues()
{
  char power[80];
  int battery = batteryPercentValue();
  if (battery >= 0) {
    snprintf(power,
             sizeof(power),
             "%d%%%s%s",
             battery,
             batteryIsCharging() ? " charging" : "",
             usbIsConnected() ? "\nUSB power present" : "");
  } else {
    snprintf(power, sizeof(power), "Battery unavailable");
  }
  setLabelText(gUi.powerValue, power);

  char network[120];
  if (networkIsOnline()) {
    snprintf(network,
             sizeof(network),
             "%s\n%s\nBLE %u seen",
             WiFi.SSID().c_str(),
             WiFi.localIP().toString().c_str(),
             static_cast<unsigned>(bleManagerDeviceCount()));
  } else {
    snprintf(network,
             sizeof(network),
             "%s\nWi-Fi sync + OTA\nBLE %u seen",
             connectivityLabel(),
             static_cast<unsigned>(bleManagerDeviceCount()));
  }
  setLabelText(gUi.networkValue, network);

  char storage[128];
  if (storageIsMounted()) {
    snprintf(storage,
             sizeof(storage),
             "%s  %" PRIu64 " MB\n/assets /config /recordings",
             sdCardTypeLabel(storageCardType()),
             storageCardSizeMb());
  } else {
    snprintf(storage, sizeof(storage), "No card\nInsert SD for audio and assets");
  }
  setLabelText(gUi.storageValue, storage);

  String timeText = localClockText() + "\nUp " + formatUptime();
  setLabelText(gUi.timeValue, timeText.c_str());

  char firmware[48];
  snprintf(firmware, sizeof(firmware), "%s\n%s", __DATE__, __TIME__);
  setLabelText(gUi.firmwareValue, firmware);

  setLabelText(gUi.controlsValue, "Swipe up/down: next app\nSide key: next / cycle\nWatch: tap date");
}

lv_obj_t *createInfoCard(lv_obj_t *parent,
                         const char *title,
                         lv_coord_t x,
                         lv_coord_t y,
                         lv_color_t accent,
                         lv_obj_t **valueOut)
{
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_size(card, 160, 92);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_style_bg_color(card, lvColor(14, 20, 30), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 18, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, accent, 0);
  lv_obj_set_style_shadow_width(card, 16, 0);
  lv_obj_set_style_shadow_color(card, accent, 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
  lv_obj_set_style_pad_all(card, 0, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *titleLabel = lv_label_create(card);
  lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(titleLabel, accent, 0);
  lv_label_set_text(titleLabel, title);
  lv_obj_align(titleLabel, LV_ALIGN_TOP_LEFT, 14, 12);

  lv_obj_t *valueLabel = lv_label_create(card);
  lv_obj_set_width(valueLabel, 140);
  lv_obj_set_style_text_font(valueLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(valueLabel, lvColor(238, 242, 247), 0);
  lv_label_set_long_mode(valueLabel, LV_LABEL_LONG_WRAP);
  lv_obj_align(valueLabel, LV_ALIGN_TOP_LEFT, 14, 34);

  *valueOut = valueLabel;
  return card;
}

void buildSystemScreen()
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);

  lv_obj_t *eyebrow = lv_label_create(screen);
  lv_obj_set_style_text_font(eyebrow, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(eyebrow, lvColor(110, 126, 148), 0);
  lv_label_set_text(eyebrow, "SYSTEM");
  lv_obj_align(eyebrow, LV_ALIGN_TOP_LEFT, 20, 18);

  lv_obj_t *headline = lv_label_create(screen);
  lv_obj_set_style_text_font(headline, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(headline, lvColor(244, 247, 252), 0);
  lv_label_set_text(headline, "Device Status");
  lv_obj_align(headline, LV_ALIGN_TOP_LEFT, 20, 40);

  createInfoCard(screen, "POWER", 20, 86, lvColor(64, 214, 146), &gUi.powerValue);
  createInfoCard(screen, "NETWORK", 188, 86, lvColor(74, 163, 255), &gUi.networkValue);
  createInfoCard(screen, "STORAGE", 20, 188, lvColor(255, 196, 74), &gUi.storageValue);
  createInfoCard(screen, "TIME", 188, 188, lvColor(182, 138, 255), &gUi.timeValue);
  createInfoCard(screen, "FIRMWARE", 20, 290, lvColor(255, 111, 145), &gUi.firmwareValue);
  createInfoCard(screen, "CONTROLS", 188, 290, lvColor(92, 224, 224), &gUi.controlsValue);

  gUi.screen = screen;
  screenRoots[static_cast<size_t>(ScreenId::System)] = screen;
  gBuilt = true;
}
} // namespace

const ScreenModule &systemScreenModule()
{
  return kModule;
}

lv_obj_t *waveformSystemScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::System)];
}

bool waveformBuildSystemScreen()
{
  if (!waveformSystemScreenRoot()) {
    buildSystemScreen();
  }
  return gBuilt && waveformSystemScreenRoot() && gUi.powerValue && gUi.networkValue && gUi.storageValue &&
         gUi.timeValue && gUi.firmwareValue && gUi.controlsValue;
}

bool waveformRefreshSystemScreen()
{
  if (!gBuilt) {
    return false;
  }
  updateSystemValues();
  return true;
}

void waveformEnterSystemScreen()
{
  updateSystemValues();
}

void waveformLeaveSystemScreen()
{
}

void waveformTickSystemScreen(uint32_t nowMs)
{
  (void)nowMs;
}

#endif
