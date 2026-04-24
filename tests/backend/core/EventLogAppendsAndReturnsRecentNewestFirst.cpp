#include "CoreTestUtils.h"

TEST_CASE(EventLogAppendsAndReturnsRecentNewestFirst) {
    EventLog log;
    log.addEventWithTime("10:00:00", "first");
    log.addEventWithTime("10:01:00", "second");
    log.addEventWithTime("10:02:00", "third");

    const auto recent = log.getRecentEvents(2);
    REQUIRE_EQ(log.getCount(), 3);
    REQUIRE_EQ(recent.size(), static_cast<std::size_t>(2));
    REQUIRE_EQ(recent[0]->message, std::string("third"));
    REQUIRE_EQ(recent[1]->message, std::string("second"));
}
