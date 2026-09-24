#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace dl::cs2 {
struct Snapshot;
}

namespace dl::net {

/// Encodes a snapshot the way the radar server expects it: the rust `Data` struct in
/// postcard form. Fields the rust side marks `serde(skip)` are left out, which is most of
/// what the overlay uses and none of what the radar does.
[[nodiscard]] std::vector<std::uint8_t> encode_radar_frame(const cs2::Snapshot& snapshot);

/// Reads one back. False when the frame is truncated or malformed, in which case the
/// snapshot is left in whatever state it had reached and should not be served.
[[nodiscard]] bool decode_radar_frame(std::span<const std::uint8_t> frame,
                                      cs2::Snapshot& out);

}  // namespace dl::net
