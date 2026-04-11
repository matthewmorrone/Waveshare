#include "screen_manager.h"

#ifdef SCREEN_LAUNCHER

extern lv_obj_t *screenRoots[];

namespace
{
struct LauncherUi
{
  lv_obj_t *screen = nullptr;
  lv_obj_t *titleLabel = nullptr;
  lv_obj_t *messageLabel = nullptr;
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

lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b)
{
  return lv_color_make(r, g, b);
}

void applyRootStyle(lv_obj_t *obj)
{
  lv_obj_set_size(obj, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_style_bg_color(obj, lvColor(6, 10, 18), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_outline_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void buildLauncherScreen()
{
  gUi.screen = lv_obj_create(nullptr);
  applyRootStyle(gUi.screen);

  gUi.titleLabel = lv_label_create(gUi.screen);
  lv_obj_set_style_text_font(gUi.titleLabel, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(gUi.titleLabel, lvColor(244, 247, 252), 0);
  lv_label_set_text(gUi.titleLabel, "Launcher");
  lv_obj_align(gUi.titleLabel, LV_ALIGN_TOP_MID, 0, 72);

  gUi.messageLabel = lv_label_create(gUi.screen);
  lv_obj_set_width(gUi.messageLabel, LCD_WIDTH - 64);
  lv_obj_set_style_text_font(gUi.messageLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(gUi.messageLabel, lvColor(148, 161, 181), 0);
  lv_obj_set_style_text_align(gUi.messageLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(gUi.messageLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(gUi.messageLabel, "Launcher is intentionally empty for now.");
  lv_obj_align(gUi.messageLabel, LV_ALIGN_CENTER, 0, 20);

  screenRoots[static_cast<size_t>(ScreenId::Launcher)] = gUi.screen;
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
  return gBuilt && waveformLauncherScreenRoot() && gUi.messageLabel;
}

bool waveformRefreshLauncherScreen()
{
  return gBuilt && waveformLauncherScreenRoot() && gUi.messageLabel;
}

void waveformEnterLauncherScreen()
{
}

void waveformLeaveLauncherScreen()
{
}

void waveformTickLauncherScreen(uint32_t nowMs)
{
  (void)nowMs;
}

void waveformLauncherScrollBy(int dy)
{
  (void)dy;
}

void waveformLauncherScrollFling()
{
}

#endif
