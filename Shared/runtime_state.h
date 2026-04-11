#pragma once

#include <Arduino.h>

enum class ConnectivityState : uint8_t
{
  Offline,
  Connecting,
  Online,
};

struct MotionState
{
  bool valid = false;
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  float gx = 0.0f;
  float gy = 0.0f;
  float gz = 0.0f;
  float pitch = 0.0f;
  float roll = 0.0f;
  char orientation[20] = "Unavailable";
};

struct Vec3
{
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};
