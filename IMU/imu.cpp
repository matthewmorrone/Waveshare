#include "screen_constants.h"
#include "screen_manager.h"
#include "imu_module.h"
#include "screen_callbacks.h"

extern lv_obj_t *screenRoots[];
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);

namespace
{
lv_obj_t *imuScreen = nullptr;
lv_obj_t *imuRawCard = nullptr;
lv_obj_t *imuOrientation = nullptr;
lv_obj_t *imuPitch = nullptr;
lv_obj_t *imuRoll = nullptr;
lv_obj_t *imuAccel = nullptr;
lv_obj_t *imuGyro = nullptr;
lv_obj_t *imuPitchCaption = nullptr;
lv_obj_t *imuRollCaption = nullptr;
lv_obj_t *imuAccelCaption = nullptr;
lv_obj_t *imuGyroCaption = nullptr;
bool imuBuilt = false;

const ScreenModule kModule = {
    ScreenId::Imu,
    "IMU",
    waveformBuildImuScreen,
    waveformRefreshImuScreen,
    waveformEnterImuScreen,
    waveformLeaveImuScreen,
    waveformTickImuScreen,
    waveformImuScreenRoot,
};

void updateImuRaw()
{
  if (!imuOrientation || !imuPitch || !imuRoll || !imuAccel || !imuGyro) {
    return;
  }

  const MotionState &displayState = imuModuleDisplayState();
  if (!displayState.valid) {
    lv_label_set_text(imuOrientation, "Unavailable");
    lv_label_set_text(imuPitch, "--.-");
    lv_label_set_text(imuRoll, "--.-");
    lv_label_set_text(imuAccel, "X  --.-\nY  --.-\nZ  --.-");
    lv_label_set_text(imuGyro, "X  ---\nY  ---\nZ  ---");
    return;
  }

  char line[80];
  lv_label_set_text(imuOrientation, displayState.orientation);

  snprintf(line, sizeof(line), "%+.0f\xc2\xb0", displayState.pitch);
  lv_label_set_text(imuPitch, line);
  snprintf(line, sizeof(line), "%+.0f\xc2\xb0", displayState.roll);
  lv_label_set_text(imuRoll, line);
  snprintf(line, sizeof(line),
           "X  %+0.1f\nY  %+0.1f\nZ  %+0.1f",
           displayState.ax, displayState.ay, displayState.az);
  lv_label_set_text(imuAccel, line);
  snprintf(line, sizeof(line),
           "X  %+0.0f\nY  %+0.0f\nZ  %+0.0f",
           displayState.gx, displayState.gy, displayState.gz);
  lv_label_set_text(imuGyro, line);
}

void buildImuScreen()
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);
  imuScreen = screen;

  imuRawCard = lv_obj_create(screen);
  lv_obj_set_size(imuRawCard, LCD_WIDTH - 36, LCD_HEIGHT - 68);
  lv_obj_align(imuRawCard, LV_ALIGN_CENTER, 0, 6);
  lv_obj_set_style_radius(imuRawCard, 26, 0);
  lv_obj_set_style_bg_color(imuRawCard, lvColor(10, 14, 20), 0);
  lv_obj_set_style_bg_opa(imuRawCard, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(imuRawCard, 1, 0);
  lv_obj_set_style_border_color(imuRawCard, lvColor(28, 36, 48), 0);
  lv_obj_set_style_outline_width(imuRawCard, 0, 0);
  lv_obj_set_style_pad_all(imuRawCard, 0, 0);
  lv_obj_clear_flag(imuRawCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(imuRawCard, LV_SCROLLBAR_MODE_OFF);

  imuOrientation = lv_label_create(imuRawCard);
  lv_obj_set_width(imuOrientation, LCD_WIDTH - 76);
  lv_obj_set_style_text_font(imuOrientation, &lv_font_montserrat_36, 0);
  lv_obj_set_style_text_color(imuOrientation, lvColor(250, 252, 255), 0);
  lv_obj_set_style_text_align(imuOrientation, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(imuOrientation, "Unavailable");
  lv_obj_align(imuOrientation, LV_ALIGN_TOP_MID, 0, 26);

  imuPitchCaption = lv_label_create(imuRawCard);
  lv_obj_set_style_text_font(imuPitchCaption, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(imuPitchCaption, lvColor(110, 126, 146), 0);
  lv_label_set_text(imuPitchCaption, "PITCH");
  lv_obj_align(imuPitchCaption, LV_ALIGN_TOP_LEFT, 28, 96);

  imuRollCaption = lv_label_create(imuRawCard);
  lv_obj_set_style_text_font(imuRollCaption, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(imuRollCaption, lvColor(110, 126, 146), 0);
  lv_label_set_text(imuRollCaption, "ROLL");
  lv_obj_align(imuRollCaption, LV_ALIGN_TOP_RIGHT, -28, 96);

  imuPitch = lv_label_create(imuRawCard);
  lv_obj_set_width(imuPitch, 112);
  lv_obj_set_style_text_font(imuPitch, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(imuPitch, lvColor(148, 214, 255), 0);
  lv_obj_set_style_text_align(imuPitch, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_text(imuPitch, "--.-");
  lv_obj_align_to(imuPitch, imuPitchCaption, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);

  imuRoll = lv_label_create(imuRawCard);
  lv_obj_set_width(imuRoll, 112);
  lv_obj_set_style_text_font(imuRoll, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(imuRoll, lvColor(244, 246, 250), 0);
  lv_obj_set_style_text_align(imuRoll, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text(imuRoll, "--.-");
  lv_obj_align_to(imuRoll, imuRollCaption, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 6);

  imuAccelCaption = lv_label_create(imuRawCard);
  lv_obj_set_style_text_font(imuAccelCaption, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(imuAccelCaption, lvColor(110, 126, 146), 0);
  lv_label_set_text(imuAccelCaption, "ACCELEROMETER");
  lv_obj_align(imuAccelCaption, LV_ALIGN_TOP_LEFT, 28, 176);

  imuAccel = lv_label_create(imuRawCard);
  lv_obj_set_width(imuAccel, 124);
  lv_obj_set_style_text_font(imuAccel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(imuAccel, lvColor(214, 220, 230), 0);
  lv_obj_set_style_text_line_space(imuAccel, 8, 0);
  lv_obj_set_style_text_align(imuAccel, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_long_mode(imuAccel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(imuAccel, "X  --.-\nY  --.-\nZ  --.-");
  lv_obj_align_to(imuAccel, imuAccelCaption, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);

  imuGyroCaption = lv_label_create(imuRawCard);
  lv_obj_set_style_text_font(imuGyroCaption, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(imuGyroCaption, lvColor(110, 126, 146), 0);
  lv_label_set_text(imuGyroCaption, "GYROSCOPE");
  lv_obj_align(imuGyroCaption, LV_ALIGN_TOP_RIGHT, -28, 176);

  imuGyro = lv_label_create(imuRawCard);
  lv_obj_set_width(imuGyro, 124);
  lv_obj_set_style_text_font(imuGyro, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(imuGyro, lvColor(160, 194, 255), 0);
  lv_obj_set_style_text_line_space(imuGyro, 8, 0);
  lv_obj_set_style_text_align(imuGyro, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(imuGyro, LV_LABEL_LONG_WRAP);
  lv_label_set_text(imuGyro, "X  ---\nY  ---\nZ  ---");
  lv_obj_align_to(imuGyro, imuGyroCaption, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 10);

  screenRoots[static_cast<size_t>(ScreenId::Imu)] = screen;
  imuBuilt = true;
}
} // namespace

const ScreenModule &imuScreenModule()
{
  return kModule;
}

lv_obj_t *waveformImuScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Imu)];
}

bool waveformBuildImuScreen()
{
  if (!waveformImuScreenRoot()) {
    buildImuScreen();
  }

  return imuBuilt && waveformImuScreenRoot() && imuRawCard && imuOrientation;
}

bool waveformRefreshImuScreen()
{
  if (!imuBuilt || !imuScreen) {
    return false;
  }

  updateImuRaw();
  return true;
}

void waveformEnterImuScreen()
{
  updateImuRaw();
}

void waveformLeaveImuScreen()
{
}

void waveformTickImuScreen(uint32_t nowMs)
{
  LV_UNUSED(nowMs);
  updateImuRaw();
}
