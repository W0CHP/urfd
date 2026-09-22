#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>
#include <map>
#include <queue>
#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>

// Most this many events may be waiting to go out. Events are raised on
// protocol threads and sent from the maintenance thread every 100ms, so this
// is far deeper than a working reflector needs; it exists so that a publisher
// nobody is draining cannot grow without limit.
#define NNG_QUEUE_MAX 1024

class CNNGPublisher
{
public:
    CNNGPublisher();
    ~CNNGPublisher();

    bool Start(const std::string &addr);
    void Stop();

    // Builds an event pre-stamped with its type, the emitting reflector's
    // callsign and the current UTC time. Stamping happens here, at the moment
    // the event occurs, rather than inside Publish(): an event may be queued
    // before it reaches the socket, and the time it was observed is not the
    // time it was sent.
    static nlohmann::json NewEvent(const char *type);

    // Hand an event over for publication. This is what reflector threads
    // should call: it moves the event onto a queue and returns, so it is safe
    // to call with a clients or users mutex held.
    void Queue(nlohmann::json &&event);

    // Send everything queued. Call only from the maintenance thread, with no
    // reflector mutex held -- this serializes each event and touches the
    // socket.
    void Drain();

    // Sends immediately, on the calling thread. Prefer Queue() from anywhere
    // that holds a reflector lock.
    void Publish(const nlohmann::json &event);

    std::string GetAndClearStats();

private:
    nng_socket m_sock;
    std::mutex m_mutex;

    // Atomic so Queue() can check it without waiting on a send in progress.
    std::atomic<bool> m_started;

    // Event counters
    std::map<std::string, int> m_EventCounts;

    // Raised on protocol threads, drained on the maintenance thread. Separate
    // from m_mutex so that queueing never waits on a send.
    std::mutex m_QueueMutex;
    std::queue<nlohmann::json> m_Queue;
    size_t m_Dropped;
};
