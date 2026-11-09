#include "ZoneActivityCounter.h"

#include <cstdint>

void ZoneActivityCounter::reset(const TopologyConfig& topology) {
  zoneCount_ = topology.zoneCount;
  controlSensorCount_ = topology.controlSensorCount;
  for (uint8_t index = 0; index < TopologyConfig::MaxZones; ++index) {
    sensorMasks_[index] = index < zoneCount_
                              ? topology.zones[index].controlSensorMask : 0;
    totals_[index] = 0;
  }
}

uint16_t ZoneActivityCounter::record(uint8_t controlSensorIndex) {
  if (controlSensorIndex >= controlSensorCount_) return 0;

  uint16_t affectedZones = 0;
  for (uint8_t index = 0; index < zoneCount_; ++index) {
    if (!(sensorMasks_[index] & (1U << controlSensorIndex))) continue;
    if (totals_[index] < UINT32_MAX) ++totals_[index];
    affectedZones |= static_cast<uint16_t>(1U << index);
  }
  return affectedZones;
}

uint32_t ZoneActivityCounter::total(uint8_t zoneIndex) const {
  return zoneIndex < zoneCount_ ? totals_[zoneIndex] : 0;
}
