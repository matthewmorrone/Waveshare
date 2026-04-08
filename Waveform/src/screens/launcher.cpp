#include "core/screen_manager.h"

#ifdef SCREEN_LAUNCHER

#include "config/pin_config.h"
#include "modules/battery.h"
#include "modules/ble_manager.h"
#include "modules/storage.h"
#include "modules/wifi_manager.h"
#include "screens/screen_callbacks.h"

#include <WiFi.h>
#include <cmath>
#include <inttypes.h>

extern lv_obj_t *screenRoots[];
extern ConnectivityState connectivityState;
void noteActivity();

namespace
{
struct LauncherApp
{
  ScreenId id;
  const char *title;
  const char *subtitle;
  const char *badge;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

struct LauncherUi
{
  lv_obj_t *screen = nullptr;
  lv_obj_t *statusLabel = nullptr;
  lv_obj_t *storageLabel = nullptr;
};

const ScreenModule kModule = {
    ScreenId::Launcher,
    "Launcher",
    waveformBuildLauncherScreen,
    waveformRefreshLauncherScreen,
    waveformEnterLauncherScreen,
    waveformLeaveLauncherScreen,
    waveformTickLauncherScreen,
    waveformLauncherScreenRoot,
};

LauncherUi gUi;
bool gBuilt = false;
int gScrollY = 0;
float gVelocity = 0.0f;
bool gFlinging = false;

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

constexpr LauncherApp kApps[] = {
#ifdef SCREEN_WATCH
    {ScreenId::Watch, "Clock", "RTC and status", "TIME", 34, 131, 255},
#endif
#ifdef SCREEN_MOTION
    {ScreenId::Motion, "Motion", "IMU views", "IMU", 18, 195, 136},
#endif
#ifdef SCREEN_WEATHER
    {ScreenId::Weather, "Weather", "Forecast stack", "WX", 255, 167, 38},
#endif
#ifdef SCREEN_CALCULATOR
    {ScreenId::Calculator, "Calc", "Scientific", "MATH", 255, 92, 114},
#endif
#ifdef SCREEN_STOPWATCH
    {ScreenId::Stopwatch, "Timers", "Stopwatch + timer", "RUN", 255, 106, 56},
#endif
#ifdef SCREEN_RADIO
    {ScreenId::Radio, "Radio", "Wi-Fi + BLE", "RF", 92, 224, 224},
#endif
#ifdef SCREEN_LOCKET
    {ScreenId::Locket, "Locket", "Sky charm scene", "SKY", 148, 214, 255},
#endif
#ifdef SCREEN_SYSTEM
    {ScreenId::System, "System", "Power and SD", "SYS", 74, 222, 128},
#endif
#ifdef SCREEN_SETTINGS
    {ScreenId::Settings, "Settings", "Device options", "SET", 168, 148, 255},
#endif
#ifdef SCREEN_SPECTRUM
    {ScreenId::Spectrum, "Spectrum", "Audio visualizer", "AUD", 255, 100, 180},
#endif
};

constexpr size_t kVisibleAppCount = sizeof(kApps) / sizeof(kApps[0]);

void onLauncherAppTap(lv_event_t *event)
{
  uintptr_t rawId = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
  noteActivity();
  showScreenById(static_cast<ScreenId>(rawId));
}

void styleAppButton(lv_obj_t *button, const LauncherApp &app)
{
  lv_obj_set_style_bg_color(button, lvColor(12, 18, 30), 0);
  lv_obj_set_style_bg_color(button, lvColor(app.red, app.green, app.blue), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_20, LV_STATE_PRESSED);
  lv_obj_set_style_radius(button, 18, 0);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(button, lvColor(app.red, app.green, app.blue), 0);
  lv_obj_set_style_shadow_width(button, 14, 0);
  lv_obj_set_style_shadow_color(button, lvColor(app.red, app.green, app.blue), 0);
  lv_obj_set_style_shadow_opa(button, LV_OPA_10, 0);
  lv_obj_set_style_pad_all(button, 0, 0);
}

void updateLauncherStatus()
{
  if (!gUi.statusLabel) {
    return;
  }

  char status[128];
  if (bleManagerScanInProgress()) {
    snprintf(status,
             sizeof(status),
             "%s  |  BLE scanning",
             networkIsOnline() ? WiFi.SSID().c_str() : connectivityLabel());
  } else {
    snprintf(status,
             sizeof(status),
             "%s  |  %u BLE seen",
             networkIsOnline() ? WiFi.SSID().c_str() : connectivityLabel(),
             static_cast<unsigned>(bleManagerDeviceCount()));
  }
  lv_label_set_text(gUi.statusLabel, status);
}

void buildLauncherScreen()
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);

  lv_obj_t *eyebrow = lv_label_create(screen);
  lv_obj_set_style_text_font(eyebrow, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(eyebrow, lvColor(96, 112, 136), 0);
  lv_label_set_text(eyebrow, "WAVEFORM");
  lv_obj_align(eyebrow, LV_ALIGN_TOP_LEFT, 20, 18);

  lv_obj_t *headline = lv_label_create(screen);
  lv_obj_set_style_text_font(headline, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(headline, lvColor(244, 247, 252), 0);
  lv_label_set_text(headline, "Utility Deck");
  lv_obj_align(headline, LV_ALIGN_TOP_LEFT, 20, 40);

  gUi.statusLabel = lv_label_create(screen);
  lv_obj_set_width(gUi.statusLabel, LCD_WIDTH - 40);
  lv_obj_set_style_text_font(gUi.statusLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(gUi.statusLabel, lvColor(148, 161, 181), 0);
  lv_label_set_long_mode(gUi.statusLabel, LV_LABEL_LONG_WRAP);
  lv_obj_align(gUi.statusLabel, LV_ALIGN_TOP_LEFT, 20, 84);

  gUi.storageLabel = nullptr;

  constexpr lv_coord_t kTileWidth = 158;
  constexpr lv_coord_t kTileHeight = 74;
  constexpr lv_coord_t kStartX = 20;
  constexpr lv_coord_t kStartY = 110;
  constexpr lv_coord_t kGapX = 12;
  constexpr lv_coord_t kGapY = 14;

  for (size_t index = 0; index < kVisibleAppCount; ++index) {
    const LauncherApp &app = kApps[index];
    size_t row = index / 2;
    size_t column = index % 2;
    lv_coord_t x = kStartX + static_cast<lv_coord_t>(column) * (kTileWidth + kGapX);
    lv_coord_t y = kStartY + static_cast<lv_coord_t>(row) * (kTileHeight + kGapY);

    lv_obj_t *button = lv_button_create(screen);
    lv_obj_set_size(button, kTileWidth, kTileHeight);
    lv_obj_set_pos(button, x, y);
    styleAppButton(button, app);
    lv_obj_add_event_cb(button,
                        onLauncherAppTap,
                        LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(app.id)));

    lv_obj_t *badge = lv_label_create(button);
    lv_obj_set_style_text_font(badge, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(badge, lvColor(app.red, app.green, app.blue), 0);
    lv_label_set_text(badge, app.badge);
    lv_obj_align(badge, LV_ALIGN_TOP_LEFT, 14, 10);

    lv_obj_t *title = lv_label_create(button);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lvColor(244, 247, 252), 0);
    lv_label_set_text(title, app.title);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 28);

    lv_obj_t *subtitle = lv_label_create(button);
    lv_obj_set_width(subtitle, kTileWidth - 28);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(subtitle, lvColor(148, 161, 181), 0);
    lv_label_set_long_mode(subtitle, LV_LABEL_LONG_WRAP);
    lv_label_set_text(subtitle, app.subtitle);
    lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 14, 50);
  }

  gUi.screen = screen;
  screenRoots[static_cast<size_t>(ScreenId::Launcher)] = screen;
  gBuilt = true;
}
} // namespace

const ScreenModule &launcherScreenModule()
{
  return kModule;
}

lv_obj_t *waveformLauncherScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Launcher)];
}

bool waveformBuildLauncherScreen()
{
  if (!waveformLauncherScreenRoot()) {
    buildLauncherScreen();
  }
  return gBuilt && waveformLauncherScreenRoot() && gUi.statusLabel;
}

bool waveformRefreshLauncherScreen()
{
  if (!gBuilt || !gUi.statusLabel) {
    return false;
  }
  updateLauncherStatus();
  return true;
}

void waveformEnterLauncherScreen()
{
  updateLauncherStatus();
  gScrollY = 0;
  gVelocity = 0.0f;
  gFlinging = false;
  if (gUi.screen) lv_obj_scroll_to_y(gUi.screen, 0, LV_ANIM_OFF);
}

void waveformLeaveLauncherScreen()
{
}

void waveformTickLauncherScreen(uint32_t nowMs)
{
  (void)nowMs;
  if (!gFlinging || !gBuilt || !gUi.screen) return;
  if (fabsf(gVelocity) < 0.5f) {
    gVelocity = 0.0f;
    gFlinging = false;
    return;
  }
  constexpr int kContentHeight = 556;
  constexpr int kMaxScroll = kContentHeight - LCD_HEIGHT;
  gScrollY += static_cast<int>(gVelocity);
  if (gScrollY < 0) { gScrollY = 0; gVelocity = 0.0f; gFlinging = false; }
  else if (gScrollY > kMaxScroll) { gScrollY = kMaxScroll; gVelocity = 0.0f; gFlinging = false; }
  lv_obj_scroll_to_y(gUi.screen, gScrollY, LV_ANIM_OFF);
  gVelocity *= 0.88f;
}

void waveformLauncherScrollBy(int dy)
{
  if (!gBuilt || !gUi.screen) return;
  // 5 rows of tiles: kStartY(110) + 5*74 + 4*14 + 20 bottom padding = 556
  constexpr int kContentHeight = 556;
  constexpr int kMaxScroll = kContentHeight - LCD_HEIGHT;
  gFlinging = false;
  gVelocity = gVelocity * 0.4f + static_cast<float>(dy) * 0.6f;
  gScrollY += dy;
  if (gScrollY < 0) gScrollY = 0;
  if (gScrollY > kMaxScroll) gScrollY = kMaxScroll;
  lv_obj_scroll_to_y(gUi.screen, gScrollY, LV_ANIM_OFF);
}

void waveformLauncherScrollFling()
{
  if (!gBuilt) return;
  if (fabsf(gVelocity) > 1.0f) gFlinging = true;
}

#endif
