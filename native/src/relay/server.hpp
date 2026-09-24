#pragma once

#include "relay/state.hpp"

namespace dl::relay {

/// Takes frames from cheat clients on 6346. One thread per client, since a client sends
/// twenty frames a second and there are never many of them.
void run_tcp_server(State& state);

/// Serves the web radar on 6347: a websocket at /client, and /stats as plain json.
void run_websocket_server(State& state);

}  // namespace dl::relay
