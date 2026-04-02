#include "core/screen_manager.h"

#ifdef SCREEN_SETTINGS

#include "config/ota_config.h"
#include "config/pin_config.h"
#include "modules/ble_manager.h"
#include "modules/ota_module.h"
#include "modules/wifi_manager.h"
#include "screens/screen_callbacks.h"
#include "state/settings_state.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <lvgl.h>

extern lv_obj_t *screenRoots[];
extern Preferences preferences;
extern bool autoCycleEnabled;
extern uint32_t lastAutoCycleAtMs;
extern uint32_t nextWifiRetryAtMs;
extern ConnectivityState connectivityState;
void applyConfiguredTimezone();
void noteActivity();
void setOfflineMode(const char *reason);
void setDisplayBrightness(uint8_t brightness);

namespace
{
const ScreenModule kModule = {
    ScreenId::Settings,
    "Settings",
    waveformBuildSettingsScreen,
    waveformRefreshSettingsScreen,
    waveformEnterSettingsScreen,
    waveformLeaveSettingsScreen,
    waveformTickSettingsScreen,
    waveformSettingsScreenRoot,
    waveformDestroySettingsScreen,
};

struct SettingsUi
{
  lv_obj_t *screen = nullptr;
  lv_obj_t *brightnessSlider = nullptr;
  lv_obj_t *brightnessValueLabel = nullptr;
  lv_obj_t *wifiToggle = nullptr;
  lv_obj_t *bleToggle = nullptr;
  lv_obj_t *clockToggle = nullptr;
  lv_obj_t *unitToggle = nullptr;
  lv_obj_t *unitRowLabel = nullptr;
  lv_obj_t *autoCycleToggle = nullptr;
  lv_obj_t *cycleIntervalLabel = nullptr;
  lv_obj_t *sleepValueLabel = nullptr;
  lv_obj_t *faceDownToggle = nullptr;
  lv_obj_t *faceDownRowLabel = nullptr;
  lv_obj_t *timezoneValueLabel = nullptr;
  lv_obj_t *otaStatusLabel = nullptr;
};

SettingsUi gUi;
bool gBuilt = false;

// ─── Palette ───────────────────────────────────────────────────────────────
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b)
{
  return lv_color_make(r, g, b);
}

// ─── Sleep cycle values ────────────────────────────────────────────────────
constexpr uint32_t kSleepOptions[] = {
    30 * 1000,
    60 * 1000,
    2 * 60 * 1000,
    5 * 60 * 1000,
    10 * 60 * 1000,
    0,  // never
};
constexpr size_t kSleepOptionCount = sizeof(kSleepOptions) / sizeof(kSleepOptions[0]);

const char *sleepLabel(uint32_t ms)
{
  if (ms == 0) return "Never";
  if (ms < 60000) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%lu sec", ms / 1000);
    return buf;
  }
  static char buf[16];
  snprintf(buf, sizeof(buf), "%lu min", ms / 60000);
  return buf;
}

// ─── Auto-cycle interval options ───────────────────────────────────────────
constexpr uint32_t kCycleOptions[] = {3000, 5000, 10000, 15000, 30000, 60000};
constexpr size_t kCycleOptionCount = sizeof(kCycleOptions) / sizeof(kCycleOptions[0]);

const char *cycleLabel(uint32_t ms)
{
  static char buf[16];
  if (ms < 1000) {
    snprintf(buf, sizeof(buf), "%lums", ms);
  } else {
    snprintf(buf, sizeof(buf), "%lus", ms / 1000);
  }
  return buf;
}

// ─── Helpers to save & apply ───────────────────────────────────────────────
void applyBrightness()
{
  setDisplayBrightness(settingsState().brightness);
}

void applyAutoCycle()
{
  autoCycleEnabled = settingsState().autoCycleEnabled;
  if (autoCycleEnabled) {
    lastAutoCycleAtMs = millis();
  }
}

void saveAll()
{
  settingsSave(preferences);
}

// ─── Row builder helpers ───────────────────────────────────────────────────
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

// Creates a simple scroll container inside the root screen
lv_obj_t *buildScrollContainer(lv_obj_t *parent)
{
  lv_obj_t *container = lv_obj_create(parent);
  lv_obj_set_size(container, LCD_WIDTH, LCD_HEIGHT - 84);
  lv_obj_set_pos(container, 0, 84);
  lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(container, 0, 0);
  lv_obj_set_style_pad_all(container, 0, 0);
  lv_obj_set_style_pad_row(container, 0, 0);
  lv_obj_set_scroll_dir(container, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
  return container;
}

// Creates a labeled row with optional right-side widget slot
// Returns the row object; sets *rightSlot to the right-hand container
lv_obj_t *buildRow(lv_obj_t *parent, const char *label, lv_obj_t **rightSlot)
{
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, LCD_WIDTH, 56);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_border_color(row, lvColor(30, 40, 54), 0);
  lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *nameLabel = lv_label_create(row);
  lv_obj_set_style_text_font(nameLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nameLabel, lvColor(188, 198, 214), 0);
  lv_label_set_text(nameLabel, label);
  lv_obj_align(nameLabel, LV_ALIGN_LEFT_MID, 20, 0);

  if (rightSlot) {
    *rightSlot = row;  // caller places widget on right side
  }
  return row;
}

// ─── Event callbacks ────────────────────────────────────────────────────────

void onBrightnessChanged(lv_event_t *e)
{
  lv_obj_t *slider = static_cast<lv_obj_t *>(lv_event_get_target(e));
  int32_t val = lv_slider_get_value(slider);
  settingsState().brightness = static_cast<uint8_t>(val);
  applyBrightness();
  noteActivity();

  if (gUi.brightnessValueLabel) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", (int)(val * 100 / 255));
    lv_label_set_text(gUi.brightnessValueLabel, buf);
  }
}

void onBrightnessReleased(lv_event_t *e)
{
  (void)e;
  saveAll();
}

void onWifiToggle(lv_event_t *e)
{
  lv_obj_t *sw = static_cast<lv_obj_t *>(lv_event_get_target(e));
  settingsState().wifiEnabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
  if (!settingsState().wifiEnabled) {
    setOfflineMode("Wi-Fi disabled in settings");
    WiFi.mode(WIFI_OFF);
    wifiManagerClearScanResults();
  } else {
    nextWifiRetryAtMs = millis();
    wifiManagerRequestScan();
  }
  saveAll();
  noteActivity();
}

void onBleToggle(lv_event_t *e)
{
  lv_obj_t *sw = static_cast<lv_obj_t *>(lv_event_get_target(e));
  settingsState().bleEnabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
  bleManagerSetEnabled(settingsState().bleEnabled);
  if (settingsState().bleEnabled) {
    bleManagerRequestScan();
  } else {
    bleManagerClearResults();
  }
  saveAll();
  noteActivity();
}

void onClockToggle(lv_event_t *e)
{
  lv_obj_t *sw = static_cast<lv_obj_t *>(lv_event_get_target(e));
  settingsState().use24hClock = lv_obj_has_state(sw, LV_STATE_CHECKED);
  saveAll();
  noteActivity();
}

void onUnitToggle(lv_event_t *e)
{
  lv_obj_t *sw = static_cast<lv_obj_t *>(lv_event_get_target(e));
  settingsState().useCelsius = lv_obj_has_state(sw, LV_STATE_CHECKED);
  if (gUi.unitRowLabel) {
    lv_label_set_text(gUi.unitRowLabel, settingsState().useCelsius ? "Celsius" : "Fahrenheit");
  }
  saveAll();
  noteActivity();
}

void onAutoCycleToggle(lv_event_t *e)
{
  lv_obj_t *sw = static_cast<lv_obj_t *>(lv_event_get_target(e));
  settingsState().autoCycleEnabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
  applyAutoCycle();
  saveAll();
  noteActivity();
}

void onFaceDownToggle(lv_event_t *e)
{
  lv_obj_t *sw = static_cast<lv_obj_t *>(lv_event_get_target(e));
  settingsState().faceDownBlackout = lv_obj_has_state(sw, LV_STATE_CHECKED);
  if (gUi.faceDownRowLabel) {
    lv_label_set_text(gUi.faceDownRowLabel, settingsState().faceDownBlackout ? "Face-down: on" : "Face-down: off");
  }
  saveAll();
  noteActivity();
}

void onCycleIntervalTap(lv_event_t *e)
{
  (void)e;
  uint32_t cur = settingsState().autoCycleIntervalMs;
  size_t next = 0;
  for (size_t i = 0; i < kCycleOptionCount; ++i) {
    if (kCycleOptions[i] == cur) {
      next = (i + 1) % kCycleOptionCount;
      break;
    }
  }
  settingsState().autoCycleIntervalMs = kCycleOptions[next];
  if (gUi.cycleIntervalLabel) {
    lv_label_set_text(gUi.cycleIntervalLabel, cycleLabel(settingsState().autoCycleIntervalMs));
  }
  saveAll();
  noteActivity();
}

void onSleepTap(lv_event_t *e)
{
  (void)e;
  uint32_t cur = settingsState().sleepAfterMs;
  size_t next = 0;
  for (size_t i = 0; i < kSleepOptionCount; ++i) {
    if (kSleepOptions[i] == cur) {
      next = (i + 1) % kSleepOptionCount;
      break;
    }
  }
  settingsState().sleepAfterMs = kSleepOptions[next];
  if (gUi.sleepValueLabel) {
    lv_label_set_text(gUi.sleepValueLabel, sleepLabel(settingsState().sleepAfterMs));
  }
  saveAll();
  noteActivity();
}

void onTimezoneTap(lv_event_t *e)
{
  (void)e;
  timezonePickerSetInitial(settingsState().utcOffsetHours);
  showTimezonePicker();
  noteActivity();
}

void onOtaTap(lv_event_t *e)
{
  (void)e;
  if (networkIsOnline()) {
    otaModuleStart();
    if (gUi.otaStatusLabel) {
      if (otaModuleIsReady()) {
        String status = String(OTA_HOSTNAME) + ".local";
        lv_label_set_text(gUi.otaStatusLabel, status.c_str());
      } else {
        lv_label_set_text(gUi.otaStatusLabel, "OTA unavailable");
      }
    }
  } else {
    if (gUi.otaStatusLabel) {
      lv_label_set_text(gUi.otaStatusLabel, "Wi-Fi needed");
    }
  }
  noteActivity();
}

// ─── Toggle helper ──────────────────────────────────────────────────────────
lv_obj_t *buildToggle(lv_obj_t *parent, bool checked, lv_event_cb_t cb)
{
  lv_obj_t *sw = lv_switch_create(parent);
  lv_obj_set_size(sw, 52, 28);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -20, 0);

  // Style: track
  lv_obj_set_style_bg_color(sw, lvColor(30, 42, 56), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(sw, lvColor(74, 163, 255), LV_PART_MAIN | LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(sw, 0, LV_PART_MAIN);

  // Style: knob
  lv_obj_set_style_bg_color(sw, lvColor(200, 210, 224), LV_PART_KNOB | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(sw, lvColor(255, 255, 255), LV_PART_KNOB | LV_STATE_CHECKED);
  lv_obj_set_style_pad_all(sw, 2, LV_PART_KNOB);

  if (checked) {
    lv_obj_add_state(sw, LV_STATE_CHECKED);
  }
  lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, nullptr);
  return sw;
}

// ─── Value-tap button (cycles through options) ──────────────────────────────
lv_obj_t *buildCycleLabel(lv_obj_t *parent, const char *text, lv_event_cb_t cb)
{
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_set_size(btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_align(btn, LV_ALIGN_RIGHT_MID, -20, 0);
  lv_obj_set_style_bg_color(btn, lvColor(20, 30, 44), 0);
  lv_obj_set_style_bg_color(btn, lvColor(40, 60, 88), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(btn, 8, 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, lvColor(60, 80, 108), 0);
  lv_obj_set_style_pad_hor(btn, 12, 0);
  lv_obj_set_style_pad_ver(btn, 6, 0);

  lv_obj_t *label = lv_label_create(btn);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label, lvColor(148, 196, 255), 0);
  lv_label_set_text(label, text);

  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  return label;
}

// ─── OTA button ─────────────────────────────────────────────────────────────
lv_obj_t *buildActionButton(lv_obj_t *parent,
                             const char *text,
                             lv_color_t accent,
                             lv_event_cb_t cb,
                             lv_obj_t **statusLabelOut)
{
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_set_size(btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_align(btn, LV_ALIGN_RIGHT_MID, -20, 0);
  lv_obj_set_style_bg_color(btn, lvColor(20, 30, 44), 0);
  lv_obj_set_style_bg_color(btn, accent, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(btn, 8, 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, accent, 0);
  lv_obj_set_style_pad_hor(btn, 12, 0);
  lv_obj_set_style_pad_ver(btn, 6, 0);

  lv_obj_t *label = lv_label_create(btn);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label, accent, 0);
  lv_label_set_text(label, text);

  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

  if (statusLabelOut) {
    *statusLabelOut = nullptr;  // no inline status in button approach
  }
  return label;
}

// ─── Brightness row ────────────────────────────────────────────────────────
void buildBrightnessRow(lv_obj_t *container)
{
  lv_obj_t *row = lv_obj_create(container);
  lv_obj_set_size(row, LCD_WIDTH, 70);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_border_color(row, lvColor(30, 40, 54), 0);
  lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *nameLabel = lv_label_create(row);
  lv_obj_set_style_text_font(nameLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nameLabel, lvColor(188, 198, 214), 0);
  lv_label_set_text(nameLabel, "Brightness");
  lv_obj_set_pos(nameLabel, 20, 10);

  gUi.brightnessValueLabel = lv_label_create(row);
  lv_obj_set_style_text_font(gUi.brightnessValueLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(gUi.brightnessValueLabel, lvColor(100, 130, 170), 0);
  char pctBuf[8];
  snprintf(pctBuf, sizeof(pctBuf), "%d%%", (int)(settingsState().brightness * 100 / 255));
  lv_label_set_text(gUi.brightnessValueLabel, pctBuf);
  lv_obj_set_pos(gUi.brightnessValueLabel, LCD_WIDTH - 52, 10);

  gUi.brightnessSlider = lv_slider_create(row);
  lv_obj_set_size(gUi.brightnessSlider, LCD_WIDTH - 40, 6);
  lv_obj_set_pos(gUi.brightnessSlider, 20, 44);
  lv_slider_set_range(gUi.brightnessSlider, 10, 255);
  lv_slider_set_value(gUi.brightnessSlider, settingsState().brightness, LV_ANIM_OFF);

  lv_obj_set_style_bg_color(gUi.brightnessSlider, lvColor(30, 42, 56), LV_PART_MAIN);
  lv_obj_set_style_bg_color(gUi.brightnessSlider, lvColor(255, 214, 100), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(gUi.brightnessSlider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(gUi.brightnessSlider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(gUi.brightnessSlider, lvColor(255, 255, 255), LV_PART_KNOB);
  lv_obj_set_style_pad_all(gUi.brightnessSlider, 6, LV_PART_KNOB);
  lv_obj_set_style_radius(gUi.brightnessSlider, 3, LV_PART_MAIN);
  lv_obj_set_style_radius(gUi.brightnessSlider, 3, LV_PART_INDICATOR);

  lv_obj_add_event_cb(gUi.brightnessSlider, onBrightnessChanged, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(gUi.brightnessSlider, onBrightnessReleased, LV_EVENT_RELEASED, nullptr);
}

// ─── Screen build ──────────────────────────────────────────────────────────
void buildSettingsScreen()
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);

  // Header
  lv_obj_t *eyebrow = lv_label_create(screen);
  lv_obj_set_style_text_font(eyebrow, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(eyebrow, lvColor(110, 126, 148), 0);
  lv_label_set_text(eyebrow, "WAVEFORM");
  lv_obj_align(eyebrow, LV_ALIGN_TOP_LEFT, 20, 18);

  lv_obj_t *headline = lv_label_create(screen);
  lv_obj_set_style_text_font(headline, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(headline, lvColor(244, 247, 252), 0);
  lv_label_set_text(headline, "Settings");
  lv_obj_align(headline, LV_ALIGN_TOP_LEFT, 20, 40);

  // Scroll container
  lv_obj_t *cont = buildScrollContainer(screen);

  // Brightness
  buildBrightnessRow(cont);

  // Wi-Fi
  {
    lv_obj_t *row = buildRow(cont, "Wi-Fi", nullptr);
    gUi.wifiToggle = buildToggle(row, settingsState().wifiEnabled, onWifiToggle);
  }

  // BLE
  {
    lv_obj_t *row = buildRow(cont, "Bluetooth LE", nullptr);
    gUi.bleToggle = buildToggle(row, settingsState().bleEnabled, onBleToggle);
  }

  // Clock format
  {
    lv_obj_t *row = buildRow(cont, "24-hour clock", nullptr);
    gUi.clockToggle = buildToggle(row, settingsState().use24hClock, onClockToggle);
  }

  // Temperature unit
  {
    lv_obj_t *row = buildRow(cont, settingsState().useCelsius ? "Celsius" : "Fahrenheit", nullptr);
    gUi.unitRowLabel = lv_obj_get_child(row, 0);
    gUi.unitToggle = buildToggle(row, settingsState().useCelsius, onUnitToggle);
  }

  // Auto-cycle
  {
    lv_obj_t *row = buildRow(cont, "Auto-cycle", nullptr);
    gUi.autoCycleToggle = buildToggle(row, settingsState().autoCycleEnabled, onAutoCycleToggle);
  }

  // Cycle interval
  {
    lv_obj_t *row = buildRow(cont, "Cycle interval", nullptr);
    gUi.cycleIntervalLabel = buildCycleLabel(row, cycleLabel(settingsState().autoCycleIntervalMs), onCycleIntervalTap);
  }

  // Sleep timeout
  {
    lv_obj_t *row = buildRow(cont, "Sleep after", nullptr);
    gUi.sleepValueLabel = buildCycleLabel(row, sleepLabel(settingsState().sleepAfterMs), onSleepTap);
  }

  // Face-down blackout
  {
    lv_obj_t *row = buildRow(cont, settingsState().faceDownBlackout ? "Face-down: on" : "Face-down: off", nullptr);
    gUi.faceDownRowLabel = lv_obj_get_child(row, 0);
    gUi.faceDownToggle = buildToggle(row, settingsState().faceDownBlackout, onFaceDownToggle);
  }

  // Timezone
  {
    lv_obj_t *row = buildRow(cont, "Timezone", nullptr);
    // Show current offset as tappable label
    char tzBuf[16];
    int off = settingsState().utcOffsetHours;
    snprintf(tzBuf, sizeof(tzBuf), "UTC%+d", off);
    gUi.timezoneValueLabel = buildCycleLabel(row, tzBuf, onTimezoneTap);
  }

  // OTA update
  {
    lv_obj_t *row = buildRow(cont, "OTA update", nullptr);
    buildActionButton(row, "Check now", lvColor(64, 214, 146), onOtaTap, &gUi.otaStatusLabel);
    // Inline status next to button
    gUi.otaStatusLabel = lv_label_create(row);
    lv_obj_set_style_text_font(gUi.otaStatusLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(gUi.otaStatusLabel, lvColor(100, 140, 100), 0);
    lv_label_set_text(gUi.otaStatusLabel, "");
    lv_obj_align_to(gUi.otaStatusLabel, row, LV_ALIGN_LEFT_MID, LCD_WIDTH / 2, 0);
  }

  gUi.screen = screen;
  screenRoots[static_cast<size_t>(ScreenId::Settings)] = screen;
  gBuilt = true;
}
} // namespace

const ScreenModule &settingsScreenModule()
{
  return kModule;
}

lv_obj_t *waveformSettingsScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Settings)];
}

bool waveformBuildSettingsScreen()
{
  if (!waveformSettingsScreenRoot()) {
    buildSettingsScreen();
  }
  return gBuilt && waveformSettingsScreenRoot();
}

bool waveformRefreshSettingsScreen()
{
  return gBuilt;
}

void waveformEnterSettingsScreen()
{
  // Sync toggle states in case settings changed externally (e.g. auto-cycle via side key)
  if (gBuilt) {
    SettingsState &s = settingsState();
    s.autoCycleEnabled = autoCycleEnabled;
    if (gUi.autoCycleToggle) {
      if (s.autoCycleEnabled) {
        lv_obj_add_state(gUi.autoCycleToggle, LV_STATE_CHECKED);
      } else {
        lv_obj_clear_state(gUi.autoCycleToggle, LV_STATE_CHECKED);
      }
    }
    if (gUi.timezoneValueLabel) {
      char tzBuf[16];
      snprintf(tzBuf, sizeof(tzBuf), "UTC%+d", s.utcOffsetHours);
      lv_label_set_text(gUi.timezoneValueLabel, tzBuf);
    }
    if (gUi.otaStatusLabel) {
      if (otaModuleIsReady()) {
        String status = String(OTA_HOSTNAME) + ".local";
        lv_label_set_text(gUi.otaStatusLabel, status.c_str());
      } else if (networkIsOnline()) {
        lv_label_set_text(gUi.otaStatusLabel, "Wi-Fi online");
      } else {
        lv_label_set_text(gUi.otaStatusLabel, "Wi-Fi needed");
      }
    }
  }
}

void waveformLeaveSettingsScreen()
{
}

void waveformTickSettingsScreen(uint32_t nowMs)
{
  (void)nowMs;
}

void waveformDestroySettingsScreen()
{
  if (gUi.screen) {
    lv_obj_delete(gUi.screen);
  }
  screenRoots[static_cast<size_t>(ScreenId::Settings)] = nullptr;
  gUi = SettingsUi{};
  gBuilt = false;
}

#endif // SCREEN_SETTINGS
