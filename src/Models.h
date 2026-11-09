#pragma once

#include <Arduino.h>

struct SensorConfig {
  char id[37] = {};
  char zoneId[37] = {};
  uint8_t slot = 0;
  uint8_t gpio = 0;
};

struct LuminaireConfig {
  char id[37] = {};
  uint8_t slot = 0;
  uint8_t gpio = 0;
};

struct ControlSensorConfig {
  char id[37] = {};
  char deviceId[37] = {};
  uint8_t slot = 0;
};

struct ZoneConfig {
  char zoneId[37] = {};
  // Bits correspondem aos índices de controlSensors, inclusive da outra placa.
  uint8_t controlSensorMask = 0;
  // Bits 0–5 correspondem aos slots 1–6 dos LEDs desta placa.
  uint8_t localLuminaireMask = 0;
};

struct TopologyConfig {
  static constexpr uint8_t MaxZones = 12;
  char deviceId[37] = {};
  SensorConfig sensors[3] = {};
  LuminaireConfig luminaires[6] = {};
  ControlSensorConfig controlSensors[6] = {};
  uint8_t luminaireSensorMask[7] = {};
  uint8_t sensorCount = 0;
  uint8_t luminaireCount = 0;
  uint8_t controlSensorCount = 0;
  ZoneConfig zones[MaxZones] = {};
  uint8_t zoneCount = 0;
};
