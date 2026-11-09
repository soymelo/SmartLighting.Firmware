#include "ZoneActivityCounter.h"

#include <cstdint>

#include "ActivityClock.h"

namespace {
constexpr std::time_t WindowSeconds = 15 * 60;
}

void ZoneActivityCounter::reset(const TopologyConfig& topology) {
  zoneCount_ = topology.zoneCount;
  controlSensorCount_ = topology.controlSensorCount;
  for (uint8_t index = 0; index < TopologyConfig::MaxZones; ++index) {
    sensorMasks_[index] = index < zoneCount_
                              ? topology.zones[index].controlSensorMask : 0;
    totals_[index] = 0;
  }
  clearWindows();
}

void ZoneActivityCounter::clearWindows() {
  windowStartUtc_ = 0;
  lastObservedUtc_ = 0;
  currentWindowComplete_ = false;
  completedWindows_ = 0;
  for (uint8_t index = 0; index < TopologyConfig::MaxZones; ++index) {
    currentCounts_[index] = 0;
    lastCounts_[index] = 0;
    previousCounts_[index] = 0;
  }
}

ZoneActivityCounter::WindowChange ZoneActivityCounter::advance(std::time_t utcNow) {
  if (!ActivityClock::validUtc(utcNow)) {
    if (windowStartUtc_ == 0) return WindowChange::None;
    clearWindows();
    return WindowChange::ClockLost;
  }

  const std::time_t start = utcNow - utcNow % WindowSeconds;
  if (windowStartUtc_ == 0) {
    windowStartUtc_ = start;
    lastObservedUtc_ = utcNow;
    currentWindowComplete_ = utcNow == start;
    return WindowChange::Started;
  }

  if (utcNow < lastObservedUtc_ || start < windowStartUtc_ ||
      start - windowStartUtc_ > WindowSeconds) {
    clearWindows();
    windowStartUtc_ = start;
    lastObservedUtc_ = utcNow;
    currentWindowComplete_ = utcNow == start;
    return WindowChange::ClockReset;
  }
  lastObservedUtc_ = utcNow;
  if (start == windowStartUtc_) return WindowChange::None;

  const bool completed = currentWindowComplete_;
  if (completed) {
    for (uint8_t index = 0; index < zoneCount_; ++index) {
      previousCounts_[index] = lastCounts_[index];
      lastCounts_[index] = currentCounts_[index];
    }
    if (completedWindows_ < 2) ++completedWindows_;
  } else {
    // Depois de uma janela parcial, as janelas guardadas deixam de ser
    // consecutivas e não podem representar os 30 minutos anteriores.
    completedWindows_ = 0;
    for (uint8_t index = 0; index < zoneCount_; ++index) {
      lastCounts_[index] = 0;
      previousCounts_[index] = 0;
    }
  }
  for (uint8_t index = 0; index < zoneCount_; ++index) currentCounts_[index] = 0;
  windowStartUtc_ = start;
  currentWindowComplete_ = utcNow == start;
  return completed ? WindowChange::Completed : WindowChange::PartialDiscarded;
}

uint16_t ZoneActivityCounter::record(uint8_t controlSensorIndex) {
  if (controlSensorIndex >= controlSensorCount_) return 0;

  uint16_t affectedZones = 0;
  for (uint8_t index = 0; index < zoneCount_; ++index) {
    if (!(sensorMasks_[index] & (1U << controlSensorIndex))) continue;
    if (totals_[index] < UINT32_MAX) ++totals_[index];
    if (currentWindowComplete_ && currentCounts_[index] < UINT32_MAX)
      ++currentCounts_[index];
    affectedZones |= static_cast<uint16_t>(1U << index);
  }
  return affectedZones;
}

uint32_t ZoneActivityCounter::total(uint8_t zoneIndex) const {
  return zoneIndex < zoneCount_ ? totals_[zoneIndex] : 0;
}

uint32_t ZoneActivityCounter::lastClosedCount(uint8_t zoneIndex) const {
  return zoneIndex < zoneCount_ && completedWindows_ > 0
             ? lastCounts_[zoneIndex] : 0;
}

bool ZoneActivityCounter::previous30mCount(uint8_t zoneIndex,
                                           uint32_t& output) const {
  if (zoneIndex >= zoneCount_ || completedWindows_ < 2) return false;
  const uint64_t sum = static_cast<uint64_t>(previousCounts_[zoneIndex]) +
                       lastCounts_[zoneIndex];
  output = sum > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(sum);
  return true;
}

std::time_t ZoneActivityCounter::currentWindowStartUtc() const {
  return windowStartUtc_;
}

bool ZoneActivityCounter::currentWindowIsComplete() const {
  return currentWindowComplete_;
}
