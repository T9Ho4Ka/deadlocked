#include "net/radar.hpp"

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace dl::net {
namespace {

constexpr const char* radar_port = "6346";
/// shared/src/version.rs
constexpr std::uint32_t proto_version = 2;
constexpr std::uint32_t heartbeat = 0xFFFF'FFFFu;
constexpr std::uint32_t max_frame_size = 16u * 1024u * 1024u;

constexpr auto connect_timeout = std::chrono::seconds(5);
constexpr auto heartbeat_interval = std::chrono::seconds(5);
constexpr auto send_interval = std::chrono::milliseconds(50);
constexpr auto retry_interval = std::chrono::seconds(1);

/// the handshake is big endian, unlike the postcard payload that follows it
void put_big_endian(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

/// A uuid on the wire is its sixteen bytes in order, which is what a u128 big endian is.
bool uuid_bytes(const std::string& uuid, std::array<std::uint8_t, 16>& out) {
    std::string digits;
    for (const char c : uuid) {
        if (std::isxdigit(static_cast<unsigned char>(c)) != 0) {
            digits.push_back(c);
        }
    }
    if (digits.size() != 32) {
        return false;
    }
    for (std::size_t i = 0; i < 16; ++i) {
        out[i] = static_cast<std::uint8_t>(std::stoul(digits.substr(i * 2, 2), nullptr, 16));
    }
    return true;
}

bool write_all(int fd, const std::uint8_t* data, std::size_t size) {
    std::size_t done = 0;
    while (done < size) {
        const ssize_t written = ::send(fd, data + done, size - done, MSG_NOSIGNAL);
        if (written <= 0) {
            return false;
        }
        done += static_cast<std::size_t>(written);
    }
    return true;
}

bool read_all(int fd, std::uint8_t* data, std::size_t size) {
    std::size_t done = 0;
    while (done < size) {
        const ssize_t got = ::recv(fd, data + done, size - done, 0);
        if (got <= 0) {
            return false;
        }
        done += static_cast<std::size_t>(got);
    }
    return true;
}

}  // namespace

RadarClient::~RadarClient() { stop(); }

std::string RadarClient::normalize_url(std::string url) {
    const auto not_space = [](unsigned char c) { return std::isspace(c) == 0; };
    url.erase(url.begin(), std::ranges::find_if(url, not_space));
    url.erase(std::find_if(url.rbegin(), url.rend(), not_space).base(), url.end());

    for (const std::string_view scheme : {"http://", "https://"}) {
        if (url.starts_with(scheme)) {
            url.erase(0, scheme.size());
            break;
        }
    }
    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    return url;
}

void RadarClient::start() {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::thread([this] { run(); });
}

void RadarClient::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

void RadarClient::configure(bool enabled, const std::string& url, const std::string& uuid) {
    const std::string normalized = normalize_url(url);
    const std::lock_guard lock(mutex_);
    if (normalized != url_ || uuid != uuid_) {
        // the server keys a session by host and uuid together, so either changing means the
        // current connection is for a different session
        ++generation_;
    }
    enabled_ = enabled;
    url_ = normalized;
    uuid_ = uuid;
}

void RadarClient::submit(std::vector<std::uint8_t> frame) {
    const std::lock_guard lock(mutex_);
    pending_ = std::move(frame);
}

int RadarClient::connect_and_greet(const std::string& url, const std::string& uuid) const {
    std::array<std::uint8_t, 16> session{};
    if (!uuid_bytes(uuid, session)) {
        return -1;
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* resolved = nullptr;
    if (::getaddrinfo(url.c_str(), radar_port, &hints, &resolved) != 0) {
        return -1;
    }

    int fd = -1;
    for (const addrinfo* entry = resolved; entry != nullptr; entry = entry->ai_next) {
        fd = ::socket(entry->ai_family, entry->ai_socktype | SOCK_CLOEXEC, entry->ai_protocol);
        if (fd < 0) {
            continue;
        }

        timeval timeout{};
        timeout.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(connect_timeout).count();
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        if (::connect(fd, entry->ai_addr, entry->ai_addrlen) == 0) {
            break;
        }
        ::close(fd);
        fd = -1;
    }
    ::freeaddrinfo(resolved);

    if (fd < 0) {
        return -1;
    }

    // frames are small and frequent, so they should not sit waiting to be coalesced
    const int one = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    std::vector<std::uint8_t> greeting;
    put_big_endian(greeting, proto_version);
    greeting.insert(greeting.end(), session.begin(), session.end());

    std::array<std::uint8_t, 16> echoed{};
    // the server echoes the session back, which is how it says it accepted the version
    if (!write_all(fd, greeting.data(), greeting.size()) ||
        !read_all(fd, echoed.data(), echoed.size()) || echoed != session) {
        ::close(fd);
        return -1;
    }
    return fd;
}

void RadarClient::run() {
    int fd = -1;
    std::uint64_t connected_generation = 0;
    auto last_heartbeat = std::chrono::steady_clock::now();

    const auto drop = [&](RadarStatus reason) {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
        status_.store(reason);
    };

    while (running_.load()) {
        bool enabled = false;
        std::string url;
        std::string uuid;
        std::vector<std::uint8_t> frame;
        std::uint64_t generation = 0;
        {
            const std::lock_guard lock(mutex_);
            enabled = enabled_;
            url = url_;
            uuid = uuid_;
            generation = generation_;
            frame.swap(pending_);
        }

        if (!enabled || url.empty() || uuid.empty()) {
            drop(RadarStatus::Disabled);
            std::this_thread::sleep_for(retry_interval);
            continue;
        }
        if (fd >= 0 && generation != connected_generation) {
            drop(RadarStatus::Disconnected);
        }

        if (fd < 0) {
            fd = connect_and_greet(url, uuid);
            if (fd < 0) {
                status_.store(RadarStatus::FailedToConnect);
                std::this_thread::sleep_for(retry_interval);
                continue;
            }
            connected_generation = generation;
            last_heartbeat = std::chrono::steady_clock::now();
            status_.store(RadarStatus::Connected);
        }

        if (!frame.empty()) {
            if (frame.size() > max_frame_size) {
                std::fprintf(stderr, "radar frame of %zu bytes is over the limit\n",
                             frame.size());
                drop(RadarStatus::FailedToConnect);
                continue;
            }
            std::vector<std::uint8_t> header;
            put_big_endian(header, static_cast<std::uint32_t>(frame.size()));
            if (!write_all(fd, header.data(), header.size()) ||
                !write_all(fd, frame.data(), frame.size())) {
                drop(RadarStatus::FailedToConnect);
                continue;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - last_heartbeat >= heartbeat_interval) {
            std::vector<std::uint8_t> beat;
            put_big_endian(beat, heartbeat);
            if (!write_all(fd, beat.data(), beat.size())) {
                drop(RadarStatus::FailedToConnect);
                continue;
            }
            last_heartbeat = now;
        }

        std::this_thread::sleep_for(send_interval);
    }

    drop(RadarStatus::Disabled);
}

}  // namespace dl::net
