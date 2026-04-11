#include "screen_constants.h"
#include "screen_manager.h"
#include "imu_module.h"
#include "math_utils.h"
#include "screen_callbacks.h"
extern lv_obj_t *screenRoots[];
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
void enableTapBubbling(lv_obj_t *obj);
void styleLine(lv_obj_t *line, lv_color_t color, int width);
void setObjYOffset(void *obj, int32_t value);

extern bool touchPressed;

void setLinePoints(lv_point_precise_t points[2], float x1, float y1, float x2, float y2)
{
  points[0].x = x1;
  points[0].y = y1;
  points[1].x = x2;
  points[1].y = y2;
}

namespace
{
lv_obj_t *motionScreen = nullptr;
lv_obj_t *motionTapOverlay = nullptr;
lv_obj_t *motionDotView = nullptr;
lv_obj_t *motionDotBoundary = nullptr;
lv_obj_t *motionDotCrossH = nullptr;
lv_obj_t *motionDotCrossV = nullptr;
lv_obj_t *motionDot = nullptr;
lv_obj_t *motionCubeView = nullptr;
lv_obj_t *motionRawView = nullptr;
lv_obj_t *motionRawCard = nullptr;
lv_obj_t *motionRawOrientation = nullptr;
lv_obj_t *motionRawPitch = nullptr;
lv_obj_t *motionRawRoll = nullptr;
lv_obj_t *motionRawAccel = nullptr;
lv_obj_t *motionRawGyro = nullptr;
lv_obj_t *motionRawPitchCaption = nullptr;
lv_obj_t *motionRawRollCaption = nullptr;
lv_obj_t *motionRawAccelCaption = nullptr;
lv_obj_t *motionRawGyroCaption = nullptr;
lv_obj_t *motionCubeEdges[kCubeEdgeCount] = {nullptr};
lv_obj_t *motionCubeArrows[kCubeArrowLineCount] = {nullptr};
lv_point_precise_t motionCubeEdgePoints[kCubeEdgeCount][2] = {};
lv_point_precise_t motionCubeArrowPoints[kCubeArrowLineCount][2] = {};

MotionViewMode motionViewMode = MotionViewMode::Dot;
MotionViewMode renderedMotionViewMode = MotionViewMode::Count;
int renderedMotionTransitionDirection = 0;
bool motionBuilt = false;
constexpr MotionViewMode kStandaloneMotionView = MotionViewMode::Cube;

const ScreenModule kModule = {
    ScreenId::Motion,
    "Cube",
    waveformBuildMotionScreen,
    waveformRefreshMotionScreen,
    waveformEnterMotionScreen,
    waveformLeaveMotionScreen,
    waveformTickMotionScreen,
    waveformMotionScreenRoot,
};

lv_obj_t *motionViewObject(MotionViewMode mode)
{
  switch (mode) {
    case MotionViewMode::Dot:
      return motionDotView;
    case MotionViewMode::Cube:
      return motionCubeView;
    case MotionViewMode::Raw:
      return motionRawView;
    default:
      return nullptr;
  }
}

void showMotionViewInstant(MotionViewMode mode)
{
  if (!motionDotView || !motionCubeView || !motionRawView) {
    return;
  }

  lv_obj_add_flag(motionDotView, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(motionCubeView, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(motionRawView, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_y(motionDotView, 0);
  lv_obj_set_y(motionCubeView, 0);
  lv_obj_set_y(motionRawView, 0);

  lv_obj_t *target = motionViewObject(mode);
  if (target) {
    lv_obj_clear_flag(target, LV_OBJ_FLAG_HIDDEN);
  }
  renderedMotionViewMode = mode;
}

void animateMotionViewTo(MotionViewMode mode)
{
  lv_obj_t *target = motionViewObject(mode);
  if (!target) {
    return;
  }

  if (renderedMotionViewMode == MotionViewMode::Count || renderedMotionViewMode == mode) {
    showMotionViewInstant(mode);
    return;
  }

  lv_obj_t *previous = motionViewObject(renderedMotionViewMode);
  if (previous) {
    lv_obj_add_flag(previous, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(previous, 0);
  }

  lv_obj_clear_flag(target, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_y(target, renderedMotionTransitionDirection * kMotionViewAnimOffsetPx);

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, target);
  lv_anim_set_values(&anim, renderedMotionTransitionDirection * kMotionViewAnimOffsetPx, 0);
  lv_anim_set_time(&anim, kMotionViewAnimMs);
  lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&anim, setObjYOffset);
  lv_anim_start(&anim);

  renderedMotionViewMode = mode;
}

float normalizeMotionIndicatorOffset(float offset)
{
  float magnitude = fabsf(offset);
  float adjustedMagnitude = (magnitude - kMotionIndicatorDeadzoneVector) /
                            (kMotionIndicatorFullScaleVector - kMotionIndicatorDeadzoneVector);
  adjustedMagnitude = constrain(adjustedMagnitude, 0.0f, 1.0f);
  return copysignf(adjustedMagnitude, offset);
}

void motionCubeBasis(const MotionState &state, Vec3 &down, Vec3 &axisA, Vec3 &axisB)
{
  down = normalizedDownVector(state);
  if (!imuModuleReferenceReady()) {
    imuModuleCaptureReference();
  }

  if (!imuModuleReferenceReady()) {
    axisA = vec3(1.0f, 0.0f, 0.0f);
    axisB = vec3(0.0f, 1.0f, 0.0f);
    return;
  }

  axisA = stablePerpendicular(down, imuModuleReferenceAxisA());
  axisB = normalizeVec3(crossVec3(down, axisA));
  if (lengthVec3(axisB) < 0.1f) {
    axisB = stablePerpendicular(down, imuModuleReferenceAxisB());
    axisA = normalizeVec3(crossVec3(axisB, down));
  }
}

void updateMotionDot()
{
  const MotionState &state = imuModuleState();
  if (!state.valid) {
    return;
  }

  if (!imuModuleReferenceReady()) {
    if (!imuModuleReferenceCapturePending()) {
      imuModuleCaptureReference();
    }
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
    lv_obj_set_pos(motionDot, centerX - (kDotDiameter / 2), centerY - (kDotDiameter / 2));
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

void updateMotionRaw()
{
  const MotionState &displayState = imuModuleDisplayState();
  if (!displayState.valid) {
    lv_label_set_text(motionRawOrientation, "Unavailable");
    lv_label_set_text(motionRawPitch, "--.-");
    lv_label_set_text(motionRawRoll, "--.-");
    lv_label_set_text(motionRawAccel, "X  --.-\nY  --.-\nZ  --.-");
    lv_label_set_text(motionRawGyro, "X  ---\nY  ---\nZ  ---");
    return;
  }

  char line[80];
  lv_label_set_text(motionRawOrientation, displayState.orientation);

  snprintf(line, sizeof(line), "%+.0f\xc2\xb0", displayState.pitch);
  lv_label_set_text(motionRawPitch, line);
  snprintf(line, sizeof(line), "%+.0f\xc2\xb0", displayState.roll);
  lv_label_set_text(motionRawRoll, line);
  snprintf(line, sizeof(line),
           "X  %+0.1f\nY  %+0.1f\nZ  %+0.1f",
           displayState.ax, displayState.ay, displayState.az);
  lv_label_set_text(motionRawAccel, line);
  snprintf(line, sizeof(line),
           "X  %+0.0f\nY  %+0.0f\nZ  %+0.0f",
           displayState.gx, displayState.gy, displayState.gz);
  lv_label_set_text(motionRawGyro, line);
}

void updateMotionCube()
{
  const MotionState &displayState = imuModuleState();
  if (!displayState.valid) {
    return;
  }

  Vec3 down, axisA, axisB;
  motionCubeBasis(displayState, down, axisA, axisB);

  constexpr Vec3 corners[8] = {
      {-1.0f, -1.0f, -1.0f},
      {1.0f, -1.0f, -1.0f},
      {1.0f, 1.0f, -1.0f},
      {-1.0f, 1.0f, -1.0f},
      {-1.0f, -1.0f, 1.0f},
      {1.0f, -1.0f, 1.0f},
      {1.0f, 1.0f, 1.0f},
      {-1.0f, 1.0f, 1.0f},
  };

  const uint8_t edges[kCubeEdgeCount][2] = {
      {0, 1}, {1, 2}, {2, 3}, {3, 0},
      {4, 5}, {5, 6}, {6, 7}, {7, 4},
      {0, 4}, {1, 5}, {2, 6}, {3, 7},
  };

  lv_point_precise_t projected[8];
  int centerX = LCD_WIDTH / 2;
  int centerY = LCD_HEIGHT / 2;
  float scale = 78.0f;

  for (size_t i = 0; i < 8; ++i) {
    const Vec3 &corner = corners[i];
    float px = (corner.x * axisA.x + corner.y * axisB.x + corner.z * down.x);
    float py = (corner.x * axisA.y + corner.y * axisB.y + corner.z * down.y);
    float pz = (corner.x * axisA.z + corner.y * axisB.z + corner.z * down.z);
    float perspective = 1.0f + (pz * 0.28f);
    projected[i].x = centerX + (px * scale * perspective);
    projected[i].y = centerY - (py * scale * perspective);
  }

  for (size_t i = 0; i < kCubeEdgeCount; ++i) {
    setLinePoints(motionCubeEdgePoints[i],
                  projected[edges[i][0]].x, projected[edges[i][0]].y,
                  projected[edges[i][1]].x, projected[edges[i][1]].y);
  }

  Vec3 arrows[kCubeArrowCount] = {scaleVec3(down, -1.0f), axisA, axisB};
  for (size_t i = 0; i < kCubeArrowCount; ++i) {
    Vec3 d = normalizeVec3(arrows[i]);
    float tipX = centerX + (d.x * 112.0f);
    float tipY = centerY - (d.y * 112.0f);
    float leftX = tipX - (d.x * 14.0f) - (d.y * 9.0f);
    float leftY = tipY + (d.y * 14.0f) - (d.x * 9.0f);
    float rightX = tipX - (d.x * 14.0f) + (d.y * 9.0f);
    float rightY = tipY + (d.y * 14.0f) + (d.x * 9.0f);

    size_t base = i * kCubeArrowSegmentCount;
    setLinePoints(motionCubeArrowPoints[base], centerX, centerY, tipX, tipY);
    setLinePoints(motionCubeArrowPoints[base + 1], tipX, tipY, leftX, leftY);
    setLinePoints(motionCubeArrowPoints[base + 2], tipX, tipY, rightX, rightY);
  }

  lv_obj_invalidate(motionCubeView);
}

void setMotionViewVisibility()
{
  if (renderedMotionViewMode != motionViewMode) {
    showMotionViewInstant(motionViewMode);
  }
}

void refreshMotionScreen()
{
  if (!motionBuilt) {
    return;
  }

  setMotionViewVisibility();
  switch (motionViewMode) {
    case MotionViewMode::Dot:
      updateMotionDot();
      break;
    case MotionViewMode::Cube:
      if (touchPressed) {
        break;
      }
      updateMotionCube();
      break;
    case MotionViewMode::Raw:
      updateMotionRaw();
      break;
    default:
      break;
  }
}

void buildMotionScreen()
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);
  motionScreen = screen;

  motionDotView = lv_obj_create(screen);
  applyRootStyle(motionDotView);
  lv_obj_set_size(motionDotView, LCD_WIDTH, LCD_HEIGHT);
  enableTapBubbling(motionDotView);

  motionDotBoundary = lv_obj_create(motionDotView);
  lv_obj_set_size(motionDotBoundary, LCD_WIDTH - 24, LCD_WIDTH - 24);
  lv_obj_align(motionDotBoundary, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(motionDotBoundary, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(motionDotBoundary, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(motionDotBoundary, 2, 0);
  lv_obj_set_style_border_color(motionDotBoundary, lvColor(52, 58, 66), 0);

  motionDotCrossH = lv_obj_create(motionDotView);
  lv_obj_set_size(motionDotCrossH, LCD_WIDTH - 48, 2);
  lv_obj_align(motionDotCrossH, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(motionDotCrossH, lvColor(38, 44, 52), 0);
  lv_obj_set_style_border_width(motionDotCrossH, 0, 0);

  motionDotCrossV = lv_obj_create(motionDotView);
  lv_obj_set_size(motionDotCrossV, 2, LCD_WIDTH - 48);
  lv_obj_align(motionDotCrossV, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(motionDotCrossV, lvColor(38, 44, 52), 0);
  lv_obj_set_style_border_width(motionDotCrossV, 0, 0);

  motionDot = lv_obj_create(motionDotView);
  lv_obj_set_size(motionDot, kDotDiameter, kDotDiameter);
  lv_obj_set_style_radius(motionDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(motionDot, lvColor(255, 255, 255), 0);
  lv_obj_set_style_border_width(motionDot, 3, 0);
  lv_obj_set_style_border_color(motionDot, lvColor(0, 0, 0), 0);
  lv_obj_align(motionDot, LV_ALIGN_CENTER, 0, 0);

  motionCubeView = lv_obj_create(screen);
  applyRootStyle(motionCubeView);
  lv_obj_set_size(motionCubeView, LCD_WIDTH, LCD_HEIGHT);
  enableTapBubbling(motionCubeView);

  for (size_t i = 0; i < kCubeEdgeCount; ++i) {
    motionCubeEdges[i] = lv_line_create(motionCubeView);
    lv_obj_set_size(motionCubeEdges[i], LCD_WIDTH, LCD_HEIGHT);
    styleLine(motionCubeEdges[i], lvColor(88, 96, 110), 2);
    lv_line_set_points(motionCubeEdges[i], motionCubeEdgePoints[i], 2);
  }

  lv_color_t arrowColors[kCubeArrowCount] = {
      lvColor(46, 142, 255),
      lvColor(255, 255, 255),
      lvColor(140, 154, 176),
  };
  for (size_t i = 0; i < kCubeArrowLineCount; ++i) {
    motionCubeArrows[i] = lv_line_create(motionCubeView);
    lv_obj_set_size(motionCubeArrows[i], LCD_WIDTH, LCD_HEIGHT);
    styleLine(motionCubeArrows[i], arrowColors[i / kCubeArrowSegmentCount], 3);
    lv_line_set_points(motionCubeArrows[i], motionCubeArrowPoints[i], 2);
  }

  motionRawView = lv_obj_create(screen);
  applyRootStyle(motionRawView);
  lv_obj_set_size(motionRawView, LCD_WIDTH, LCD_HEIGHT);
  enableTapBubbling(motionRawView);

  motionRawCard = lv_obj_create(motionRawView);
  lv_obj_set_size(motionRawCard, LCD_WIDTH - 36, LCD_HEIGHT - 68);
  lv_obj_align(motionRawCard, LV_ALIGN_CENTER, 0, 6);
  lv_obj_set_style_radius(motionRawCard, 26, 0);
  lv_obj_set_style_bg_color(motionRawCard, lvColor(10, 14, 20), 0);
  lv_obj_set_style_bg_opa(motionRawCard, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(motionRawCard, 1, 0);
  lv_obj_set_style_border_color(motionRawCard, lvColor(28, 36, 48), 0);
  lv_obj_set_style_outline_width(motionRawCard, 0, 0);
  lv_obj_set_style_pad_all(motionRawCard, 0, 0);
  lv_obj_clear_flag(motionRawCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(motionRawCard, LV_SCROLLBAR_MODE_OFF);

  motionRawOrientation = lv_label_create(motionRawCard);
  lv_obj_set_width(motionRawOrientation, LCD_WIDTH - 76);
  lv_obj_set_style_text_font(motionRawOrientation, &lv_font_montserrat_36, 0);
  lv_obj_set_style_text_color(motionRawOrientation, lvColor(250, 252, 255), 0);
  lv_obj_set_style_text_align(motionRawOrientation, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(motionRawOrientation, "Unavailable");
  lv_obj_align(motionRawOrientation, LV_ALIGN_TOP_MID, 0, 26);

  motionRawPitchCaption = lv_label_create(motionRawCard);
  lv_obj_set_style_text_font(motionRawPitchCaption, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(motionRawPitchCaption, lvColor(110, 126, 146), 0);
  lv_label_set_text(motionRawPitchCaption, "PITCH");
  lv_obj_align(motionRawPitchCaption, LV_ALIGN_TOP_LEFT, 28, 96);

  motionRawRollCaption = lv_label_create(motionRawCard);
  lv_obj_set_style_text_font(motionRawRollCaption, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(motionRawRollCaption, lvColor(110, 126, 146), 0);
  lv_label_set_text(motionRawRollCaption, "ROLL");
  lv_obj_align(motionRawRollCaption, LV_ALIGN_TOP_RIGHT, -28, 96);

  motionRawPitch = lv_label_create(motionRawCard);
  lv_obj_set_width(motionRawPitch, 112);
  lv_obj_set_style_text_font(motionRawPitch, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(motionRawPitch, lvColor(148, 214, 255), 0);
  lv_obj_set_style_text_align(motionRawPitch, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_text(motionRawPitch, "--.-");
  lv_obj_align_to(motionRawPitch, motionRawPitchCaption, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);

  motionRawRoll = lv_label_create(motionRawCard);
  lv_obj_set_width(motionRawRoll, 112);
  lv_obj_set_style_text_font(motionRawRoll, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(motionRawRoll, lvColor(244, 246, 250), 0);
  lv_obj_set_style_text_align(motionRawRoll, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text(motionRawRoll, "--.-");
  lv_obj_align_to(motionRawRoll, motionRawRollCaption, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 6);

  motionRawAccelCaption = lv_label_create(motionRawCard);
  lv_obj_set_style_text_font(motionRawAccelCaption, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(motionRawAccelCaption, lvColor(110, 126, 146), 0);
  lv_label_set_text(motionRawAccelCaption, "ACCELEROMETER");
  lv_obj_align(motionRawAccelCaption, LV_ALIGN_TOP_LEFT, 28, 176);

  motionRawAccel = lv_label_create(motionRawCard);
  lv_obj_set_width(motionRawAccel, 124);
  lv_obj_set_style_text_font(motionRawAccel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(motionRawAccel, lvColor(214, 220, 230), 0);
  lv_obj_set_style_text_line_space(motionRawAccel, 8, 0);
  lv_obj_set_style_text_align(motionRawAccel, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_long_mode(motionRawAccel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(motionRawAccel, "X  --.-\nY  --.-\nZ  --.-");
  lv_obj_align_to(motionRawAccel, motionRawAccelCaption, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);

  motionRawGyroCaption = lv_label_create(motionRawCard);
  lv_obj_set_style_text_font(motionRawGyroCaption, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(motionRawGyroCaption, lvColor(110, 126, 146), 0);
  lv_label_set_text(motionRawGyroCaption, "GYROSCOPE");
  lv_obj_align(motionRawGyroCaption, LV_ALIGN_TOP_RIGHT, -28, 176);

  motionRawGyro = lv_label_create(motionRawCard);
  lv_obj_set_width(motionRawGyro, 124);
  lv_obj_set_style_text_font(motionRawGyro, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(motionRawGyro, lvColor(160, 194, 255), 0);
  lv_obj_set_style_text_line_space(motionRawGyro, 8, 0);
  lv_obj_set_style_text_align(motionRawGyro, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(motionRawGyro, LV_LABEL_LONG_WRAP);
  lv_label_set_text(motionRawGyro, "X  ---\nY  ---\nZ  ---");
  lv_obj_align_to(motionRawGyro, motionRawGyroCaption, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 10);

  motionTapOverlay = lv_obj_create(screen);
  lv_obj_set_size(motionTapOverlay, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_set_style_bg_opa(motionTapOverlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(motionTapOverlay, 0, 0);
  lv_obj_set_style_outline_width(motionTapOverlay, 0, 0);
  lv_obj_set_style_pad_all(motionTapOverlay, 0, 0);
  lv_obj_clear_flag(motionTapOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(motionTapOverlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_move_foreground(motionTapOverlay);

  screenRoots[static_cast<size_t>(ScreenId::Motion)] = screen;
  motionBuilt = true;
}
} // namespace

// --- Public motion view control (called from main.cpp) ---

MotionViewMode motionGetViewMode()
{
  return motionViewMode;
}

void motionSetViewMode(MotionViewMode mode)
{
  (void)mode;
  motionViewMode = kStandaloneMotionView;
}

void motionAdvanceView()
{
}

void motionReverseView()
{
}

void motionCenterDot()
{
  if (!motionDot) {
    return;
  }

  if (motionDotBoundary) {
    lv_obj_align_to(motionDot, motionDotBoundary, LV_ALIGN_CENTER, 0, 0);
    return;
  }

  lv_obj_align(motionDot, LV_ALIGN_CENTER, 0, 0);
}

// --- Screen module accessors ---

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

  return motionBuilt && waveformMotionScreenRoot() && motionScreen && motionDotView && motionCubeView && motionRawView;
}

bool waveformRefreshMotionScreen()
{
  if (!motionBuilt || !motionScreen || !motionDotView || !motionCubeView || !motionRawView) {
    return false;
  }

  refreshMotionScreen();
  return true;
}

void waveformEnterMotionScreen()
{
  motionViewMode = kStandaloneMotionView;
  renderedMotionTransitionDirection = 0;
  imuModuleCaptureReference();
  motionCenterDot();
  showMotionViewInstant(motionViewMode);
}

void waveformLeaveMotionScreen()
{
}

void waveformTickMotionScreen(uint32_t nowMs)
{
  (void)nowMs;
  refreshMotionScreen();
}

const ScreenModule &motionScreenModule()
{
  return kModule;
}
