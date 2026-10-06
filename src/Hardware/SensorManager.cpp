#include "SensorManager.h"

namespace {
SensorConfig sensors[3];
uint8_t count = 0;
bool active[4] = {};
}

void SensorManager::begin(const TopologyConfig& topology) {
  count = topology.sensorCount;
  for (uint8_t slot = 1; slot <= 3; ++slot) active[slot] = false;
  for (uint8_t index = 0; index < count; ++index) {
    sensors[index] = topology.sensors[index];
    pinMode(sensors[index].gpio, INPUT);
  }
}

uint8_t SensorManager::update() {
  uint8_t detections = 0;
  for (uint8_t index = 0; index < count; ++index) {
    const uint8_t slot = sensors[index].slot;
    const bool detected = digitalRead(sensors[index].gpio) == LOW;
    if (detected && !active[slot]) detections |= 1U << (slot - 1);
    active[slot] = detected;
  }
  return detections;
}

bool SensorManager::isActive(uint8_t slot) {
  return slot >= 1 && slot <= 3 && active[slot];
}
