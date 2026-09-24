#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace dl::net {

enum class UpdateState { Checking, UpToDate, Available, Failed };

struct UpdateResult {
    UpdateState state = UpdateState::Checking;
    /// the tag of the newer release, when there is one
    std::string version;
    /// where to read about it
    std::string url;
    /// why the check could not be made
    std::string error;
};

/// Asks GitHub whether a newer release exists.
///
/// Runs on its own thread: the request can take as long as the network does, and the
/// window should be up before then. A failed check is reported, never retried, and never
/// fatal, since it says nothing about whether the client works.
class UpdateCheck {
public:
    ~UpdateCheck();

    void start();
    [[nodiscard]] UpdateResult result() const;

    /// Compares two semantic versions. Exposed for testing, since getting this wrong makes
    /// the client either nag forever or never mention an update at all.
    [[nodiscard]] static bool is_newer(const std::string& candidate, const std::string& current);

    /// Pulls one string field out of a json object. The response has two fields worth
    /// having and no nesting, so this is cheaper than a parser and handles the escapes.
    [[nodiscard]] static std::string json_string_field(const std::string& json,
                                                       const std::string& field);

private:
    void run();

    std::thread thread_;
    mutable std::mutex mutex_;
    UpdateResult result_;
};

}  // namespace dl::net
