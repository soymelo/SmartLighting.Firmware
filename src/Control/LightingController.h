#pragma once

#include "../Models.h"

namespace LightingController {
void begin();
bool applyTopology(const TopologyConfig& topology);
void update();
}
