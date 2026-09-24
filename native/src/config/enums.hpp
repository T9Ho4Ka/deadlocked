#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace dl::config {

/// One variant of a config enum: the value, how it reads in the ui, and the key it is
/// written under in the config file. Keys are names rather than indices, so reordering an
/// enum later cannot silently change what a saved config means.
template <typename E>
struct EnumEntry {
    E value;
    std::string_view name;
    std::string_view key;
};

/// Every config enum provides `enum_entries(E{})` returning its table, found by adl.
template <typename E>
constexpr std::string_view enum_name(E value) {
    for (const auto& entry : enum_entries(E{})) {
        if (entry.value == value) {
            return entry.name;
        }
    }
    return {};
}

template <typename E>
constexpr std::string_view enum_key(E value) {
    for (const auto& entry : enum_entries(E{})) {
        if (entry.value == value) {
            return entry.key;
        }
    }
    return {};
}

template <typename E>
constexpr E enum_from_key(std::string_view key, E fallback) {
    for (const auto& entry : enum_entries(E{})) {
        if (entry.key == key) {
            return entry.value;
        }
    }
    return fallback;
}

}  // namespace dl::config
