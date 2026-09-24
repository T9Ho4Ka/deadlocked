#include "relay/server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

#include "net/json.hpp"
#include "net/radar_frame.hpp"
#include "relay/websocket.hpp"

namespace dl::relay {
namespace {

constexpr std::uint16_t tcp_port = 6346;
constexpr std::uint16_t websocket_port = 6347;

/// shared/src/version.rs
constexpr std::uint32_t proto_version = 2;
constexpr std::uint32_t heartbeat = 0xFFFF'FFFFu;
constexpr std::uint32_t max_frame_size = 16u * 1024u * 1024u;

constexpr auto inactivity_timeout = std::chrono::seconds(10);
constexpr auto push_interval = std::chrono::milliseconds(50);

int listen_on(std::uint16_t port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        std::fprintf(stderr, "could not open a socket: %s\n", std::strerror(errno));
        return -1;
    }

    // without this a restart fails for as long as the old sockets linger
    const int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        ::listen(fd, 16) < 0) {
        std::fprintf(stderr, "could not listen on port %u: %s\n", port, std::strerror(errno));
        ::close(fd);
        return -1;
    }
    return fd;
}

void set_timeout(int fd, std::chrono::seconds seconds) {
    timeval timeout{};
    timeout.tv_sec = seconds.count();
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

bool read_exact(int fd, std::uint8_t* data, std::size_t size) {
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

bool write_exact(int fd, const std::uint8_t* data, std::size_t size) {
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

std::optional<std::uint32_t> read_u32(int fd) {
    std::array<std::uint8_t, 4> bytes{};
    if (!read_exact(fd, bytes.data(), bytes.size())) {
        return std::nullopt;
    }
    std::uint32_t value = 0;
    for (const std::uint8_t byte : bytes) {
        value = (value << 8) | byte;
    }
    return value;
}

/// The session id as the client sent it: sixteen bytes, rendered the canonical way so it
/// matches what a browser will ask for.
std::string format_uuid(const std::array<std::uint8_t, 16>& bytes) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(36);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            out.push_back('-');
        }
        out.push_back(digits[bytes[i] >> 4]);
        out.push_back(digits[bytes[i] & 0x0F]);
    }
    return out;
}

void handle_client(int fd, State& state) {
    state.tcp_opened();
    set_timeout(fd, inactivity_timeout);

    std::string session;
    const auto version = read_u32(fd);
    if (!version.has_value()) {
        std::fprintf(stderr, "a client hung up before sending its protocol version\n");
    } else if (*version != proto_version) {
        std::fprintf(stderr, "a client speaks protocol %u, this relay speaks %u\n", *version,
                     proto_version);
    } else {
        std::array<std::uint8_t, 16> uuid{};
        if (read_exact(fd, uuid.data(), uuid.size()) &&
            // the echo is what tells the client the relay accepted it
            write_exact(fd, uuid.data(), uuid.size())) {
            session = format_uuid(uuid);
            state.open_session(session);
            std::printf("session %s connected\n", session.c_str());

            cs2::Snapshot snapshot;
            std::vector<std::uint8_t> frame;
            while (true) {
                const auto length = read_u32(fd);
                if (!length.has_value()) {
                    break;
                }
                if (*length == heartbeat) {
                    continue;
                }
                if (*length > max_frame_size) {
                    std::fprintf(stderr, "session %s sent an oversized frame of %u bytes\n",
                                 session.c_str(), *length);
                    break;
                }

                frame.resize(*length);
                if (!read_exact(fd, frame.data(), frame.size())) {
                    break;
                }
                if (!net::decode_radar_frame(frame, snapshot)) {
                    // a frame that will not parse is dropped, not the connection: the next
                    // one is usually fine
                    std::fprintf(stderr, "session %s sent a frame that would not decode\n",
                                 session.c_str());
                    continue;
                }
                state.publish(session, snapshot);
            }
        }
    }

    if (!session.empty()) {
        state.close_session(session);
        std::printf("session %s disconnected\n", session.c_str());
    }
    ::close(fd);
    state.tcp_closed();
}

/// Reads an http request up to the blank line that ends its headers.
std::string read_request(int fd) {
    std::string request;
    std::array<char, 1024> chunk{};
    while (request.find("\r\n\r\n") == std::string::npos && request.size() < 16384) {
        const ssize_t got = ::recv(fd, chunk.data(), chunk.size(), 0);
        if (got <= 0) {
            return {};
        }
        request.append(chunk.data(), static_cast<std::size_t>(got));
    }
    return request;
}

std::string header_value(const std::string& request, std::string_view name) {
    // header names are case insensitive, so both sides are lowered before comparing
    std::string lowered;
    lowered.reserve(request.size());
    std::ranges::transform(request, std::back_inserter(lowered),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::string key(name);
    std::ranges::transform(key, key.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    key += ':';

    const std::size_t at = lowered.find(key);
    if (at == std::string::npos) {
        return {};
    }
    std::size_t start = at + key.size();
    while (start < request.size() && (request[start] == ' ' || request[start] == '\t')) {
        ++start;
    }
    const std::size_t end = request.find("\r\n", start);
    return request.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

void send_http(int fd, std::string_view status, std::string_view content_type,
               std::string_view body) {
    std::string response;
    response += "HTTP/1.1 ";
    response += status;
    response += "\r\nContent-Type: ";
    response += content_type;
    response += "\r\nContent-Length: " + std::to_string(body.size());
    // the web radar is served from somewhere else, so it needs to be allowed to ask
    response += "\r\nAccess-Control-Allow-Origin: *";
    response += "\r\nConnection: close\r\n\r\n";
    response += body;
    write_exact(fd, reinterpret_cast<const std::uint8_t*>(response.data()), response.size());
}

void serve_websocket(int fd, State& state, const std::string& key) {
    const std::string handshake = "HTTP/1.1 101 Switching Protocols\r\n"
                                  "Upgrade: websocket\r\n"
                                  "Connection: Upgrade\r\n"
                                  "Sec-WebSocket-Accept: " +
                                  websocket::accept_key(key) + "\r\n\r\n";
    if (!write_exact(fd, reinterpret_cast<const std::uint8_t*>(handshake.data()),
                     handshake.size())) {
        return;
    }

    // the first thing a viewer sends is the session it wants to watch
    set_timeout(fd, inactivity_timeout);
    std::vector<std::uint8_t> buffer;
    std::array<std::uint8_t, 1024> chunk{};
    std::string session;
    while (session.empty()) {
        const ssize_t got = ::recv(fd, chunk.data(), chunk.size(), 0);
        if (got <= 0) {
            return;
        }
        buffer.insert(buffer.end(), chunk.begin(), chunk.begin() + got);
        const auto frame = websocket::parse_frame(buffer);
        if (!frame.has_value()) {
            continue;
        }
        if (frame->opcode == websocket::Opcode::Close) {
            return;
        }
        session = frame->payload;
        buffer.erase(buffer.begin(),
                     buffer.begin() + static_cast<std::ptrdiff_t>(frame->consumed));
    }

    state.websocket_opened();
    std::printf("viewer joined session %s\n", session.c_str());

    while (true) {
        const std::optional<cs2::Snapshot> snapshot = state.snapshot(session);
        if (!snapshot.has_value()) {
            // the session went away, so there is nothing left to watch
            break;
        }
        const std::vector<std::uint8_t> frame =
            websocket::text_frame(net::snapshot_to_json(*snapshot));
        if (!write_exact(fd, frame.data(), frame.size())) {
            break;
        }
        std::this_thread::sleep_for(push_interval);
    }

    std::printf("viewer left session %s\n", session.c_str());
    state.websocket_closed();
}

void handle_http(int fd, State& state) {
    const std::string request = read_request(fd);
    if (request.empty()) {
        ::close(fd);
        return;
    }

    if (request.starts_with("GET /stats")) {
        const std::string body = "{\"tcp_connections\":" +
                                 std::to_string(state.tcp_connections()) +
                                 ",\"websocket_connections\":" +
                                 std::to_string(state.websocket_connections()) +
                                 ",\"sessions\":" + std::to_string(state.session_count()) + "}";
        send_http(fd, "200 OK", "application/json", body);
        ::close(fd);
        return;
    }

    const std::string key = header_value(request, "Sec-WebSocket-Key");
    if (request.starts_with("GET /client") && !key.empty()) {
        serve_websocket(fd, state, key);
        ::close(fd);
        return;
    }

    send_http(fd, "404 Not Found", "text/plain", "not found");
    ::close(fd);
}

void accept_loop(int listener, State& state, void (*handler)(int, State&)) {
    while (true) {
        const int fd = ::accept(listener, nullptr, nullptr);
        if (fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::fprintf(stderr, "accept failed: %s\n", std::strerror(errno));
            continue;
        }
        const int one = 1;
        ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        std::thread(handler, fd, std::ref(state)).detach();
    }
}

}  // namespace

void run_tcp_server(State& state) {
    const int listener = listen_on(tcp_port);
    if (listener < 0) {
        return;
    }
    std::printf("taking client frames on port %u\n", tcp_port);
    accept_loop(listener, state, handle_client);
}

void run_websocket_server(State& state) {
    const int listener = listen_on(websocket_port);
    if (listener < 0) {
        return;
    }
    std::printf("serving the radar on port %u\n", websocket_port);
    accept_loop(listener, state, handle_http);
}

}  // namespace dl::relay
