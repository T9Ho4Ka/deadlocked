#include "relay/state.hpp"

#include <algorithm>
#include <cstdio>

namespace dl::relay {

void State::publish(const std::string& session, const cs2::Snapshot& snapshot) {
    const std::lock_guard lock(mutex_);
    Session& entry = sessions_[session];
    entry.snapshot = snapshot;
    entry.updated = std::chrono::steady_clock::now();
}

std::optional<cs2::Snapshot> State::snapshot(const std::string& session) const {
    const std::lock_guard lock(mutex_);
    const auto found = sessions_.find(session);
    if (found == sessions_.end()) {
        return std::nullopt;
    }
    return found->second.snapshot;
}

void State::open_session(const std::string& session) {
    const std::lock_guard lock(mutex_);
    sessions_[session].updated = std::chrono::steady_clock::now();
}

void State::close_session(const std::string& session) {
    const std::lock_guard lock(mutex_);
    sessions_.erase(session);
}

std::size_t State::drop_stale() {
    const auto now = std::chrono::steady_clock::now();
    const std::lock_guard lock(mutex_);
    return std::erase_if(sessions_, [&](const auto& entry) {
        const bool stale = now - entry.second.updated >= session_timeout;
        if (stale) {
            std::printf("dropped session %s after timeout\n", entry.first.c_str());
        }
        return stale;
    });
}

std::size_t State::session_count() const {
    const std::lock_guard lock(mutex_);
    return sessions_.size();
}

}  // namespace dl::relay
