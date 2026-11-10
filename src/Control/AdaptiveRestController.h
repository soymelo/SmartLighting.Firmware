#pragma once

#include <cstdint>
#include <ctime>

#include "../Models.h"
#include "ZoneActivityCounter.h"

// Traduz previsões por zona em um repouso para cada LED local.
class AdaptiveRestController {
 public:
  void reset();
  void update(const TopologyConfig& topology, const ZoneActivityCounter& history,
              std::time_t predictionAtUtc);
  uint8_t zonePwm(uint8_t zoneIndex) const;
  bool predicted(uint8_t zoneIndex) const;
  double probability(uint8_t zoneIndex) const;
  uint8_t luminairePwm(uint8_t slot) const;
  const uint8_t (&luminairePwms() const)[7];

 private:
  uint8_t zonePwm_[TopologyConfig::MaxZones] = {};
  double probabilities_[TopologyConfig::MaxZones] = {};
  uint16_t predictedZones_ = 0;
  uint8_t luminairePwm_[7] = {};
};
