#pragma once

#include <cstdint>
#include <vector>

namespace dl::cs2 {
struct Snapshot;
}

namespace dl::net {

/// Encodes a snapshot the way the radar server expects it: the rust `Data` struct in
/// postcard form. Fields the rust side marks `serde(skip)` are left out, which is most of
/// what the overlay uses and none of what the radar does.
[[nodiscard]] std::vector<std::uint8_t> encode_radar_frame(const cs2::Snapshot& snapshot);

}  // namespace dl::net
