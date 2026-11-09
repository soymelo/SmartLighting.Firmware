#include "LightingController.h"

#include <HardwareSerial.h>
#include <strings.h>

#include "../../DeviceConfig.h"
#include "../Hardware/LuminaireManager.h"
#include "../Hardware/SensorManager.h"
#include "../Services/TelemetryService.h"
#include "../Services/TopologyService.h"
#include "ZoneActivityCounter.h"

namespace {
constexpr unsigned long HoldMs = 5000;
constexpr unsigned long PeerRepeatMs = 1000;
TopologyConfig current;
ZoneActivityCounter activityCounter;
bool configured = false;
unsigned long lastMotion[6] = {};
bool sawMotion[6] = {};
unsigned long lastTransmit[3] = {};
bool transmitted[3] = {};
bool wasLocalActive[3] = {};
HardwareSerial peerSerial(2);
char receivedMessage[38] = {};
uint8_t receivedLength = 0;
uint8_t transmitPin = 0;
uint8_t receivePin = 0;

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

void receivePeerMotion(unsigned long now) {
  while (peerSerial.available() > 0) {
    const char value = static_cast<char>(peerSerial.read());
    if (value == '\n') {
      if (receivedLength == 37 &&
          (receivedMessage[0] == 'A' || receivedMessage[0] == 'R')) {
        receivedMessage[37] = '\0';
        for (uint8_t index = 0; index < current.controlSensorCount; ++index) {
          const ControlSensorConfig& sensor = current.controlSensors[index];
          if (strcasecmp(sensor.id, receivedMessage + 1) == 0 &&
              strcasecmp(sensor.deviceId, current.deviceId) != 0) {
            lastMotion[index] = now;
            sawMotion[index] = true;
            break;
          }
        }
      }
      receivedLength = 0;
    } else if (value != '\r' && receivedLength < sizeof(receivedMessage) - 1) {
      receivedMessage[receivedLength++] = value;
    } else {
      receivedLength = sizeof(receivedMessage);
    }
  }
}
}

void LightingController::begin() {
  if (DeviceConfig::BoardNumber == 1) {
    transmitPin = 13;
    receivePin = 14;
  } else if (DeviceConfig::BoardNumber == 2) {
    transmitPin = 14;
    receivePin = 13;
  } else {
    Serial.println("BoardNumber nao configurado: comunicacao entre ESP32 desativada.");
    return;
  }
  peerSerial.begin(115200, SERIAL_8N1, receivePin, transmitPin);
}

bool LightingController::applyTopology(const TopologyConfig& topology) {
  if (!LuminaireManager::begin(topology)) return false;
  SensorManager::begin(topology);
  current = topology;
  activityCounter.reset(current);
  configured = true;
  for (uint8_t index = 0; index < 6; ++index) {
    lastMotion[index] = 0;
    sawMotion[index] = false;
  }
  for (uint8_t index = 0; index < 3; ++index) {
    lastTransmit[index] = 0;
    transmitted[index] = false;
    wasLocalActive[index] = false;
  }
  receivedLength = 0;
  if (transmitPin != 0) while (peerSerial.available() > 0) peerSerial.read();
  TopologyService::print(current);
  return true;
}

void LightingController::update() {
  if (!configured) return;
  const unsigned long now = millis();
  const uint8_t detections = SensorManager::update();
  for (uint8_t slot = 1; slot <= 3; ++slot) {
    if (detections & (1U << (slot - 1))) {
      const char* id = sensorId(slot);
      if (id != nullptr) TelemetryService::queueSensorEvent(id);
    }
  }

  for (uint8_t index = 0; index < current.controlSensorCount; ++index) {
    const ControlSensorConfig& sensor = current.controlSensors[index];
    if (strcasecmp(sensor.deviceId, current.deviceId) != 0) continue;
    if (detections & (1U << (sensor.slot - 1))) {
      const uint16_t affectedZones = activityCounter.record(index);
      for (uint8_t zone = 0; zone < current.zoneCount; ++zone)
        if (affectedZones & (1U << zone))
          Serial.printf("Zona %s: %lu ativacao(oes) local(is) desde a topologia.\n",
                        current.zones[zone].zoneId,
                        static_cast<unsigned long>(activityCounter.total(zone)));
    }
    const bool active = SensorManager::isActive(sensor.slot);
    const uint8_t slotIndex = sensor.slot - 1;
    if (!active) {
      if (wasLocalActive[slotIndex] && transmitPin != 0) {
        peerSerial.write('R');
        peerSerial.print(sensor.id);
        peerSerial.write('\n');
      }
      wasLocalActive[slotIndex] = false;
      transmitted[slotIndex] = false;
      continue;
    }
    lastMotion[index] = now;
    sawMotion[index] = true;
    wasLocalActive[slotIndex] = true;
    if (transmitPin != 0 &&
        (!transmitted[slotIndex] || now - lastTransmit[slotIndex] >= PeerRepeatMs)) {
      peerSerial.write('A');
      peerSerial.print(sensor.id);
      peerSerial.write('\n');
      lastTransmit[slotIndex] = now;
      transmitted[slotIndex] = true;
    }
  }

  if (receivePin != 0) receivePeerMotion(now);

  uint8_t activeSensors = 0;
  for (uint8_t index = 0; index < current.controlSensorCount; ++index)
    if (sawMotion[index] && now - lastMotion[index] < HoldMs)
      activeSensors |= 1U << index;

  uint8_t activeLuminaires = 0;
  for (uint8_t slot = 1; slot <= 6; ++slot)
    if (current.luminaireSensorMask[slot] & activeSensors)
      activeLuminaires |= 1U << (slot - 1);

  const uint8_t changed = LuminaireManager::update(now, activeLuminaires);
  for (uint8_t slot = 1; slot <= 6; ++slot) {
    if (!(changed & (1U << (slot - 1)))) continue;
    const char* id = luminaireId(slot);
    if (id != nullptr)
      TelemetryService::queueLuminaireChange(
        id, LuminaireManager::brightnessPercentage(slot));
  }
}
