#pragma once

#include <string>

namespace dl::cs2 {
struct Snapshot;
}

namespace dl::net {

/// Renders a snapshot the way serde_json renders the rust `Data`, which is what the web
/// radar parses. Field names, field order and number formatting all have to match, so the
/// tests compare the output against serde_json's own.
[[nodiscard]] std::string snapshot_to_json(const cs2::Snapshot& snapshot);

/// A float the way serde_json writes one: the shortest form that reads back exactly, with
/// a trailing ".0" when it would otherwise look like an integer.
[[nodiscard]] std::string json_number(float value);

/// A string with the characters json requires escaped.
[[nodiscard]] std::string json_escape(std::string_view text);

}  // namespace dl::net
