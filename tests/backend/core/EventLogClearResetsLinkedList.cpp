#include "CoreTestUtils.h"

TEST_CASE(EventLogClearResetsLinkedList) {
    EventLog log;
    log.addEventWithTime("10:00:00", "first");
    log.clear();
    REQUIRE_EQ(log.getCount(), 0);
    REQUIRE_TRUE(log.getHead() == nullptr);
    REQUIRE_TRUE(log.getRecentEvents(5).empty());
}
