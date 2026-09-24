#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace dl::net {

enum class RadarStatus { Disabled, Disconnected, FailedToConnect, Connected };

/// Streams the frame the overlay draws to a relay server, which serves the web radar.
///
/// It runs on its own thread: a connection attempt blocks for seconds, and the game loop
/// cannot wait for that. The newest frame is handed over under a lock and the thread sends
/// whatever is there, so a slow link drops frames rather than holding the game up.
class RadarClient {
public:
    RadarClient() = default;
    RadarClient(const RadarClient&) = delete;
    RadarClient& operator=(const RadarClient&) = delete;
    ~RadarClient();

    void start();
    void stop();

    /// Applies settings from the ui. Changing either the host or the session drops the
    /// current connection, since the server keys a session by both.
    void configure(bool enabled, const std::string& url, const std::string& uuid);

    /// Hands over the newest frame. Cheap: it only swaps a buffer under the lock.
    void submit(std::vector<std::uint8_t> frame);

    [[nodiscard]] RadarStatus status() const { return status_.load(); }

    /// Strips a scheme and a trailing slash, the way the rust client does, so the host can
    /// be pasted in either form.
    [[nodiscard]] static std::string normalize_url(std::string url);

private:
    void run();
    /// Opens the socket and does the handshake. Returns -1 on failure.
    [[nodiscard]] int connect_and_greet(const std::string& url, const std::string& uuid) const;

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<RadarStatus> status_{RadarStatus::Disabled};

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string url_;
    std::string uuid_;
    std::vector<std::uint8_t> pending_;
    /// bumped whenever the host or session changes, so the worker knows to reconnect
    std::uint64_t generation_ = 0;
};

}  // namespace dl::net
