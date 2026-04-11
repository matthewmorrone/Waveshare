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

namespace
{
lv_obj_t *cubeScreen = nullptr;
lv_obj_t *cubeTapOverlay = nullptr;
lv_obj_t *cubeView = nullptr;
lv_obj_t *cubeEdges[kCubeEdgeCount] = {nullptr};
lv_obj_t *cubeArrows[kCubeArrowLineCount] = {nullptr};
lv_point_precise_t cubeEdgePoints[kCubeEdgeCount][2] = {};
lv_point_precise_t cubeArrowPoints[kCubeArrowLineCount][2] = {};
bool cubeBuilt = false;

const ScreenModule kModule = {
    ScreenId::Cube,
    "Cube",
    waveformBuildCubeScreen,
    waveformRefreshCubeScreen,
    waveformEnterCubeScreen,
    waveformLeaveCubeScreen,
    waveformTickCubeScreen,
    waveformCubeScreenRoot,
};

void setLinePoints(lv_point_precise_t points[2], float x1, float y1, float x2, float y2)
{
  points[0].x = x1;
  points[0].y = y1;
  points[1].x = x2;
  points[1].y = y2;
}

void cubeBasis(const MotionState &state, Vec3 &down, Vec3 &axisA, Vec3 &axisB)
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

void updateCube()
{
  const MotionState &state = imuModuleState();
  if (!state.valid || !cubeView) {
    return;
  }

  Vec3 down, axisA, axisB;
  cubeBasis(state, down, axisA, axisB);

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
    setLinePoints(cubeEdgePoints[i],
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
    setLinePoints(cubeArrowPoints[base], centerX, centerY, tipX, tipY);
    setLinePoints(cubeArrowPoints[base + 1], tipX, tipY, leftX, leftY);
    setLinePoints(cubeArrowPoints[base + 2], tipX, tipY, rightX, rightY);
  }

  lv_obj_invalidate(cubeView);
}

void buildCubeScreen()
{
  lv_obj_t *screen = lv_obj_create(nullptr);
  applyRootStyle(screen);
  cubeScreen = screen;

  cubeView = lv_obj_create(screen);
  applyRootStyle(cubeView);
  enableTapBubbling(cubeView);

  for (size_t i = 0; i < kCubeEdgeCount; ++i) {
    cubeEdges[i] = lv_line_create(cubeView);
    lv_obj_set_size(cubeEdges[i], LCD_WIDTH, LCD_HEIGHT);
    styleLine(cubeEdges[i], lvColor(88, 96, 110), 2);
    lv_line_set_points(cubeEdges[i], cubeEdgePoints[i], 2);
  }

  lv_color_t arrowColors[kCubeArrowCount] = {
      lvColor(46, 142, 255),
      lvColor(255, 255, 255),
      lvColor(140, 154, 176),
  };
  for (size_t i = 0; i < kCubeArrowLineCount; ++i) {
    cubeArrows[i] = lv_line_create(cubeView);
    lv_obj_set_size(cubeArrows[i], LCD_WIDTH, LCD_HEIGHT);
    styleLine(cubeArrows[i], arrowColors[i / kCubeArrowSegmentCount], 3);
    lv_line_set_points(cubeArrows[i], cubeArrowPoints[i], 2);
  }

  cubeTapOverlay = lv_obj_create(screen);
  applyRootStyle(cubeTapOverlay);
  enableTapBubbling(cubeTapOverlay);
  lv_obj_move_foreground(cubeTapOverlay);

  screenRoots[static_cast<size_t>(ScreenId::Cube)] = screen;
  cubeBuilt = true;
}
} // namespace

void cubeHandleTap()
{
  imuModuleCaptureReference();
}

const ScreenModule &cubeScreenModule()
{
  return kModule;
}

lv_obj_t *waveformCubeScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Cube)];
}

lv_obj_t *waveformCubeTapTarget()
{
  return cubeTapOverlay ? cubeTapOverlay : waveformCubeScreenRoot();
}

bool waveformBuildCubeScreen()
{
  if (!waveformCubeScreenRoot()) {
    buildCubeScreen();
  }

  return cubeBuilt && waveformCubeScreenRoot() && cubeView;
}

bool waveformRefreshCubeScreen()
{
  if (!cubeBuilt || !cubeScreen) {
    return false;
  }

  updateCube();
  return true;
}

void waveformEnterCubeScreen()
{
  imuModuleCaptureReference();
  updateCube();
}

void waveformLeaveCubeScreen()
{
}

void waveformTickCubeScreen(uint32_t nowMs)
{
  LV_UNUSED(nowMs);
  updateCube();
}
