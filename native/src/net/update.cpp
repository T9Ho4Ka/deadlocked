#include "net/update.hpp"

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <array>
#include <charconv>
#include <cstdio>
#include <vector>

namespace dl::net {
namespace {

/// Which repository the client checks. The rust client hardcodes the upstream one, and so
/// does this: a fork with no releases would answer every check with a 404.
constexpr const char* repository = DL_UPDATE_REPO;
constexpr const char* host = "api.github.com";
constexpr const char* port = "443";
/// what this build reports itself as
constexpr const char* current_version = DL_VERSION;

/// Splits "1.2.3" into its three numbers. Anything unparseable sorts as zero, which makes
/// a malformed tag look older rather than newer.
std::array<int, 3> parse_version(std::string_view text) {
    if (text.starts_with('v')) {
        text.remove_prefix(1);
    }
    std::array<int, 3> parts{0, 0, 0};
    for (int& part : parts) {
        const std::size_t dot = text.find('.');
        const std::string_view piece = text.substr(0, dot);
        std::from_chars(piece.data(), piece.data() + piece.size(), part);
        if (dot == std::string_view::npos) {
            break;
        }
        text.remove_prefix(dot + 1);
    }
    return parts;
}

/// One plain https GET, returning the body. Empty on any failure, with the reason in
/// `error`.
std::string https_get(const std::string& path, std::string& error) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* resolved = nullptr;
    if (::getaddrinfo(host, port, &hints, &resolved) != 0) {
        error = "could not resolve " + std::string(host);
        return {};
    }

    int fd = -1;
    for (const addrinfo* entry = resolved; entry != nullptr; entry = entry->ai_next) {
        fd = ::socket(entry->ai_family, entry->ai_socktype | SOCK_CLOEXEC, entry->ai_protocol);
        if (fd < 0) {
            continue;
        }
        timeval timeout{};
        timeout.tv_sec = 5;
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
        error = "could not reach " + std::string(host);
        return {};
    }

    SSL_CTX* context = SSL_CTX_new(TLS_client_method());
    if (context == nullptr) {
        ::close(fd);
        error = "could not set up tls";
        return {};
    }
    // without this the connection would be encrypted but unauthenticated, which is worse
    // than useless: anyone in the way could answer for github
    SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);
    SSL_CTX_set_default_verify_paths(context);

    SSL* ssl = SSL_new(context);
    SSL_set_fd(ssl, fd);
    // the certificate is checked against this name, and the server needs it to pick one
    SSL_set_tlsext_host_name(ssl, host);
    SSL_set1_host(ssl, host);

    std::string body;
    if (SSL_connect(ssl) == 1) {
        const std::string request = "GET " + path +
                                    " HTTP/1.1\r\nHost: " + host +
                                    "\r\nUser-Agent: deadlocked\r\n"
                                    "Accept: application/vnd.github.v3+json\r\n"
                                    "Connection: close\r\n\r\n";
        if (SSL_write(ssl, request.data(), static_cast<int>(request.size())) > 0) {
            std::string response;
            std::array<char, 4096> chunk{};
            while (true) {
                const int got = SSL_read(ssl, chunk.data(), static_cast<int>(chunk.size()));
                if (got <= 0) {
                    break;
                }
                response.append(chunk.data(), static_cast<std::size_t>(got));
            }

            const std::size_t status_end = response.find("\r\n");
            if (status_end != std::string::npos &&
                response.find(" 200 ") > status_end) {
                error = "github answered: " + response.substr(0, status_end);
            } else {
                const std::size_t split = response.find("\r\n\r\n");
                if (split != std::string::npos) {
                    body = response.substr(split + 4);
                }
            }
        } else {
            error = "could not send the request";
        }
    } else {
        error = "tls handshake failed";
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(context);
    ::close(fd);
    return body;
}

}  // namespace

UpdateCheck::~UpdateCheck() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

std::string UpdateCheck::json_string_field(const std::string& json, const std::string& field) {
    const std::string key = "\"" + field + "\"";
    std::size_t at = json.find(key);
    if (at == std::string::npos) {
        return {};
    }
    at = json.find(':', at + key.size());
    if (at == std::string::npos) {
        return {};
    }
    at = json.find('"', at);
    if (at == std::string::npos) {
        return {};
    }

    std::string value;
    for (std::size_t i = at + 1; i < json.size(); ++i) {
        if (json[i] == '\\' && i + 1 < json.size()) {
            // github escapes the slashes in urls, and nothing else worth decoding
            const char next = json[++i];
            value.push_back(next == 'n' ? '\n' : next == 't' ? '\t' : next);
            continue;
        }
        if (json[i] == '"') {
            break;
        }
        value.push_back(json[i]);
    }
    return value;
}

bool UpdateCheck::is_newer(const std::string& candidate, const std::string& current) {
    return parse_version(candidate) > parse_version(current);
}

void UpdateCheck::start() {
    thread_ = std::thread([this] { run(); });
}

UpdateResult UpdateCheck::result() const {
    const std::lock_guard lock(mutex_);
    return result_;
}

void UpdateCheck::run() {
    std::string error;
    const std::string body =
        https_get("/repos/" + std::string(repository) + "/releases/latest", error);

    UpdateResult outcome;
    if (body.empty()) {
        outcome.state = UpdateState::Failed;
        outcome.error = error.empty() ? "no response" : error;
    } else {
        const std::string tag = json_string_field(body, "tag_name");
        if (tag.empty()) {
            outcome.state = UpdateState::Failed;
            outcome.error = "no release tag in the response";
        } else if (is_newer(tag, current_version)) {
            outcome.state = UpdateState::Available;
            outcome.version = tag;
            outcome.url = json_string_field(body, "html_url");
        } else {
            outcome.state = UpdateState::UpToDate;
        }
    }

    const std::lock_guard lock(mutex_);
    result_ = outcome;
}

}  // namespace dl::net
