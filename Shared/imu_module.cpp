#include "imu_module.h"

#include "pin_config.h"
#include "screen_constants.h"

#include <SensorQMI8658.hpp>
#include <Wire.h>

// Forward declarations for main.cpp globals
void noteActivity();
extern bool inLightSleep;

namespace
{
SensorQMI8658 imu;
MotionState motionState;
MotionState motionDisplayState;
Vec3 motionReferenceDown = {0.0f, 0.0f, 1.0f};
Vec3 motionReferenceAxisA = {1.0f, 0.0f, 0.0f};
Vec3 motionReferenceAxisB = {0.0f, 1.0f, 0.0f};

bool imuReady = false;
bool referenceReady = false;
bool referenceCapturePending = false;
bool displayValid = false;
bool filterReady = false;
bool haveLastAccelSample = false;
float lastAccelX = 0.0f;
float lastAccelY = 0.0f;
float lastAccelZ = 0.0f;
float dotPitch = 0.0f;
float dotRoll = 0.0f;
uint32_t lastImuSampleAtMs = 0;

void remapImuAxes(float rawAx, float rawAy, float rawAz,
                  float rawGx, float rawGy, float rawGz,
                  float &ax, float &ay, float &az,
                  float &gx, float &gy, float &gz)
{
  ax = -rawAy;
  ay = -rawAx;
  az = -rawAz;
  gx = -rawGy;
  gy = -rawGx;
  gz = -rawGz;
}

float blendMotionValue(float current, float target, float alpha)
{
  return current + ((target - current) * alpha);
}

void setOrientationLabel(float ax, float ay, float az)
{
  float absX = fabsf(ax);
  float absY = fabsf(ay);
  float absZ = fabsf(az);

  const char *label = "Moving";
  if (absZ >= absX && absZ >= absY) {
    label = az >= 0.0f ? "Face Up" : "Face Down";
  } else if (absX >= absY) {
    label = ax >= 0.0f ? "Left Up" : "Right Up";
  } else {
    label = ay >= 0.0f ? "Bottom Up" : "Top Up";
  }

  snprintf(motionState.orientation, sizeof(motionState.orientation), "%s", label);
}

bool initImu()
{
  imuReady = imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (!imuReady) {
    Serial.println("QMI8658 not found");
    motionState.valid = false;
    return false;
  }

  if (!imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G,
                               SensorQMI8658::ACC_ODR_125Hz,
                               SensorQMI8658::LPF_MODE_0)) {
    Serial.println("QMI8658 accelerometer configuration failed");
    imuReady = false;
    motionState.valid = false;
    return false;
  }

  imu.configGyroscope(SensorQMI8658::GYR_RANGE_512DPS,
                      SensorQMI8658::GYR_ODR_112_1Hz,
                      SensorQMI8658::LPF_MODE_3);

  if (!imu.enableAccelerometer()) {
    Serial.println("QMI8658 accelerometer enable failed");
    imuReady = false;
    motionState.valid = false;
    return false;
  }

  imu.enableGyroscope();
  pinMode(IMU_IRQ, INPUT_PULLUP);

  motionState.valid = false;
  displayValid = false;
  filterReady = false;
  haveLastAccelSample = false;
  lastImuSampleAtMs = 0;
  return true;
}
} // namespace

bool imuModuleInit()
{
  return initImu();
}

void imuModuleUpdate()
{
  if (!imuReady || inLightSleep) {
    return;
  }

  uint32_t now = millis();
  if (now - lastImuSampleAtMs < kImuSampleIntervalMs) {
    return;
  }
  lastImuSampleAtMs = now;

  if (!imu.getDataReady()) {
    return;
  }

  float ax = 0.0f, ay = 0.0f, az = 0.0f;
  if (!imu.getAccelerometer(ax, ay, az)) {
    return;
  }
  float rawAx = ax, rawAy = ay, rawAz = az;

  float gx = 0.0f, gy = 0.0f, gz = 0.0f;
  imu.getGyroscope(gx, gy, gz);
  float rawGx = gx, rawGy = gy, rawGz = gz;

  remapImuAxes(rawAx, rawAy, rawAz, rawGx, rawGy, rawGz, ax, ay, az, gx, gy, gz);

  if (motionState.valid) {
    ax = blendMotionValue(motionState.ax, ax, kMotionSensorSmoothingAlpha);
    ay = blendMotionValue(motionState.ay, ay, kMotionSensorSmoothingAlpha);
    az = blendMotionValue(motionState.az, az, kMotionSensorSmoothingAlpha);
    gx = blendMotionValue(motionState.gx, gx, kMotionSensorSmoothingAlpha);
    gy = blendMotionValue(motionState.gy, gy, kMotionSensorSmoothingAlpha);
    gz = blendMotionValue(motionState.gz, gz, kMotionSensorSmoothingAlpha);
  }

  motionState.valid = true;
  motionState.ax = ax;
  motionState.ay = ay;
  motionState.az = az;
  motionState.gx = gx;
  motionState.gy = gy;
  motionState.gz = gz;
  motionState.pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / PI;
  motionState.roll = atan2f(ay, az) * 180.0f / PI;
  setOrientationLabel(ax, ay, az);

  if (!displayValid) {
    motionDisplayState = motionState;
    displayValid = true;
  } else {
    motionDisplayState.ax = blendMotionValue(motionDisplayState.ax, motionState.ax, kMotionReadoutSmoothingAlpha);
    motionDisplayState.ay = blendMotionValue(motionDisplayState.ay, motionState.ay, kMotionReadoutSmoothingAlpha);
    motionDisplayState.az = blendMotionValue(motionDisplayState.az, motionState.az, kMotionReadoutSmoothingAlpha);
    motionDisplayState.gx = blendMotionValue(motionDisplayState.gx, motionState.gx, kMotionReadoutSmoothingAlpha);
    motionDisplayState.gy = blendMotionValue(motionDisplayState.gy, motionState.gy, kMotionReadoutSmoothingAlpha);
    motionDisplayState.gz = blendMotionValue(motionDisplayState.gz, motionState.gz, kMotionReadoutSmoothingAlpha);
    motionDisplayState.pitch = blendMotionValue(motionDisplayState.pitch, motionState.pitch, kMotionReadoutSmoothingAlpha);
    motionDisplayState.roll = blendMotionValue(motionDisplayState.roll, motionState.roll, kMotionReadoutSmoothingAlpha);
    motionDisplayState.valid = true;
    snprintf(motionDisplayState.orientation, sizeof(motionDisplayState.orientation), "%s", motionState.orientation);
  }

  if (!haveLastAccelSample) {
    lastAccelX = rawAx;
    lastAccelY = rawAy;
    lastAccelZ = rawAz;
    haveLastAccelSample = true;
    return;
  }

  float delta = fabsf(rawAx - lastAccelX) + fabsf(rawAy - lastAccelY) + fabsf(rawAz - lastAccelZ);
  lastAccelX = rawAx;
  lastAccelY = rawAy;
  lastAccelZ = rawAz;
  if (delta >= kMotionDeltaThreshold) {
    noteActivity();
  }
}

void imuModuleCaptureReference()
{
  dotPitch = 0.0f;
  dotRoll = 0.0f;
  filterReady = false;
  referenceReady = false;
  referenceCapturePending = true;
}

const MotionState &imuModuleState()
{
  return motionState;
}

const MotionState &imuModuleDisplayState()
{
  return motionDisplayState;
}

bool imuModuleIsReady()
{
  return imuReady;
}

Vec3 imuModuleReferenceDown()
{
  return motionReferenceDown;
}

Vec3 imuModuleReferenceAxisA()
{
  return motionReferenceAxisA;
}

Vec3 imuModuleReferenceAxisB()
{
  return motionReferenceAxisB;
}

bool imuModuleReferenceReady()
{
  return referenceReady;
}

bool imuModuleReferenceCapturePending()
{
  return referenceCapturePending;
}

bool imuModuleDisplayValid()
{
  return displayValid;
}

bool imuModuleFilterReady()
{
  return filterReady;
}

float imuModuleDotPitch()
{
  return dotPitch;
}

float imuModuleDotRoll()
{
  return dotRoll;
}

void imuModuleSetDotPitch(float v)
{
  dotPitch = v;
}

void imuModuleSetDotRoll(float v)
{
  dotRoll = v;
}

void imuModuleSetFilterReady(bool v)
{
  filterReady = v;
}

void imuModuleSetReferenceReady(bool v)
{
  referenceReady = v;
}

void imuModuleSetReferenceCapturePending(bool v)
{
  referenceCapturePending = v;
}

void imuModuleSetReferenceDown(const Vec3 &v)
{
  motionReferenceDown = v;
}

void imuModuleSetReferenceAxisA(const Vec3 &v)
{
  motionReferenceAxisA = v;
}

void imuModuleSetReferenceAxisB(const Vec3 &v)
{
  motionReferenceAxisB = v;
}

bool imuModuleConfigureWakeOnMotion()
{
  if (!imuReady) {
    return false;
  }

  if (!imu.configWakeOnMotion(100,
                              SensorQMI8658::ACC_ODR_LOWPOWER_128Hz,
                              SensorQMI8658::INTERRUPT_PIN_2)) {
    Serial.println("QMI8658 wake-on-motion configuration failed");
    return false;
  }

  pinMode(IMU_IRQ, INPUT_PULLUP);
  return true;
}
