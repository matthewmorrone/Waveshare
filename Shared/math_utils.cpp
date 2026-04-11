#include "math_utils.h"

#include <math.h>
#include <string.h>

Vec3 vec3(float x, float y, float z)
{
  return {x, y, z};
}

Vec3 addVec3(const Vec3 &a, const Vec3 &b)
{
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 subtractVec3(const Vec3 &a, const Vec3 &b)
{
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 scaleVec3(const Vec3 &value, float scalar)
{
  return {value.x * scalar, value.y * scalar, value.z * scalar};
}

float lengthVec3(const Vec3 &value)
{
  return sqrtf(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vec3 normalizeVec3(const Vec3 &value)
{
  float length = lengthVec3(value);
  if (length < 0.0001f) {
    return {0.0f, 0.0f, 0.0f};
  }
  return {value.x / length, value.y / length, value.z / length};
}

Vec3 crossVec3(const Vec3 &a, const Vec3 &b)
{
  return {
      a.y * b.z - a.z * b.y,
      a.z * b.x - a.x * b.z,
      a.x * b.y - a.y * b.x,
  };
}

float dotVec3(const Vec3 &a, const Vec3 &b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 projectOntoPlane(const Vec3 &vector, const Vec3 &normal)
{
  return subtractVec3(vector, scaleVec3(normal, dotVec3(vector, normal)));
}

Vec3 stablePerpendicular(const Vec3 &base, const Vec3 &fallback)
{
  Vec3 perpendicular = normalizeVec3(projectOntoPlane(fallback, base));
  if (lengthVec3(perpendicular) >= 0.1f) {
    return perpendicular;
  }

  Vec3 alternate = fabsf(base.x) < 0.85f ? vec3(1.0f, 0.0f, 0.0f) : vec3(0.0f, 1.0f, 0.0f);
  perpendicular = normalizeVec3(projectOntoPlane(alternate, base));
  if (lengthVec3(perpendicular) >= 0.1f) {
    return perpendicular;
  }

  return {1.0f, 0.0f, 0.0f};
}

Vec3 normalizedDownVector(const MotionState &state)
{
  return normalizeVec3(vec3(state.ax, state.ay, state.az));
}

float degreesToRadians(float degrees)
{
  return degrees * (PI / 180.0f);
}

float radiansToDegrees(float radians)
{
  return radians * (180.0f / PI);
}

float normalizeDegrees(float degrees)
{
  while (degrees < 0.0f) {
    degrees += 360.0f;
  }
  while (degrees >= 360.0f) {
    degrees -= 360.0f;
  }
  return degrees;
}

double julianDateFromUnix(time_t unixTime)
{
  return (static_cast<double>(unixTime) / 86400.0) + 2440587.5;
}

double localSiderealTimeDegrees(time_t unixTime, float longitudeDegrees)
{
  double jd = julianDateFromUnix(unixTime);
  double t = (jd - 2451545.0) / 36525.0;
  double gmst = 280.46061837 +
                (360.98564736629 * (jd - 2451545.0)) +
                (0.000387933 * t * t) -
                (t * t * t / 38710000.0);
  return normalizeDegrees(static_cast<float>(gmst + longitudeDegrees));
}

bool equatorialToHorizontal(float raHours,
                            float decDegrees,
                            float latitudeDegrees,
                            float longitudeDegrees,
                            time_t unixTime,
                            float &altitudeDegrees,
                            float &azimuthDegrees)
{
  double raDegrees = static_cast<double>(raHours) * 15.0;
  double decRadians = degreesToRadians(decDegrees);
  double latRadians = degreesToRadians(latitudeDegrees);
  double hourAngleDegrees = localSiderealTimeDegrees(unixTime, longitudeDegrees) - raDegrees;
  while (hourAngleDegrees < -180.0) {
    hourAngleDegrees += 360.0;
  }
  while (hourAngleDegrees > 180.0) {
    hourAngleDegrees -= 360.0;
  }

  double hourAngleRadians = degreesToRadians(static_cast<float>(hourAngleDegrees));
  double sinAltitude = (sin(decRadians) * sin(latRadians)) +
                       (cos(decRadians) * cos(latRadians) * cos(hourAngleRadians));
  sinAltitude = fmax(-1.0, fmin(1.0, sinAltitude));
  double altitudeRadians = asin(sinAltitude);
  double cosAltitude = cos(altitudeRadians);

  if (fabs(cosAltitude) < 0.000001) {
    altitudeDegrees = 90.0f;
    azimuthDegrees = 0.0f;
    return true;
  }

  double cosAzimuth = (sin(decRadians) - (sin(altitudeRadians) * sin(latRadians))) /
                      (cosAltitude * cos(latRadians));
  cosAzimuth = fmax(-1.0, fmin(1.0, cosAzimuth));
  double azimuthRadians = acos(cosAzimuth);
  if (sin(hourAngleRadians) > 0.0) {
    azimuthRadians = (2.0 * PI) - azimuthRadians;
  }

  altitudeDegrees = radiansToDegrees(static_cast<float>(altitudeRadians));
  azimuthDegrees = radiansToDegrees(static_cast<float>(azimuthRadians));
  return true;
}

float compressedOrbitRadius(float distanceAu)
{
  return sqrtf(fmaxf(distanceAu, 0.0f));
}

bool parseHorizonsVectorRow(const String &result, float &xAu, float &yAu, float &zAu)
{
  int startIndex = result.indexOf("$$SOE");
  if (startIndex < 0) {
    return false;
  }

  startIndex = result.indexOf('\n', startIndex);
  if (startIndex < 0) {
    return false;
  }
  ++startIndex;

  int endIndex = result.indexOf('\n', startIndex);
  if (endIndex < 0) {
    return false;
  }

  String row = result.substring(startIndex, endIndex);
  row.trim();
  if (row.length() == 0) {
    return false;
  }

  float parsedX = 0.0f;
  float parsedY = 0.0f;
  float parsedZ = 0.0f;
  if (sscanf(row.c_str(), " %*[^,], %*[^,], %f, %f, %f", &parsedX, &parsedY, &parsedZ) != 3) {
    return false;
  }

  xAu = parsedX;
  yAu = parsedY;
  zAu = parsedZ;
  return true;
}
