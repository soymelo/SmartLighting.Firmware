#pragma once

#include <cstdint>
#include <ctime>

#include "../Models.h"

// Conta ativações por zona e mantém as duas últimas janelas completas de 15 min.
class ZoneActivityCounter {
 public:
  enum class WindowChange : uint8_t {
    None, Started, PartialDiscarded, Completed, ClockReset, ClockLost
  };

  void reset(const TopologyConfig& topology);
  // Deve ser chamado antes de registrar as ativações do instante atual.
  WindowChange advance(std::time_t utcNow);
  // O chamador deve invocar este método uma vez por início de detecção.
  // Retorna os índices das zonas afetadas como bits.
  uint16_t record(uint8_t controlSensorIndex);
  uint32_t total(uint8_t zoneIndex) const;
  uint32_t lastClosedCount(uint8_t zoneIndex) const;
  bool previous30mCount(uint8_t zoneIndex, uint32_t& output) const;
  std::time_t currentWindowStartUtc() const;
  bool currentWindowIsComplete() const;

 private:
  void clearWindows();

  uint8_t zoneCount_ = 0;
  uint8_t controlSensorCount_ = 0;
  uint8_t sensorMasks_[TopologyConfig::MaxZones] = {};
  uint32_t totals_[TopologyConfig::MaxZones] = {};
  uint32_t currentCounts_[TopologyConfig::MaxZones] = {};
  uint32_t lastCounts_[TopologyConfig::MaxZones] = {};
  uint32_t previousCounts_[TopologyConfig::MaxZones] = {};
  uint8_t completedWindows_ = 0;
  std::time_t windowStartUtc_ = 0;
  std::time_t lastObservedUtc_ = 0;
  bool currentWindowComplete_ = false;
};
