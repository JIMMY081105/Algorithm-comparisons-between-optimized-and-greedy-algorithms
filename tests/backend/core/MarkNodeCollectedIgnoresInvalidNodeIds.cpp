#include "CoreTestUtils.h"

TEST_CASE(MarkNodeCollectedIgnoresInvalidNodeIds) {
    WasteSystem system;
    initializeSystem(system);
    system.markNodeCollected(1);
    REQUIRE_TRUE(system.getGraph().getNode(1).isCollected());
    system.markNodeCollected(9999);
    system.resetCollectionStatus();
    REQUIRE_FALSE(system.getGraph().getNode(1).isCollected());
}
