#include "os/process.hpp"

#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <immintrin.h>
#include <limits>
#include <utility>

#include "constants.hpp"

namespace dl::os {
namespace {

constexpr std::size_t max_string_length = 1024;
/// slack added on each side of the game's modules, same as the rust client
constexpr std::uintptr_t data_range_slack = 1'000'000;

std::optional<int> find_pid(std::string_view process_name) {
    std::error_code error;
    std::filesystem::directory_iterator entries("/proc", error);
    if (error) {
        return std::nullopt;
    }

    for (const std::filesystem::directory_entry& entry : entries) {
        if (!entry.is_directory(error)) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (name.empty() ||
            !std::ranges::all_of(name, [](char c) { return c >= '0' && c <= '9'; })) {
            continue;
        }

        const std::filesystem::path exe =
            std::filesystem::read_symlink(entry.path() / "exe", error);
        if (error) {
            // every process we do not own lands here, that is expected
            error.clear();
            continue;
        }
        if (exe.filename().string() != process_name) {
            continue;
        }

        int pid = 0;
        const char* begin = name.data();
        if (std::from_chars(begin, begin + name.size(), pid).ec == std::errc{}) {
            return pid;
        }
    }
    return std::nullopt;
}

/// scalar fallback, and the tail the vector path cannot cover
std::optional<std::size_t> scan_scalar(const std::uint8_t* pattern, const std::uint8_t* mask,
                                       std::size_t pattern_length,
                                       const std::vector<std::uint8_t>& haystack,
                                       std::size_t from) {
    if (haystack.size() < pattern_length) {
        return std::nullopt;
    }
    const std::size_t stop = haystack.size() - pattern_length;
    for (std::size_t i = from; i <= stop; ++i) {
        bool hit = true;
        for (std::size_t j = 0; j < pattern_length; ++j) {
            if (mask[j] == 0xFF && haystack[i + j] != pattern[j]) {
                hit = false;
                break;
            }
        }
        if (hit) {
            return i;
        }
    }
    return std::nullopt;
}

/// 32 bytes at a time: xor against the pattern, mask off the wildcards, and a zero result
/// means every fixed byte matched
__attribute__((target("avx2"))) std::optional<std::size_t> scan_avx2(
    const std::uint8_t* pattern, const std::uint8_t* mask, std::size_t pattern_length,
    const std::vector<std::uint8_t>& haystack) {
    alignas(32) std::uint8_t pattern_padded[32]{};
    alignas(32) std::uint8_t mask_padded[32]{};
    std::memcpy(pattern_padded, pattern, pattern_length);
    std::memcpy(mask_padded, mask, pattern_length);

    const __m256i pattern_vec =
        _mm256_load_si256(reinterpret_cast<const __m256i*>(pattern_padded));
    const __m256i mask_vec = _mm256_load_si256(reinterpret_cast<const __m256i*>(mask_padded));

    const std::size_t stop = haystack.size() - 32;
    for (std::size_t i = 0; i <= stop; ++i) {
        if (mask[0] == 0xFF && haystack[i] != pattern[0]) {
            continue;
        }
        const __m256i window =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(haystack.data() + i));
        const __m256i masked = _mm256_and_si256(_mm256_xor_si256(window, pattern_vec), mask_vec);
        if (_mm256_testz_si256(masked, masked) == 1) {
            return i;
        }
    }
    return std::nullopt;
}

}  // namespace

bool parse_pattern(std::string_view pattern, std::vector<std::uint8_t>& bytes,
                   std::vector<std::uint8_t>& mask) {
    bytes.clear();
    mask.clear();

    std::size_t position = 0;
    while (position < pattern.size()) {
        const std::size_t start = pattern.find_first_not_of(" \t\r\n", position);
        if (start == std::string_view::npos) {
            break;
        }
        std::size_t end = pattern.find_first_of(" \t\r\n", start);
        if (end == std::string_view::npos) {
            end = pattern.size();
        }
        const std::string_view token = pattern.substr(start, end - start);
        position = end;

        if (token == "?" || token == "??") {
            bytes.push_back(0x00);
            mask.push_back(0x00);
            continue;
        }
        unsigned value = 0;
        const char* begin = token.data();
        if (token.size() != 2 ||
            std::from_chars(begin, begin + token.size(), value, 16).ec != std::errc{}) {
            std::fprintf(stderr, "unrecognized pattern token \"%.*s\"\n",
                         static_cast<int>(token.size()), token.data());
            continue;
        }
        bytes.push_back(static_cast<std::uint8_t>(value));
        mask.push_back(0xFF);
    }

    return !bytes.empty();
}

std::optional<std::size_t> scan_bytes(const std::vector<std::uint8_t>& pattern,
                                      const std::vector<std::uint8_t>& mask,
                                      const std::vector<std::uint8_t>& haystack) {
    if (pattern.empty() || pattern.size() != mask.size() || haystack.size() < pattern.size()) {
        return std::nullopt;
    }

    // the vector path needs a full 32 byte window, so the last stretch is always scanned
    // scalar. the rust version simply stops early there and can miss a match in the tail.
    const bool use_avx2 = pattern.size() <= 32 && haystack.size() >= 32 &&
                          __builtin_cpu_supports("avx2");
    if (use_avx2) {
        if (const std::optional<std::size_t> hit =
                scan_avx2(pattern.data(), mask.data(), pattern.size(), haystack)) {
            return hit;
        }
        return scan_scalar(pattern.data(), mask.data(), pattern.size(), haystack,
                           haystack.size() - 32 + 1);
    }
    return scan_scalar(pattern.data(), mask.data(), pattern.size(), haystack, 0);
}

Process::Process(int pid) : pid_(pid) {
    if (pid <= 0) {
        return;
    }
    const std::string path = "/proc/" + std::to_string(pid) + "/mem";
    mem_fd_ = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (mem_fd_ < 0) {
        std::fprintf(stderr, "failed to open %s: %s\n", path.c_str(), std::strerror(errno));
    }
    compute_data_range();
}

Process::~Process() {
    if (mem_fd_ >= 0) {
        ::close(mem_fd_);
    }
}

Process::Process(Process&& other) noexcept
    : pid_(other.pid_),
      mem_fd_(other.mem_fd_),
      data_min_(other.data_min_),
      data_max_(other.data_max_) {
    other.pid_ = -1;
    other.mem_fd_ = -1;
}

Process& Process::operator=(Process&& other) noexcept {
    if (this != &other) {
        if (mem_fd_ >= 0) {
            ::close(mem_fd_);
        }
        pid_ = other.pid_;
        mem_fd_ = other.mem_fd_;
        data_min_ = other.data_min_;
        data_max_ = other.data_max_;
        other.pid_ = -1;
        other.mem_fd_ = -1;
    }
    return *this;
}

std::optional<Process> Process::open(std::string_view process_name) {
    const std::optional<int> pid = find_pid(process_name);
    if (!pid.has_value()) {
        return std::nullopt;
    }
    Process process(*pid);
    if (!process.valid()) {
        return std::nullopt;
    }
    return process;
}

bool Process::valid() const {
    if (pid_ <= 0) {
        return false;
    }
    std::error_code error;
    return std::filesystem::exists("/proc/" + std::to_string(pid_), error);
}

void Process::compute_data_range() {
    std::uintptr_t min = std::numeric_limits<std::uintptr_t>::max();
    std::uintptr_t max = 0;

    for (const std::string_view lib : constants::cs2::libs) {
        const std::optional<std::uintptr_t> base = module_base_address(lib);
        if (!base.has_value()) {
            continue;
        }
        const std::size_t size = module_size(*base);
        min = std::min(min, *base > data_range_slack ? *base - data_range_slack : 0);
        max = std::max(max, *base + size + data_range_slack);
    }

    if (min == std::numeric_limits<std::uintptr_t>::max()) {
        // none of the game's modules are here, so nothing is in range
        min = 0;
        max = 0;
    }
    data_min_ = min;
    data_max_ = max;
}

bool Process::read_raw(std::uintptr_t address, void* out, std::size_t size) const {
    if (pid_ <= 0 || size == 0) {
        return false;
    }
    iovec local{out, size};
    iovec remote{reinterpret_cast<void*>(address), size};
    const ssize_t got = ::process_vm_readv(pid_, &local, 1, &remote, 1, 0);
    return got == static_cast<ssize_t>(size);
}

bool Process::write_raw([[maybe_unused]] std::uintptr_t address,
                        [[maybe_unused]] const void* data,
                        [[maybe_unused]] std::size_t size) const {
#ifdef DL_READ_ONLY
    return false;
#else
    if (pid_ <= 0 || size == 0) {
        return false;
    }
    iovec local{const_cast<void*>(data), size};
    iovec remote{reinterpret_cast<void*>(address), size};
    return ::process_vm_writev(pid_, &local, 1, &remote, 1, 0) == static_cast<ssize_t>(size);
#endif
}

std::vector<std::uint8_t> Process::read_vec(std::uintptr_t address, std::size_t length) const {
    std::vector<std::uint8_t> buffer(length, 0);
    if (length != 0) {
        read_raw(address, buffer.data(), length);
    }
    return buffer;
}

std::string Process::read_string(std::uintptr_t address) const {
    const std::vector<std::uint8_t> chunk = read_vec(address, max_string_length);
    const auto terminator = std::ranges::find(chunk, 0);
    return std::string(chunk.begin(), terminator);
}

std::vector<std::uint8_t> Process::read_bytes(std::uintptr_t address, std::size_t count) const {
    std::vector<std::uint8_t> buffer(count, 0);
    if (mem_fd_ < 0 || count == 0) {
        return buffer;
    }

    std::size_t done = 0;
    while (done < count) {
        const ssize_t got = ::pread(mem_fd_, buffer.data() + done, count - done,
                                    static_cast<off_t>(address + done));
        if (got <= 0) {
            break;
        }
        done += static_cast<std::size_t>(got);
    }
    return buffer;
}

std::optional<std::uintptr_t> Process::module_base_address(std::string_view module_name) const {
    if (pid_ <= 0) {
        return std::nullopt;
    }
    std::ifstream maps("/proc/" + std::to_string(pid_) + "/maps");
    if (!maps) {
        return std::nullopt;
    }

    std::string line;
    while (std::getline(maps, line)) {
        if (line.find(module_name) == std::string::npos) {
            continue;
        }
        const std::size_t dash = line.find('-');
        if (dash == std::string::npos) {
            continue;
        }
        std::uintptr_t address = 0;
        const char* begin = line.data();
        if (std::from_chars(begin, begin + dash, address, 16).ec == std::errc{}) {
            return address;
        }
    }
    return std::nullopt;
}

std::size_t Process::module_size(std::uintptr_t base_address) const {
    const auto offset = read<std::uint64_t>(base_address + constants::elf::section_header_offset);
    const auto entry_size =
        read<std::uint16_t>(base_address + constants::elf::section_header_entry_size);
    const auto entries =
        read<std::uint16_t>(base_address + constants::elf::section_header_num_entries);
    return static_cast<std::size_t>(offset) +
           static_cast<std::size_t>(entry_size) * static_cast<std::size_t>(entries);
}

std::vector<std::uint8_t> Process::dump_module(std::uintptr_t base_address) const {
    return read_bytes(base_address, module_size(base_address));
}

std::optional<std::uintptr_t> Process::scan(std::string_view pattern,
                                            std::uintptr_t base_address) const {
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint8_t> mask;
    if (!parse_pattern(pattern, bytes, mask)) {
        return std::nullopt;
    }

    const std::vector<std::uint8_t> module = dump_module(base_address);
    if (module.size() < 500) {
        return std::nullopt;
    }

    const std::optional<std::size_t> hit = scan_bytes(bytes, mask, module);
    if (!hit.has_value()) {
        std::fprintf(stderr, "pattern \"%.*s\" not found, might be outdated\n",
                     static_cast<int>(pattern.size()), pattern.data());
        return std::nullopt;
    }
    return base_address + *hit;
}

}  // namespace dl::os
