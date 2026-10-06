#pragma once

#include "../Models.h"

namespace LuminaireManager {
bool begin(const TopologyConfig& topology);
uint8_t update(unsigned long now, uint8_t activePairs);
uint8_t brightnessPercentage(uint8_t slot);
}
