#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "cs2/snapshot.hpp"

namespace dl::relay {

/// What the relay knows: the latest frame of every session, and how many of each kind of
/// connection it is holding.
///
/// One lock over the whole table. A session is written once every fifty milliseconds by one
/// client and read at the same rate by its viewers, so there is nothing here worth a finer
/// grained scheme.
class State {
public:
    /// how long a session survives without a frame before it is dropped
    static constexpr std::chrono::seconds session_timeout{60};

    void publish(const std::string& session, const cs2::Snapshot& snapshot);
    /// The latest frame of a session, empty when there is no such session.
    [[nodiscard]] std::optional<cs2::Snapshot> snapshot(const std::string& session) const;
    void open_session(const std::string& session);
    void close_session(const std::string& session);

    /// Drops sessions nobody has sent a frame for. Returns how many went.
    std::size_t drop_stale();

    [[nodiscard]] std::size_t session_count() const;
    [[nodiscard]] std::size_t tcp_connections() const { return tcp_.load(); }
    [[nodiscard]] std::size_t websocket_connections() const { return websocket_.load(); }

    void tcp_opened() { ++tcp_; }
    void tcp_closed() { --tcp_; }
    void websocket_opened() { ++websocket_; }
    void websocket_closed() { --websocket_; }

private:
    struct Session {
        cs2::Snapshot snapshot;
        std::chrono::steady_clock::time_point updated;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Session> sessions_;
    std::atomic<std::size_t> tcp_{0};
    std::atomic<std::size_t> websocket_{0};
};

}  // namespace dl::relay
