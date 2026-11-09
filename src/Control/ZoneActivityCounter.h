#pragma once

#include <cstdint>

#include "../Models.h"

// Conta ativações novas desde a última aplicação da topologia. Ainda não
// separa as contagens em janelas de tempo.
class ZoneActivityCounter {
 public:
  void reset(const TopologyConfig& topology);
  // O chamador deve invocar este método uma vez por início de detecção.
  // Retorna os índices das zonas afetadas como bits.
  uint16_t record(uint8_t controlSensorIndex);
  uint32_t total(uint8_t zoneIndex) const;

 private:
  uint8_t zoneCount_ = 0;
  uint8_t controlSensorCount_ = 0;
  uint8_t sensorMasks_[TopologyConfig::MaxZones] = {};
  uint32_t totals_[TopologyConfig::MaxZones] = {};
};
