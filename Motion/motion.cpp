#include "screen_constants.h"
#include "screen_manager.h"
#include "imu_module.h"
#include "math_utils.h"
#include "screen_callbacks.h"

extern lv_obj_t *screenRoots[];
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
void enableTapBubbling(lv_obj_t *obj);

namespace
{
lv_obj_t *motionScreen = nullptr;
lv_obj_t *motionTapOverlay = nullptr;
lv_obj_t *motionDotBoundary = nullptr;
lv_obj_t *motionDotCrossH = nullptr;
lv_obj_t *motionDotCrossV = nullptr;
lv_obj_t *motionDot = nullptr;
bool motionBuilt = false;

const ScreenModule kModule = {
    ScreenId::Motion,
    "Motion",
    waveformBuildMotionScreen,
    waveformRefreshMotionScreen,
    waveformEnterMotionScreen,
    waveformLeaveMotionScreen,
    waveformTickMotionScreen,
    waveformMotionScreenRoot,
};

float normalizeMotionIndicatorOffset(float offset)
{
  float magnitude = fabsf(offset);
  float adjustedMagnitude = (magnitude - kMotionIndicatorDeadzoneVector) /
                            (kMotionIndicatorFullScaleVector - kMotionIndicatorDeadzoneVector);
  adjustedMagnitude = constrain(adjustedMagnitude, 0.0f, 1.0f);
  return copysignf(adjustedMagnitude, offset);
}

void updateMotionDot()
{
  const MotionState &state = imuModuleState();
  if (!state.valid || !motionDotBoundary || !motionDot) {
    return;
  }

  if (!imuModuleReferenceReady() && !imuModuleReferenceCapturePending()) {
    imuModuleCaptureReference();
  }

  Vec3 currentDown = normalizedDownVector(state);

  lv_area_t boundary = {};
  lv_obj_get_coords(motionDotBoundary, &boundary);
  int centerX = (boundary.x1 + boundary.x2) / 2;
  int centerY = (boundary.y1 + boundary.y2) / 2;

  if (imuModuleReferenceCapturePending()) {
    Vec3 refDown = currentDown;
    Vec3 refAxisA = stablePerpendicular(refDown, vec3(1.0f, 0.0f, 0.0f));
    Vec3 refAxisB = normalizeVec3(crossVec3(refDown, refAxisA));
    if (lengthVec3(refAxisB) < 0.1f) {
      refAxisB = stablePerpendicular(refDown, vec3(0.0f, 1.0f, 0.0f));
    }

    imuModuleSetReferenceDown(refDown);
    imuModuleSetReferenceAxisA(refAxisA);
    imuModuleSetReferenceAxisB(refAxisB);
    imuModuleSetReferenceReady(true);
    imuModuleSetReferenceCapturePending(false);
    imuModuleSetDotPitch(0.0f);
    imuModuleSetDotRoll(0.0f);
    imuModuleSetFilterReady(true);
    motionCenterDot();
    return;
  }

  if (!imuModuleReferenceReady()) {
    return;
  }

  Vec3 downDelta = subtractVec3(currentDown, imuModuleReferenceDown());
  float horizontalOffset = dotVec3(downDelta, imuModuleReferenceAxisA());
  float verticalOffset = dotVec3(downDelta, imuModuleReferenceAxisB());

  float dotPitch = imuModuleDotPitch();
  float dotRoll = imuModuleDotRoll();
  if (!imuModuleFilterReady()) {
    dotPitch = verticalOffset;
    dotRoll = horizontalOffset;
    imuModuleSetFilterReady(true);
  } else {
    dotPitch += (verticalOffset - dotPitch) * kMotionIndicatorSmoothingAlpha;
    dotRoll += (horizontalOffset - dotRoll) * kMotionIndicatorSmoothingAlpha;
  }
  imuModuleSetDotPitch(dotPitch);
  imuModuleSetDotRoll(dotRoll);

  int travel = ((boundary.x2 - boundary.x1) / 2) - 22;
  float normalizedX = normalizeMotionIndicatorOffset(dotRoll);
  float normalizedY = normalizeMotionIndicatorOffset(dotPitch);
  int x = centerX + static_cast<int>(normalizedX * travel) - (kDotDiameter / 2);
  int y = centerY - static_cast<int>(normalizedY * travel) - (kDotDiameter / 2);
  x = constrain(x, 0, LCD_WIDTH - kDotDiameter);
  y = constrain(y, 0, LCD_HEIGHT - kDotDiameter);
  lv_obj_set_pos(motionDot, x, y);
}

void buildMotionScreen()
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);
  motionScreen = screen;

  motionDotBoundary = lv_obj_create(screen);
  lv_obj_set_size(motionDotBoundary, LCD_WIDTH - 24, LCD_WIDTH - 24);
  lv_obj_align(motionDotBoundary, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(motionDotBoundary, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(motionDotBoundary, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(motionDotBoundary, 2, 0);
  lv_obj_set_style_border_color(motionDotBoundary, lvColor(52, 58, 66), 0);

  motionDotCrossH = lv_obj_create(screen);
  lv_obj_set_size(motionDotCrossH, LCD_WIDTH - 48, 2);
  lv_obj_align(motionDotCrossH, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(motionDotCrossH, lvColor(38, 44, 52), 0);
  lv_obj_set_style_border_width(motionDotCrossH, 0, 0);

  motionDotCrossV = lv_obj_create(screen);
  lv_obj_set_size(motionDotCrossV, 2, LCD_WIDTH - 48);
  lv_obj_align(motionDotCrossV, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(motionDotCrossV, lvColor(38, 44, 52), 0);
  lv_obj_set_style_border_width(motionDotCrossV, 0, 0);

  motionDot = lv_obj_create(screen);
  lv_obj_set_size(motionDot, kDotDiameter, kDotDiameter);
  lv_obj_set_style_radius(motionDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(motionDot, lvColor(255, 255, 255), 0);
  lv_obj_set_style_border_width(motionDot, 3, 0);
  lv_obj_set_style_border_color(motionDot, lvColor(0, 0, 0), 0);
  motionCenterDot();

  motionTapOverlay = lv_obj_create(screen);
  applyRootStyle(motionTapOverlay);
  enableTapBubbling(motionTapOverlay);
  lv_obj_move_foreground(motionTapOverlay);

  screenRoots[static_cast<size_t>(ScreenId::Motion)] = screen;
  motionBuilt = true;
}
} // namespace

void motionHandleTap()
{
  imuModuleCaptureReference();
  motionCenterDot();
}

void motionCenterDot()
{
  if (!motionDot) {
    return;
  }

  if (motionDotBoundary) {
    lv_obj_align_to(motionDot, motionDotBoundary, LV_ALIGN_CENTER, 0, 0);
  } else {
    lv_obj_align(motionDot, LV_ALIGN_CENTER, 0, 0);
  }
}

const ScreenModule &motionScreenModule()
{
  return kModule;
}

lv_obj_t *waveformMotionScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Motion)];
}

lv_obj_t *waveformMotionTapTarget()
{
  return motionTapOverlay ? motionTapOverlay : waveformMotionScreenRoot();
}

bool waveformBuildMotionScreen()
{
  if (!waveformMotionScreenRoot()) {
    buildMotionScreen();
  }

  return motionBuilt && waveformMotionScreenRoot() && motionDotBoundary && motionDot;
}

bool waveformRefreshMotionScreen()
{
  if (!motionBuilt || !motionScreen) {
    return false;
  }

  updateMotionDot();
  return true;
}

void waveformEnterMotionScreen()
{
  imuModuleCaptureReference();
  motionCenterDot();
  updateMotionDot();
}

void waveformLeaveMotionScreen()
{
}

void waveformTickMotionScreen(uint32_t nowMs)
{
  LV_UNUSED(nowMs);
  updateMotionDot();
}
