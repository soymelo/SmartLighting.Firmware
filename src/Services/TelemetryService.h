#pragma once

#include <Arduino.h>

namespace TelemetryService {
void begin();
void queueSensorEvent(const char* sensorId);
void queueLuminaireChange(const char* luminaireId, uint8_t brightnessPercentage);
void update();
}
