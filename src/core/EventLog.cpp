#include "core/EventLog.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace {
std::tm buildLocalTimestamp(std::time_t now) {
    std::tm timestamp{};
#ifdef _WIN32
    localtime_s(&timestamp, &now);
#else
    localtime_r(&now, &timestamp);
#endif
    return timestamp;
}
} // namespace

EventEntry::EventEntry(const std::string& ts, const std::string& msg)
    : timestamp(ts), message(msg), next(nullptr) {}

const EventEntry* EventEntry::nextEntry() const {
    return next.get();
}

EventLog::EventLog() : head(nullptr), tail(nullptr), count(0) {}

EventLog::~EventLog() {
    // Walk the linked list and free every node to prevent memory leaks.
    // This is the standard cleanup pattern for a singly-linked list.
    clear();
}

void EventLog::addEvent(const std::string& message) {
    addEventWithTime(getCurrentTimestamp(), message);
}

void EventLog::addEventWithTime(const std::string& timestamp, const std::string& message) {
    // Allocate a new node and append it to the tail of the list.
    // Maintaining a tail pointer makes this O(1) instead of O(n).
    auto newEntry = std::make_unique<EventEntry>(timestamp, message);
    EventEntry* newTail = newEntry.get();

    if (tail == nullptr) {
        // First entry — both head and tail point to it
        head = std::move(newEntry);
    } else {
        // Link the current tail to the new node, then advance tail
        tail->next = std::move(newEntry);
    }
    tail = newTail;
    count++;
}

const EventEntry* EventLog::getHead() const {
    return head.get();
}

int EventLog::getCount() const {
    return count;
}

std::vector<const EventEntry*> EventLog::getRecentEvents(int maxCount) const {
    if (maxCount <= 0 || head == nullptr) {
        return {};
    }

    const int retainedCount = std::min(count, maxCount);
    const int skipCount = std::max(0, count - retainedCount);

    std::vector<const EventEntry*> recent;
    recent.reserve(retainedCount);

    const EventEntry* current = head.get();
    for (int skipped = 0; skipped < skipCount && current != nullptr; ++skipped) {
        current = current->nextEntry();
    }

    while (current != nullptr) {
        recent.push_back(current);
        current = current->nextEntry();
    }

    std::reverse(recent.begin(), recent.end());
    return recent;
}

void EventLog::clear() {
    head.reset();
    tail = nullptr;
    count = 0;
}

std::string EventLog::getCurrentTimestamp() const {
    const std::tm localTime = buildLocalTimestamp(std::time(nullptr));
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << localTime.tm_hour << ":"
        << std::setw(2) << localTime.tm_min << ":"
        << std::setw(2) << localTime.tm_sec;
    return oss.str();
}
