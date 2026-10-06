#include "LightingController.h"

#include "../../DeviceConfig.h"
#include "../Hardware/LuminaireManager.h"
#include "../Hardware/SensorManager.h"
#include "../Services/TelemetryService.h"
#include "../Services/TopologyService.h"

namespace {
constexpr unsigned long HoldMs = 7000;
TopologyConfig current;
bool configured = false;
unsigned long lastMotion[3] = {};
bool sawMotion[3] = {};
unsigned long lastRemoteMotion = 0;
bool sawRemoteMotion = false;
uint8_t transmitPin = 0;
uint8_t receivePin = 0;
uint8_t borderSensor = 0;
uint8_t borderPair = 0;

const char* sensorId(uint8_t slot) {
  for (uint8_t index = 0; index < current.sensorCount; ++index)
    if (current.sensors[index].slot == slot) return current.sensors[index].id;
  return nullptr;
}

const char* luminaireId(uint8_t slot) {
  for (uint8_t index = 0; index < current.luminaireCount; ++index)
    if (current.luminaires[index].slot == slot) return current.luminaires[index].id;
  return nullptr;
}
}

void LightingController::begin() {
  if (DeviceConfig::BoardNumber == 1) {
    transmitPin = 13;
    receivePin = 14;
    borderSensor = 3;
    borderPair = 3;
  } else if (DeviceConfig::BoardNumber == 2) {
    transmitPin = 14;
    receivePin = 13;
    borderSensor = 1;
    borderPair = 1;
  } else {
    Serial.println("BoardNumber nao configurado: comunicacao entre ESP32 desativada.");
    return;
  }
  pinMode(transmitPin, OUTPUT);
  digitalWrite(transmitPin, LOW);
  pinMode(receivePin, INPUT_PULLDOWN);
}

bool LightingController::applyTopology(const TopologyConfig& topology) {
  if (!LuminaireManager::begin(topology)) return false;
  SensorManager::begin(topology);
  current = topology;
  configured = true;
  for (uint8_t index = 0; index < 3; ++index) {
    lastMotion[index] = 0;
    sawMotion[index] = false;
  }
  sawRemoteMotion = false;
  TopologyService::print(current);
  return true;
}

void LightingController::update() {
  if (!configured) return;
  const unsigned long now = millis();
  const uint8_t detections = SensorManager::update();
  uint8_t activePairs = 0;
  for (uint8_t slot = 1; slot <= 3; ++slot) {
    if (detections & (1U << (slot - 1))) {
      const char* id = sensorId(slot);
      if (id != nullptr) TelemetryService::queueSensorEvent(id);
    }
    if (SensorManager::isActive(slot)) {
      lastMotion[slot - 1] = now;
      sawMotion[slot - 1] = true;
    }
    if (sawMotion[slot - 1] && now - lastMotion[slot - 1] < HoldMs)
      activePairs |= 1U << (slot - 1);
  }

  if (transmitPin != 0) {
    digitalWrite(transmitPin, SensorManager::isActive(borderSensor) ? HIGH : LOW);
    if (digitalRead(receivePin) == HIGH) {
      lastRemoteMotion = now;
      sawRemoteMotion = true;
    }
    if (sawRemoteMotion && now - lastRemoteMotion < HoldMs)
      activePairs |= 1U << (borderPair - 1);
  }

  const uint8_t changed = LuminaireManager::update(now, activePairs);
  for (uint8_t slot = 1; slot <= 6; ++slot) {
    if (!(changed & (1U << (slot - 1)))) continue;
    const char* id = luminaireId(slot);
    if (id != nullptr)
      TelemetryService::queueLuminaireChange(
        id, LuminaireManager::brightnessPercentage(slot));
  }
}
