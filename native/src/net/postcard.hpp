#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "math.hpp"

namespace dl::net {

/// Writes the postcard wire format, which is what the radar server expects.
///
/// Postcard is serde's compact binary format: no field names, no type tags, just the values
/// in declaration order. That makes the encoding entirely a matter of writing the same
/// fields in the same order as the rust structs, and nothing checks that for you, so the
/// tests compare the bytes against what rust's own serializer produces.
class Writer {
public:
    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const { return out_; }
    [[nodiscard]] std::size_t size() const { return out_.size(); }
    void clear() { out_.clear(); }

    /// a byte goes out as itself
    void u8(std::uint8_t value) { out_.push_back(value); }
    void boolean(bool value) { u8(value ? 1 : 0); }
    void i8(std::int8_t value) { u8(static_cast<std::uint8_t>(value)); }

    /// anything wider is leb128: seven bits a byte, top bit set while more follow
    void varint(std::uint64_t value) {
        while (value >= 0x80) {
            out_.push_back(static_cast<std::uint8_t>(value) | 0x80);
            value >>= 7;
        }
        out_.push_back(static_cast<std::uint8_t>(value));
    }

    void u16(std::uint16_t value) { varint(value); }
    void u32(std::uint32_t value) { varint(value); }
    void u64(std::uint64_t value) { varint(value); }

    /// signed values are zigzagged first, so small negatives stay short
    void i16(std::int16_t value) { varint(zigzag(value, 15)); }
    void i32(std::int32_t value) { varint(zigzag(value, 31)); }
    void i64(std::int64_t value) { varint(zigzag(value, 63)); }

    /// floats are their bit pattern, little endian, never varint
    void f32(float value) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        for (int i = 0; i < 4; ++i) {
            out_.push_back(static_cast<std::uint8_t>(bits >> (i * 8)));
        }
    }

    void vec3(const Vec3& value) {
        f32(value.x);
        f32(value.y);
        f32(value.z);
    }

    /// a string and a sequence are both a length followed by the contents
    void string(std::string_view value) {
        varint(value.size());
        out_.insert(out_.end(), value.begin(), value.end());
    }
    void length(std::size_t count) { varint(count); }

    /// an enum is the index of its variant, counting from the first one declared
    void variant(std::size_t index) { varint(index); }

    /// an option is a present flag, then the value if there is one
    void none() { u8(0); }
    void some() { u8(1); }

private:
    static std::uint64_t zigzag(std::int64_t value, int bits) {
        return static_cast<std::uint64_t>((value << 1) ^ (value >> bits));
    }

    std::vector<std::uint8_t> out_;
};

}  // namespace dl::net
