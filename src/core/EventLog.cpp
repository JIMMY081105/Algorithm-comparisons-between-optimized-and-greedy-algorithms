#include "core/EventLog.h"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <vector>

EventEntry::EventEntry(const std::string& ts, const std::string& msg)
    : timestamp(ts), message(msg), next(nullptr) {}

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
    EventEntry* newEntry = new EventEntry(timestamp, message);

    if (tail == nullptr) {
        // First entry — both head and tail point to it
        head = newEntry;
        tail = newEntry;
    } else {
        // Link the current tail to the new node, then advance tail
        tail->next = newEntry;
        tail = newEntry;
    }
    count++;
}

const EventEntry* EventLog::getHead() const {
    return head;
}

int EventLog::getCount() const {
    return count;
}

std::vector<const EventEntry*> EventLog::getRecentEvents(int maxCount) const {
    // Collect all events into a vector, then return the last N.
    // For a production system with thousands of events we'd use a
    // different approach, but for coursework this is clear and correct.
    std::vector<const EventEntry*> all;
    const EventEntry* current = head;
    while (current != nullptr) {
        all.push_back(current);
        current = current->next;
    }

    // Return the most recent events (from the end of the list)
    std::vector<const EventEntry*> recent;
    int start = static_cast<int>(all.size()) - maxCount;
    if (start < 0) start = 0;

    for (int i = static_cast<int>(all.size()) - 1; i >= start; i--) {
        recent.push_back(all[i]);
    }
    return recent;
}

void EventLog::clear() {
    EventEntry* current = head;
    while (current != nullptr) {
        EventEntry* next = current->next;
        delete current;
        current = next;
    }
    head = nullptr;
    tail = nullptr;
    count = 0;
}

std::string EventLog::getCurrentTimestamp() const {
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << localTime->tm_hour << ":"
        << std::setw(2) << localTime->tm_min << ":"
        << std::setw(2) << localTime->tm_sec;
    return oss.str();
}
