#include "SensorManager.h"

namespace {
constexpr unsigned long ActivationDebounceMs = 100;
constexpr unsigned long ReleaseDebounceMs = 1000;
SensorConfig sensors[3];
uint8_t count = 0;
bool active[4] = {};
bool rawActive[4] = {};
unsigned long rawChangedAt[4] = {};
}

void SensorManager::begin(const TopologyConfig& topology) {
  count = topology.sensorCount;
  for (uint8_t slot = 1; slot <= 3; ++slot) {
    active[slot] = false;
    rawActive[slot] = false;
    rawChangedAt[slot] = millis();
  }
  for (uint8_t index = 0; index < count; ++index) {
    sensors[index] = topology.sensors[index];
    pinMode(sensors[index].gpio, INPUT);
  }
}

uint8_t SensorManager::update() {
  uint8_t detections = 0;
  const unsigned long now = millis();
  for (uint8_t index = 0; index < count; ++index) {
    const uint8_t slot = sensors[index].slot;
    const bool detected = digitalRead(sensors[index].gpio) == LOW;
    if (detected != rawActive[slot]) {
      rawActive[slot] = detected;
      rawChangedAt[slot] = now;
    }
    const unsigned long debounceMs = detected ? ActivationDebounceMs
                                              : ReleaseDebounceMs;
    if (detected != active[slot] && now - rawChangedAt[slot] >= debounceMs) {
      active[slot] = detected;
      if (detected) detections |= 1U << (slot - 1);
    }
  }
  return detections;
}

bool SensorManager::isActive(uint8_t slot) {
  return slot >= 1 && slot <= 3 && active[slot];
}
