#pragma once

#include "../Models.h"

namespace TopologyService {
bool loadCached(TopologyConfig& topology);
bool fetch(TopologyConfig& topology);
void print(const TopologyConfig& topology);
}
