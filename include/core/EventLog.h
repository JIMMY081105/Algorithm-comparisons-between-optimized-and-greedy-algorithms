#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include <string>
#include <vector>

// A singly-linked list node for storing timestamped events.
// This demonstrates linked list usage as required by coursework.
// Each event records something that happened during the simulation,
// such as a route computation, a node being collected, or file export.
struct EventEntry {
    std::string timestamp;   // formatted time string
    std::string message;     // description of what happened
    EventEntry* next;        // pointer to next event in the chain

    EventEntry(const std::string& ts, const std::string& msg);
};

// Linked list-based event log that records simulation activities.
// New events are appended to the tail so they display in chronological order.
// This is intentionally implemented as a manual linked list rather than
// using std::list, to satisfy the coursework requirement for demonstrating
// linked list data structures with pointer manipulation.
class EventLog {
private:
    EventEntry* head;
    EventEntry* tail;
    int count;

public:
    EventLog();
    ~EventLog();

    // Prevent copying to avoid double-free issues with raw pointers
    EventLog(const EventLog&) = delete;
    EventLog& operator=(const EventLog&) = delete;

    // Add a new event with automatic timestamp
    void addEvent(const std::string& message);

    // Add event with a specific timestamp (useful for testing)
    void addEventWithTime(const std::string& timestamp, const std::string& message);

    // Access
    const EventEntry* getHead() const;
    int getCount() const;

    // Get the most recent N events (returns them newest-first for display)
    // Caller does not own the returned pointers — they belong to this log.
    std::vector<const EventEntry*> getRecentEvents(int maxCount) const;

    // Clear all logged events
    void clear();

private:
    // Internal helper to get current timestamp string
    std::string getCurrentTimestamp() const;
};

#endif // EVENT_LOG_H
