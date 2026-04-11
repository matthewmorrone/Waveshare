#include "screen_manager.h"

#ifdef SCREEN_RADIO

#include "pin_config.h"
#include "ble_manager.h"
#include "wifi_manager.h"
#include "screen_callbacks.h"
#include "settings_state.h"

#include <WiFi.h>
#include <lvgl.h>

extern lv_obj_t *screenRoots[];
extern ConnectivityState connectivityState;
void noteActivity();

namespace
{
constexpr size_t kMaxUiWifiEntries = 5;
constexpr size_t kMaxUiBleEntries = 4;

struct RadioUi
{
  lv_obj_t *screen = nullptr;
  lv_obj_t *wifiStatusValue = nullptr;
  lv_obj_t *wifiMetaValue = nullptr;
  lv_obj_t *bleStatusValue = nullptr;
  lv_obj_t *bleMetaValue = nullptr;
  lv_obj_t *wifiListValue = nullptr;
  lv_obj_t *bleListValue = nullptr;
  lv_obj_t *wifiButtonLabel = nullptr;
  lv_obj_t *bleButtonLabel = nullptr;
};

const ScreenModule kModule = {
    ScreenId::Radio,
    "Radio",
    waveformBuildRadioScreen,
    waveformRefreshRadioScreen,
    waveformEnterRadioScreen,
    waveformLeaveRadioScreen,
    waveformTickRadioScreen,
    waveformRadioScreenRoot,
    waveformDestroyRadioScreen,
};

RadioUi gUi;
bool gBuilt = false;
bool gRequestWifiScanOnEnter = false;
bool gRequestBleScanOnEnter = false;

lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b)
{
  return lv_color_make(r, g, b);
}

void applyRootStyle(lv_obj_t *obj)
{
  lv_obj_set_size(obj, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_style_bg_color(obj, lvColor(5, 8, 16), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_outline_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(obj, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
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

String ageLabel(uint32_t nowMs, uint32_t thenMs)
{
  if (thenMs == 0) {
    return String("never");
  }

  uint32_t elapsedMs = nowMs - thenMs;
  if (elapsedMs < 1000) {
    return String("just now");
  }

  uint32_t elapsedSeconds = elapsedMs / 1000;
  if (elapsedSeconds < 60) {
    return String(elapsedSeconds) + "s ago";
  }

  uint32_t elapsedMinutes = elapsedSeconds / 60;
  return String(elapsedMinutes) + "m ago";
}

lv_obj_t *createCard(lv_obj_t *parent,
                     const char *title,
                     lv_coord_t x,
                     lv_coord_t y,
                     lv_coord_t width,
                     lv_coord_t height,
                     lv_color_t accent,
                     lv_obj_t **primaryOut,
                     lv_obj_t **secondaryOut)
{
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_size(card, width, height);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_style_bg_color(card, lvColor(13, 19, 30), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 18, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, accent, 0);
  lv_obj_set_style_pad_all(card, 0, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *titleLabel = lv_label_create(card);
  lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(titleLabel, accent, 0);
  lv_label_set_text(titleLabel, title);
  lv_obj_align(titleLabel, LV_ALIGN_TOP_LEFT, 14, 12);

  lv_obj_t *primary = lv_label_create(card);
  lv_obj_set_width(primary, width - 28);
  lv_obj_set_style_text_font(primary, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(primary, lvColor(244, 247, 252), 0);
  lv_label_set_long_mode(primary, LV_LABEL_LONG_WRAP);
  lv_obj_align(primary, LV_ALIGN_TOP_LEFT, 14, 34);

  lv_obj_t *secondary = lv_label_create(card);
  lv_obj_set_width(secondary, width - 28);
  lv_obj_set_style_text_font(secondary, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(secondary, lvColor(148, 161, 181), 0);
  lv_label_set_long_mode(secondary, LV_LABEL_LONG_WRAP);
  lv_obj_align(secondary, LV_ALIGN_BOTTOM_LEFT, 14, -12);

  if (primaryOut) {
    *primaryOut = primary;
  }
  if (secondaryOut) {
    *secondaryOut = secondary;
  }
  return card;
}

lv_obj_t *createScanButton(lv_obj_t *parent,
                           const char *text,
                           lv_coord_t x,
                           lv_coord_t y,
                           lv_color_t accent,
                           lv_event_cb_t cb,
                           lv_obj_t **labelOut)
{
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_set_size(button, 156, 42);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_style_bg_color(button, lvColor(16, 26, 40), 0);
  lv_obj_set_style_bg_color(button, accent, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(button, 14, 0);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(button, accent, 0);
  lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *label = lv_label_create(button);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(label, accent, 0);
  lv_label_set_text(label, text);
  lv_obj_center(label);

  if (labelOut) {
    *labelOut = label;
  }
  return button;
}

void onWifiScanTap(lv_event_t *event)
{
  (void)event;
  wifiManagerRequestScan();
  noteActivity();
}

void onBleScanTap(lv_event_t *event)
{
  (void)event;
  bleManagerRequestScan();
  noteActivity();
}

void updateWifiSummary(uint32_t nowMs)
{
  if (!gUi.wifiStatusValue || !gUi.wifiMetaValue || !gUi.wifiListValue || !gUi.wifiButtonLabel) {
    return;
  }

  if (!settingsState().wifiEnabled) {
    lv_label_set_text(gUi.wifiStatusValue, "Disabled");
    lv_label_set_text(gUi.wifiMetaValue, "Enable Wi-Fi in Settings to scan and sync.");
    lv_label_set_text(gUi.wifiListValue, "Wi-Fi scanning is paused.");
    lv_label_set_text(gUi.wifiButtonLabel, "Wi-Fi off");
    return;
  }

  if (wifiManagerScanInProgress()) {
    lv_label_set_text(gUi.wifiButtonLabel, "Scanning...");
  } else {
    lv_label_set_text(gUi.wifiButtonLabel, "Scan Wi-Fi");
  }

  char status[96];
  if (networkIsOnline()) {
    snprintf(status,
             sizeof(status),
             "%s\n%s",
             WiFi.SSID().c_str(),
             WiFi.localIP().toString().c_str());
  } else if (connectivityState == ConnectivityState::Connecting) {
    snprintf(status, sizeof(status), "Connecting\nSaved networks");
  } else if (wifiManagerHasKnownNetworks()) {
    snprintf(status, sizeof(status), "Offline\nSaved networks ready");
  } else {
    snprintf(status, sizeof(status), "Offline\nNo saved SSIDs");
  }
  lv_label_set_text(gUi.wifiStatusValue, status);

  char meta[128];
  if (networkIsOnline()) {
    snprintf(meta,
             sizeof(meta),
             "RSSI %ld dBm  ch%ld\nScan %s",
             static_cast<long>(WiFi.RSSI()),
             static_cast<long>(WiFi.channel()),
             ageLabel(nowMs, wifiManagerLastScanAtMs()).c_str());
  } else {
    snprintf(meta,
             sizeof(meta),
             "%s\nLast scan %s",
             connectivityLabel(),
             ageLabel(nowMs, wifiManagerLastScanAtMs()).c_str());
  }
  lv_label_set_text(gUi.wifiMetaValue, meta);

  WiFiScanEntry entries[kMaxUiWifiEntries] = {};
  const size_t count = wifiManagerSnapshot(entries, kMaxUiWifiEntries);
  if (count == 0) {
    lv_label_set_text(gUi.wifiListValue, wifiManagerScanInProgress() ? "Searching for nearby Wi-Fi networks..." :
                                                                       "No Wi-Fi networks cached yet.");
    return;
  }

  char list[512];
  size_t offset = 0;
  for (size_t index = 0; index < count && offset < sizeof(list); ++index) {
    const WiFiScanEntry &entry = entries[index];
    const char *ssid = entry.hidden ? "<hidden>" : entry.ssid;
    int written = snprintf(list + offset,
                           sizeof(list) - offset,
                           "%s%s  %ld dBm  c%ld  %s\n",
                           entry.known ? "* " : "",
                           ssid,
                           static_cast<long>(entry.rssi),
                           static_cast<long>(entry.channel),
                           wifiManagerEncryptionLabel(entry.encryption));
    if (written < 0) {
      break;
    }
    offset += static_cast<size_t>(written);
  }
  lv_label_set_text(gUi.wifiListValue, list);
}

void updateBleSummary(uint32_t nowMs)
{
  if (!gUi.bleStatusValue || !gUi.bleMetaValue || !gUi.bleListValue || !gUi.bleButtonLabel) {
    return;
  }

  if (!settingsState().bleEnabled) {
    lv_label_set_text(gUi.bleStatusValue, "Disabled");
    lv_label_set_text(gUi.bleMetaValue, "Enable BLE in Settings to discover nearby devices.");
    lv_label_set_text(gUi.bleListValue, "BLE scanning is paused.");
    lv_label_set_text(gUi.bleButtonLabel, "BLE off");
    return;
  }

  if (!bleManagerAvailable()) {
    lv_label_set_text(gUi.bleStatusValue, "Unavailable");
    lv_label_set_text(gUi.bleMetaValue, "BLE stack did not initialize on this build.");
    lv_label_set_text(gUi.bleListValue, "No BLE scan data available.");
    lv_label_set_text(gUi.bleButtonLabel, "BLE unavailable");
    return;
  }

  if (bleManagerScanInProgress()) {
    lv_label_set_text(gUi.bleButtonLabel, "Scanning...");
  } else {
    lv_label_set_text(gUi.bleButtonLabel, "Scan BLE");
  }

  const size_t deviceCount = bleManagerDeviceCount();
  char status[96];
  snprintf(status,
           sizeof(status),
           "%u nearby\nPassive scan",
           static_cast<unsigned>(deviceCount));
  lv_label_set_text(gUi.bleStatusValue, status);

  char meta[128];
  snprintf(meta,
           sizeof(meta),
           "%s\nLast scan %s",
           bleManagerScanInProgress() ? "Listening now" : "Ready",
           ageLabel(nowMs, bleManagerLastScanAtMs()).c_str());
  lv_label_set_text(gUi.bleMetaValue, meta);

  BleScanEntry entries[kMaxUiBleEntries] = {};
  const size_t count = bleManagerSnapshot(entries, kMaxUiBleEntries);
  if (count == 0) {
    lv_label_set_text(gUi.bleListValue, bleManagerScanInProgress() ? "Searching for nearby BLE advertisements..." :
                                                                      "No BLE devices cached yet.");
    return;
  }

  char list[512];
  size_t offset = 0;
  for (size_t index = 0; index < count && offset < sizeof(list); ++index) {
    const BleScanEntry &entry = entries[index];
    int written = snprintf(list + offset,
                           sizeof(list) - offset,
                           "%s  %d dBm  %s\n",
                           entry.name,
                           entry.rssi,
                           entry.connectable ? "connectable" : "broadcast");
    if (written < 0) {
      break;
    }
    offset += static_cast<size_t>(written);
  }
  lv_label_set_text(gUi.bleListValue, list);
}

void buildRadioScreen()
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);

  lv_obj_t *eyebrow = lv_label_create(screen);
  lv_obj_set_style_text_font(eyebrow, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(eyebrow, lvColor(96, 112, 136), 0);
  lv_label_set_text(eyebrow, "RADIO");
  lv_obj_set_pos(eyebrow, 20, 18);

  lv_obj_t *headline = lv_label_create(screen);
  lv_obj_set_style_text_font(headline, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(headline, lvColor(244, 247, 252), 0);
  lv_label_set_text(headline, "Wireless");
  lv_obj_set_pos(headline, 20, 40);

  createCard(screen, "WIFI 2.4", 20, 92, 156, 118, lvColor(74, 163, 255), &gUi.wifiStatusValue, &gUi.wifiMetaValue);
  createCard(screen, "BLE", 188, 92, 156, 118, lvColor(92, 224, 224), &gUi.bleStatusValue, &gUi.bleMetaValue);

  createScanButton(screen, "Scan Wi-Fi", 20, 224, lvColor(74, 163, 255), onWifiScanTap, &gUi.wifiButtonLabel);
  createScanButton(screen, "Scan BLE", 188, 224, lvColor(92, 224, 224), onBleScanTap, &gUi.bleButtonLabel);

  createCard(screen, "NEARBY WIFI", 20, 280, 324, 136, lvColor(74, 163, 255), &gUi.wifiListValue, nullptr);
  createCard(screen, "NEARBY BLE", 20, 430, 324, 126, lvColor(92, 224, 224), &gUi.bleListValue, nullptr);
  lv_obj_set_style_text_font(gUi.wifiListValue, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_font(gUi.bleListValue, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(gUi.wifiListValue, lvColor(230, 236, 244), 0);
  lv_obj_set_style_text_color(gUi.bleListValue, lvColor(230, 236, 244), 0);

  gUi.screen = screen;
  screenRoots[static_cast<size_t>(ScreenId::Radio)] = screen;
  gBuilt = true;
}
} // namespace

const ScreenModule &radioScreenModule()
{
  return kModule;
}

lv_obj_t *waveformRadioScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Radio)];
}

bool waveformBuildRadioScreen()
{
  if (!waveformRadioScreenRoot()) {
    buildRadioScreen();
  }

  return gBuilt && waveformRadioScreenRoot() && gUi.wifiStatusValue && gUi.bleStatusValue &&
         gUi.wifiListValue && gUi.bleListValue;
}

bool waveformRefreshRadioScreen()
{
  if (!gBuilt) {
    return false;
  }

  const uint32_t nowMs = millis();
  updateWifiSummary(nowMs);
  updateBleSummary(nowMs);
  return true;
}

void waveformEnterRadioScreen()
{
  if (gUi.screen) {
    lv_obj_scroll_to_y(gUi.screen, 0, LV_ANIM_OFF);
  }
  gRequestWifiScanOnEnter = true;
  gRequestBleScanOnEnter = true;
}

void waveformLeaveRadioScreen()
{
}

void waveformTickRadioScreen(uint32_t nowMs)
{
  (void)nowMs;
  if (gRequestWifiScanOnEnter) {
    gRequestWifiScanOnEnter = false;
    wifiManagerRequestScan();
  }
  if (gRequestBleScanOnEnter) {
    gRequestBleScanOnEnter = false;
    bleManagerRequestScan();
  }
}

void waveformDestroyRadioScreen()
{
  if (gUi.screen) {
    lv_obj_delete(gUi.screen);
  }
  screenRoots[static_cast<size_t>(ScreenId::Radio)] = nullptr;
  gUi = RadioUi{};
  gBuilt = false;
}

#endif // SCREEN_RADIO
