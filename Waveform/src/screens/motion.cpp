#include "config/screen_constants.h"
#include "core/screen_manager.h"
#include "screens/screen_callbacks.h"

extern lv_obj_t *screenRoots[];
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
void enableTapBubbling(lv_obj_t *obj);
void styleLine(lv_obj_t *line, lv_color_t color, int width);
void captureMotionReference();
void motionCubeBasis(const MotionState &state, Vec3 &down, Vec3 &axisA, Vec3 &axisB);
Vec3 vec3(float x, float y, float z);
Vec3 normalizedDownVector(const MotionState &state);
Vec3 stablePerpendicular(const Vec3 &base, const Vec3 &fallback);
Vec3 normalizeVec3(const Vec3 &value);
Vec3 crossVec3(const Vec3 &a, const Vec3 &b);
float dotVec3(const Vec3 &a, const Vec3 &b);
Vec3 subtractVec3(const Vec3 &a, const Vec3 &b);
Vec3 scaleVec3(const Vec3 &value, float scalar);
float lengthVec3(const Vec3 &value);
float normalizeMotionIndicatorOffset(float offset);
void showMotionViewInstant(MotionViewMode mode);
void centerMotionDot();

extern bool motionBuilt;
extern bool touchPressed;
extern bool motionReferenceReady;
extern bool motionReferenceCapturePending;
extern bool motionFilterReady;
extern MotionViewMode motionViewMode;
extern MotionViewMode renderedMotionViewMode;
extern int renderedMotionTransitionDirection;
extern MotionState motionState;
extern MotionState motionDisplayState;
extern Vec3 motionReferenceDown;
extern Vec3 motionReferenceAxisA;
extern Vec3 motionReferenceAxisB;
extern float motionPitchZero;
extern float motionRollZero;
extern float motionDotPitch;
extern float motionDotRoll;

extern lv_obj_t *motionScreen;
extern lv_obj_t *motionTapOverlay;
extern lv_obj_t *motionDotView;
extern lv_obj_t *motionDotBoundary;
extern lv_obj_t *motionDotCrossH;
extern lv_obj_t *motionDotCrossV;
extern lv_obj_t *motionDot;
extern lv_obj_t *motionCubeView;
extern lv_obj_t *motionRawView;
extern lv_obj_t *motionRawCard;
extern lv_obj_t *motionRawOrientation;
extern lv_obj_t *motionRawPitch;
extern lv_obj_t *motionRawRoll;
extern lv_obj_t *motionRawAccel;
extern lv_obj_t *motionRawGyro;
extern lv_obj_t *motionRawPitchCaption;
extern lv_obj_t *motionRawRollCaption;
extern lv_obj_t *motionRawAccelCaption;
extern lv_obj_t *motionRawGyroCaption;

extern lv_obj_t *motionCubeEdges[kCubeEdgeCount];
extern lv_obj_t *motionCubeArrows[kCubeArrowLineCount];
extern lv_point_precise_t motionCubeEdgePoints[kCubeEdgeCount][2];
extern lv_point_precise_t motionCubeArrowPoints[kCubeArrowLineCount][2];

namespace
{
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
} // namespace

const ScreenModule &motionScreenModule()
{
  return kModule;
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
  lv_obj_move_foreground(motionTapOverlay);

  screenRoots[static_cast<size_t>(ScreenId::Motion)] = screen;
  motionBuilt = true;
}

void setMotionViewVisibility()
{
  if (renderedMotionViewMode != motionViewMode) {
    showMotionViewInstant(motionViewMode);
  }
}

void updateMotionDot()
{
  if (!motionState.valid) {
    return;
  }

  if (!motionReferenceReady) {
    if (!motionReferenceCapturePending) {
      captureMotionReference();
    }
  }

  const MotionState &displayState = motionState;
  Vec3 currentDown = normalizedDownVector(displayState);

  lv_area_t boundary = {};
  lv_obj_get_coords(motionDotBoundary, &boundary);
  int centerX = (boundary.x1 + boundary.x2) / 2;
  int centerY = (boundary.y1 + boundary.y2) / 2;

  if (motionReferenceCapturePending) {
    motionPitchZero = displayState.pitch;
    motionRollZero = displayState.roll;
    motionReferenceDown = currentDown;
    motionReferenceAxisA = stablePerpendicular(motionReferenceDown, vec3(1.0f, 0.0f, 0.0f));
    motionReferenceAxisB = normalizeVec3(crossVec3(motionReferenceDown, motionReferenceAxisA));
    if (lengthVec3(motionReferenceAxisB) < 0.1f) {
      motionReferenceAxisB = stablePerpendicular(motionReferenceDown, vec3(0.0f, 1.0f, 0.0f));
    }
    motionReferenceReady = true;
    motionReferenceCapturePending = false;
    motionDotPitch = 0.0f;
    motionDotRoll = 0.0f;
    motionFilterReady = true;
    lv_obj_set_pos(motionDot, centerX - (kDotDiameter / 2), centerY - (kDotDiameter / 2));
    return;
  }

  if (!motionReferenceReady) {
    return;
  }

  Vec3 downDelta = subtractVec3(currentDown, motionReferenceDown);
  float horizontalOffset = dotVec3(downDelta, motionReferenceAxisA);
  float verticalOffset = dotVec3(downDelta, motionReferenceAxisB);

  if (!motionFilterReady) {
    motionDotPitch = verticalOffset;
    motionDotRoll = horizontalOffset;
    motionFilterReady = true;
  } else {
    motionDotPitch += (verticalOffset - motionDotPitch) * kMotionIndicatorSmoothingAlpha;
    motionDotRoll += (horizontalOffset - motionDotRoll) * kMotionIndicatorSmoothingAlpha;
  }

  int travel = ((boundary.x2 - boundary.x1) / 2) - 22;
  float normalizedX = normalizeMotionIndicatorOffset(motionDotRoll);
  float normalizedY = normalizeMotionIndicatorOffset(motionDotPitch);
  int x = centerX + static_cast<int>(normalizedX * travel) - (kDotDiameter / 2);
  int y = centerY - static_cast<int>(normalizedY * travel) - (kDotDiameter / 2);
  x = constrain(x, 0, LCD_WIDTH - kDotDiameter);
  y = constrain(y, 0, LCD_HEIGHT - kDotDiameter);
  lv_obj_set_pos(motionDot, x, y);
}

void updateMotionRaw()
{
  if (!motionDisplayState.valid) {
    lv_label_set_text(motionRawOrientation, "Unavailable");
    lv_label_set_text(motionRawPitch, "--.-");
    lv_label_set_text(motionRawRoll, "--.-");
    lv_label_set_text(motionRawAccel, "X  --.-\nY  --.-\nZ  --.-");
    lv_label_set_text(motionRawGyro, "X  ---\nY  ---\nZ  ---");
    return;
  }

  char line[80];
  lv_label_set_text(motionRawOrientation, motionDisplayState.orientation);

  snprintf(line, sizeof(line), "%+.0f\xc2\xb0", motionDisplayState.pitch);
  lv_label_set_text(motionRawPitch, line);
  snprintf(line, sizeof(line), "%+.0f\xc2\xb0", motionDisplayState.roll);
  lv_label_set_text(motionRawRoll, line);
  snprintf(line,
           sizeof(line),
           "X  %+0.1f\nY  %+0.1f\nZ  %+0.1f",
           motionDisplayState.ax,
           motionDisplayState.ay,
           motionDisplayState.az);
  lv_label_set_text(motionRawAccel, line);
  snprintf(line,
           sizeof(line),
           "X  %+0.0f\nY  %+0.0f\nZ  %+0.0f",
           motionDisplayState.gx,
           motionDisplayState.gy,
           motionDisplayState.gz);
  lv_label_set_text(motionRawGyro, line);
}

void setLinePoints(lv_point_precise_t points[2], float x1, float y1, float x2, float y2)
{
  points[0].x = x1;
  points[0].y = y1;
  points[1].x = x2;
  points[1].y = y2;
}

void updateMotionCube()
{
  if (!motionDisplayState.valid) {
    return;
  }

  Vec3 down;
  Vec3 axisA;
  Vec3 axisB;
  motionCubeBasis(motionDisplayState, down, axisA, axisB);

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
                  projected[edges[i][0]].x,
                  projected[edges[i][0]].y,
                  projected[edges[i][1]].x,
                  projected[edges[i][1]].y);
    lv_obj_invalidate(motionCubeEdges[i]);
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
    lv_obj_invalidate(motionCubeArrows[base]);
    lv_obj_invalidate(motionCubeArrows[base + 1]);
    lv_obj_invalidate(motionCubeArrows[base + 2]);
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

lv_obj_t *waveformMotionScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Motion)];
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
  renderedMotionViewMode = MotionViewMode::Count;
  renderedMotionTransitionDirection = 0;
  captureMotionReference();
  centerMotionDot();
}

void waveformLeaveMotionScreen()
{
}

void waveformTickMotionScreen(uint32_t nowMs)
{
  (void)nowMs;
  refreshMotionScreen();
}
