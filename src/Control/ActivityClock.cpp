#include "ActivityClock.h"

namespace {
constexpr std::time_t EarliestValidUtc = 1735689600;  // 2025-01-01.
constexpr int LocalUtcOffsetSeconds = -3 * 60 * 60;
}

bool ActivityClock::validUtc(std::time_t utc) {
  return utc >= EarliestValidUtc;
}

bool ActivityClock::fromUtc(std::time_t utc, LocalTime& output) {
  if (!validUtc(utc)) return false;

  const std::time_t localEpoch = utc + LocalUtcOffsetSeconds;
  std::tm local{};
  if (gmtime_r(&localEpoch, &local) == nullptr) return false;

  output.hour = static_cast<uint8_t>(local.tm_hour);
  output.weekday = static_cast<uint8_t>((local.tm_wday + 6) % 7);
  return true;
}
