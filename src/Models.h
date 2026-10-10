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

struct TopologyConfig {
  char deviceId[37] = {};
  SensorConfig sensors[3] = {};
  LuminaireConfig luminaires[6] = {};
  ControlSensorConfig controlSensors[6] = {};
  uint8_t luminaireSensorMask[7] = {};
  uint8_t sensorCount = 0;
  uint8_t luminaireCount = 0;
  uint8_t controlSensorCount = 0;
};
