#pragma once

#include <Arduino.h>

#include <sys/time.h>
#include <time.h>

namespace waveform
{
constexpr time_t kMinValidEpoch = 1704067200;

inline int monthFromBuildString(const char *month)
{
  if (strcmp(month, "Jan") == 0) return 0;
  if (strcmp(month, "Feb") == 0) return 1;
  if (strcmp(month, "Mar") == 0) return 2;
  if (strcmp(month, "Apr") == 0) return 3;
  if (strcmp(month, "May") == 0) return 4;
  if (strcmp(month, "Jun") == 0) return 5;
  if (strcmp(month, "Jul") == 0) return 6;
  if (strcmp(month, "Aug") == 0) return 7;
  if (strcmp(month, "Sep") == 0) return 8;
  if (strcmp(month, "Oct") == 0) return 9;
  if (strcmp(month, "Nov") == 0) return 10;
  if (strcmp(month, "Dec") == 0) return 11;
  return -1;
}

inline time_t buildTimestampEpoch()
{
  char month[4] = {};
  int day = 0;
  int year = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;

  if (sscanf(__DATE__, "%3s %d %d", month, &day, &year) != 3) {
    return 0;
  }
  if (sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second) != 3) {
    return 0;
  }

  int monthIndex = monthFromBuildString(month);
  if (monthIndex < 0) {
    return 0;
  }

  struct tm buildTm = {};
  buildTm.tm_year = year - 1900;
  buildTm.tm_mon = monthIndex;
  buildTm.tm_mday = day;
  buildTm.tm_hour = hour;
  buildTm.tm_min = minute;
  buildTm.tm_sec = second;
  buildTm.tm_isdst = -1;
  return mktime(&buildTm);
}

inline time_t minimumReasonableEpoch()
{
  time_t buildEpoch = buildTimestampEpoch();
  return buildEpoch > kMinValidEpoch ? buildEpoch : kMinValidEpoch;
}

inline bool rtcTimeLooksValid(const struct tm &timeInfo)
{
  int year = timeInfo.tm_year + 1900;
  return year >= 2024 && year <= 2099 &&
         timeInfo.tm_mon >= 0 && timeInfo.tm_mon <= 11 &&
         timeInfo.tm_mday >= 1 && timeInfo.tm_mday <= 31 &&
         timeInfo.tm_hour >= 0 && timeInfo.tm_hour <= 23 &&
         timeInfo.tm_min >= 0 && timeInfo.tm_min <= 59 &&
         timeInfo.tm_sec >= 0 && timeInfo.tm_sec <= 59;
}

inline bool setSystemTimeFromEpoch(time_t epoch)
{
  if (epoch < kMinValidEpoch) {
    return false;
  }

  timeval tv = {.tv_sec = epoch, .tv_usec = 0};
  return settimeofday(&tv, nullptr) == 0;
}

inline bool setSystemTimeFromTm(const struct tm &timeInfo)
{
  struct tm localTime = timeInfo;
  localTime.tm_isdst = -1;
  return setSystemTimeFromEpoch(mktime(&localTime));
}

inline bool hasValidTime()
{
  return time(nullptr) >= kMinValidEpoch;
}

inline bool hasReasonableTime()
{
  return time(nullptr) >= minimumReasonableEpoch();
}

inline bool ensureSystemTimeAtLeastBuildTimestamp()
{
  time_t buildEpoch = buildTimestampEpoch();
  if (buildEpoch < kMinValidEpoch) {
    return hasValidTime();
  }

  time_t now = time(nullptr);
  if (now >= buildEpoch) {
    return true;
  }

  return setSystemTimeFromEpoch(buildEpoch);
}

inline time_t effectiveNow()
{
  time_t now = time(nullptr);
  if (now >= minimumReasonableEpoch()) {
    return now;
  }

  time_t buildEpoch = buildTimestampEpoch();
  if (buildEpoch >= kMinValidEpoch) {
    return buildEpoch;
  }

  return now;
}

inline bool effectiveLocalTime(struct tm &localTime, time_t *epochOut = nullptr)
{
  time_t now = effectiveNow();
  if (now < kMinValidEpoch) {
    return false;
  }

  localtime_r(&now, &localTime);
  if (epochOut) {
    *epochOut = now;
  }
  return true;
}

inline String formatCurrentLocal(const char *format, const char *fallback)
{
  struct tm localTime = {};
  if (!effectiveLocalTime(localTime)) {
    return String(fallback);
  }

  char buffer[48];
  strftime(buffer, sizeof(buffer), format, &localTime);
  return String(buffer);
}

inline String formatTimeText(bool use24hClock)
{
  return formatCurrentLocal(use24hClock ? "%H:%M" : "%I:%M", "--:--");
}

inline String formatDateText()
{
  return formatCurrentLocal("%a, %b %d", "Waiting for RTC or Wi-Fi");
}

inline String formatTimezoneText()
{
  return formatCurrentLocal("%Z", "TIME UNAVAILABLE");
}
} // namespace waveform
