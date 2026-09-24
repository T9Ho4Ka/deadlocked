#include <chrono>
#include <cstdio>
#include <thread>

#include "relay/server.hpp"
#include "relay/state.hpp"

/// The relay: cheat clients push frames in over tcp, browsers watch them over a websocket.
/// It keeps only the latest frame of each session and nothing on disk.
int main() {
    dl::relay::State state;

    std::thread tcp([&state] { dl::relay::run_tcp_server(state); });
    std::thread websocket([&state] { dl::relay::run_websocket_server(state); });

    // a client that stops sending leaves its session behind, so they are swept periodically
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        state.drop_stale();
    }

    tcp.join();
    websocket.join();
    return 0;
}
