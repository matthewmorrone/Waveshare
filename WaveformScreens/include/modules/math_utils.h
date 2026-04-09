#pragma once

#include "state/runtime_state.h"
#include <Arduino.h>
#include <time.h>

// --- Vec3 operations ---
Vec3 vec3(float x, float y, float z);
Vec3 addVec3(const Vec3 &a, const Vec3 &b);
Vec3 subtractVec3(const Vec3 &a, const Vec3 &b);
Vec3 scaleVec3(const Vec3 &value, float scalar);
float lengthVec3(const Vec3 &value);
Vec3 normalizeVec3(const Vec3 &value);
Vec3 crossVec3(const Vec3 &a, const Vec3 &b);
float dotVec3(const Vec3 &a, const Vec3 &b);
Vec3 projectOntoPlane(const Vec3 &vector, const Vec3 &normal);
Vec3 stablePerpendicular(const Vec3 &base, const Vec3 &fallback);
Vec3 normalizedDownVector(const MotionState &state);

// --- Angle math ---
float degreesToRadians(float degrees);
float radiansToDegrees(float radians);
float normalizeDegrees(float degrees);

// --- Astronomy ---
double julianDateFromUnix(time_t unixTime);
double localSiderealTimeDegrees(time_t unixTime, float longitudeDegrees);
bool equatorialToHorizontal(float raHours,
                            float decDegrees,
                            float latitudeDegrees,
                            float longitudeDegrees,
                            time_t unixTime,
                            float &altitudeDegrees,
                            float &azimuthDegrees);

// --- Orbital ---
float compressedOrbitRadius(float distanceAu);
bool parseHorizonsVectorRow(const String &result, float &xAu, float &yAu, float &zAu);
