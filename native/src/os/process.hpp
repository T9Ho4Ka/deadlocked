#pragma once

#include <cstddef>
#include <cstring>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace dl::os {

/// Read access to another process's memory, through process_vm_readv for scattered reads
/// and /proc/pid/mem for bulk ones. Ported from cheat/src/os/process.rs.
///
/// Reads never throw. `read` hands back a zeroed value when the read fails, which is what
/// the rust client does; call `try_read` instead wherever telling "zero" apart from "could
/// not read" actually matters.
class Process {
public:
    Process() = default;
    explicit Process(int pid);
    ~Process();

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    Process(Process&& other) noexcept;
    Process& operator=(Process&& other) noexcept;

    /// Finds a running process by executable name. Empty when it is not running, or when
    /// its memory cannot be opened.
    static std::optional<Process> open(std::string_view process_name);

    [[nodiscard]] int pid() const { return pid_; }
    [[nodiscard]] bool valid() const;

    /// Lowest and highest address of the game's own modules, with a megabyte of slack on
    /// either side. A pointer outside this is not one of ours.
    [[nodiscard]] std::uintptr_t data_min() const { return data_min_; }
    [[nodiscard]] std::uintptr_t data_max() const { return data_max_; }
    [[nodiscard]] bool in_data_range(std::uintptr_t address) const {
        return address >= data_min_ && address <= data_max_;
    }

    template <typename T>
    [[nodiscard]] T read(std::uintptr_t address) const {
        static_assert(std::is_trivially_copyable_v<T>, "reads are raw byte copies");
        T value{};
        read_raw(address, &value, sizeof(T));
        return value;
    }

    template <typename T>
    [[nodiscard]] bool try_read(std::uintptr_t address, T& out) const {
        static_assert(std::is_trivially_copyable_v<T>, "reads are raw byte copies");
        return read_raw(address, &out, sizeof(T));
    }

    [[nodiscard]] std::vector<std::uint8_t> read_vec(std::uintptr_t address,
                                                     std::size_t length) const;

    /// Reads `count` values that sit `stride` bytes apart, in one call.
    template <typename T>
    [[nodiscard]] std::vector<T> read_typed_vec(std::uintptr_t address, std::size_t stride,
                                                std::size_t count) const {
        static_assert(std::is_trivially_copyable_v<T>, "reads are raw byte copies");
        std::vector<T> result(count);
        if (count == 0 || stride < sizeof(T)) {
            return result;
        }
        const std::vector<std::uint8_t> buffer = read_vec(address, stride * count);
        if (buffer.size() < stride * count) {
            return result;
        }
        for (std::size_t i = 0; i < count; ++i) {
            std::memcpy(&result[i], buffer.data() + i * stride, sizeof(T));
        }
        return result;
    }

    /// Reads a nul terminated string, capped at 1024 bytes like the rust client.
    [[nodiscard]] std::string read_string(std::uintptr_t address) const;

    /// Bulk read through /proc/pid/mem, which beats process_vm_readv for whole modules.
    [[nodiscard]] std::vector<std::uint8_t> read_bytes(std::uintptr_t address,
                                                       std::size_t count) const;

    /// Writing is compiled out when DL_READ_ONLY is defined, matching the rust
    /// `read-only` feature. Returns whether anything was written.
    template <typename T>
    bool write(std::uintptr_t address, const T& value) const {
        static_assert(std::is_trivially_copyable_v<T>, "writes are raw byte copies");
        return write_raw(address, &value, sizeof(T));
    }

    /// Load address of a mapped module, looked up in /proc/pid/maps.
    [[nodiscard]] std::optional<std::uintptr_t> module_base_address(
        std::string_view module_name) const;

    /// Size of a mapped module, worked out from its ELF section header table.
    [[nodiscard]] std::size_t module_size(std::uintptr_t base_address) const;

    [[nodiscard]] std::vector<std::uint8_t> dump_module(std::uintptr_t base_address) const;

    /// Finds a byte pattern such as "48 8B 05 ? ? ? ? 48 85 C0" inside a module and returns
    /// its absolute address.
    [[nodiscard]] std::optional<std::uintptr_t> scan(std::string_view pattern,
                                                     std::uintptr_t base_address) const;

private:
    bool read_raw(std::uintptr_t address, void* out, std::size_t size) const;
    bool write_raw(std::uintptr_t address, const void* data, std::size_t size) const;
    void compute_data_range();

    int pid_ = -1;
    int mem_fd_ = -1;
    std::uintptr_t data_min_ = 0;
    std::uintptr_t data_max_ = 0;
};

/// Searches `haystack` for `pattern`, where a mask byte of 0 means "any byte here".
/// Exposed for testing; `Process::scan` is the one callers want.
std::optional<std::size_t> scan_bytes(const std::vector<std::uint8_t>& pattern,
                                      const std::vector<std::uint8_t>& mask,
                                      const std::vector<std::uint8_t>& haystack);

/// Parses "48 8B ? ?" into pattern and mask bytes. Empty when the pattern is unusable.
bool parse_pattern(std::string_view pattern, std::vector<std::uint8_t>& bytes,
                   std::vector<std::uint8_t>& mask);

}  // namespace dl::os
