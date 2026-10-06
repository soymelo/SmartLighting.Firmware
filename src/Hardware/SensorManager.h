#pragma once

#include "../Models.h"

namespace SensorManager {
void begin(const TopologyConfig& topology);
uint8_t update();
bool isActive(uint8_t slot);
}
