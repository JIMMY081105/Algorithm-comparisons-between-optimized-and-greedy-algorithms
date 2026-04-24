#ifndef CORE_TEST_UTILS_H
#define CORE_TEST_UTILS_H

#include "TestHarness.h"

#include "core/CostModel.h"
#include "core/EventLog.h"
#include "core/RoadEvent.h"
#include "core/RouteResult.h"
#include "core/TollStation.h"
#include "core/WasteNode.h"
#include "core/WasteSystem.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

inline void initializeSystem(WasteSystem& system, unsigned int seed = 1234u) {
    system.initializeMap();
    system.generateNewDayWithSeed(seed);
}

inline void clearNonHqWaste(WasteSystem& system) {
    MapGraph& graph = system.getGraph();
    for (int i = 0; i < graph.getNodeCount(); ++i) {
        WasteNode& node = graph.getNodeMutable(i);
        if (!node.getIsHQ()) {
            node.setWasteLevel(0.0f);
        }
    }
}

#endif // CORE_TEST_UTILS_H
