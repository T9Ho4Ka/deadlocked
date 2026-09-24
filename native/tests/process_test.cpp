#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include "os/process.hpp"

#include "check.hpp"

using namespace dl::os;
using dl::test::check;

struct Entry { std::uint32_t id; float value; char pad[24]; };

int main() {
    Process self(::getpid());
    check(self.valid(), "process is valid");
    check(self.pid() == ::getpid(), "pid matches");

    // --- scalar reads out of our own memory ---
    const std::uint64_t magic = 0x0123456789ABCDEFull;
    check(self.read<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&magic)) == magic, "read u64");

    const float pi = 3.14159f;
    check(self.read<float>(reinterpret_cast<std::uintptr_t>(&pi)) == pi, "read float");

    const Entry entry{42, 1.5f, {}};
    const Entry got = self.read<Entry>(reinterpret_cast<std::uintptr_t>(&entry));
    check(got.id == 42 && got.value == 1.5f, "read struct");

    const char* text = "deadlocked native";
    check(self.read_string(reinterpret_cast<std::uintptr_t>(text)) == "deadlocked native", "read_string stops at nul");

    std::vector<std::uint8_t> blob(300);
    for (std::size_t i=0;i<blob.size();++i) blob[i] = static_cast<std::uint8_t>(i*7);
    const auto back = self.read_vec(reinterpret_cast<std::uintptr_t>(blob.data()), blob.size());
    check(back == blob, "read_vec 300 bytes");

    // --- strided read: grab only the id of each entry ---
    Entry entries[4]{{10,0,{}},{20,0,{}},{30,0,{}},{40,0,{}}};
    const auto ids = self.read_typed_vec<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(entries), sizeof(Entry), 4);
    check(ids.size()==4 && ids[0]==10 && ids[1]==20 && ids[2]==30 && ids[3]==40, "read_typed_vec honours stride");

    // --- failure behaviour ---
    std::uint64_t sink = 0xFFFF;
    check(!self.try_read<std::uint64_t>(0x10, sink), "try_read reports failure on a bad address");
    check(self.read<std::uint64_t>(0x10) == 0, "read returns zero on a bad address");

    Process dead(-1);
    check(!dead.valid(), "pid -1 is not valid");
    check(dead.read<int>(reinterpret_cast<std::uintptr_t>(&magic)) == 0, "invalid process reads zero");

    // --- pattern parsing ---
    std::vector<std::uint8_t> pat, mask;
    check(parse_pattern("48 8B ? 0F", pat, mask), "parse_pattern accepts a pattern");
    check(pat.size()==4 && mask.size()==4, "parse_pattern length");
    check(pat[0]==0x48 && pat[1]==0x8B && pat[3]==0x0F, "parse_pattern bytes");
    check(mask[0]==0xFF && mask[2]==0x00 && mask[3]==0xFF, "parse_pattern wildcard mask");
    check(!parse_pattern("   ", pat, mask), "parse_pattern rejects an empty pattern");
    check(parse_pattern("48 ZZ 8B", pat, mask) && pat.size()==2, "parse_pattern skips a bad token");

    // --- scanning ---
    std::vector<std::uint8_t> hay(1000, 0);
    for (std::size_t i=0;i<hay.size();++i) hay[i] = static_cast<std::uint8_t>(i % 251);
    std::vector<std::uint8_t> p{0xAA,0xBB,0xCC,0xDD};
    std::vector<std::uint8_t> m{0xFF,0xFF,0xFF,0xFF};
    std::memcpy(hay.data()+400, p.data(), 4);
    auto hit = scan_bytes(p, m, hay);
    check(hit.has_value() && *hit==400, "scan finds an exact pattern");

    std::vector<std::uint8_t> pw{0xAA,0x00,0xCC,0xDD};
    std::vector<std::uint8_t> mw{0xFF,0x00,0xFF,0xFF};
    hit = scan_bytes(pw, mw, hay);
    check(hit.has_value() && *hit==400, "scan honours wildcards");

    // 0xFE cannot occur in a haystack built from i % 251
    std::vector<std::uint8_t> absent(6, 0xFE);
    std::vector<std::uint8_t> am(absent.size(), 0xFF);
    check(!scan_bytes(absent, am, hay).has_value(), "scan reports a miss");

    // the match sits inside the last 32 bytes, where the vector path cannot look.
    // the rust version stops early here and misses it.
    std::vector<std::uint8_t> tail(1000, 0x00);
    std::memcpy(tail.data() + tail.size() - 6, p.data(), 4);
    hit = scan_bytes(p, m, tail);
    check(hit.has_value() && *hit==tail.size()-6, "scan finds a match in the last 32 bytes");

    // a pattern longer than one vector register must still work
    std::vector<std::uint8_t> big(40);
    for (std::size_t i=0;i<big.size();++i) big[i] = static_cast<std::uint8_t>(0xC0 + i);
    std::vector<std::uint8_t> bm(big.size(), 0xFF);
    std::vector<std::uint8_t> hay2(500, 0x00);
    std::memcpy(hay2.data()+100, big.data(), big.size());
    hit = scan_bytes(big, bm, hay2);
    check(hit.has_value() && *hit==100, "scan handles a pattern longer than 32 bytes");

    // --- modules, against this very binary ---
    const auto base = self.module_base_address("process_test");
    check(base.has_value(), "module_base_address finds this binary");
    if (base) {
        check(self.module_size(*base) > 1000, "module_size is plausible");
        const auto elf_magic = self.read_vec(*base, 4);
        check(elf_magic[0]==0x7F && elf_magic[1]=='E' && elf_magic[2]=='L' && elf_magic[3]=='F',
              "module base points at an ELF header");
    }
    check(!self.module_base_address("definitely_not_mapped_xyz").has_value(), "missing module reports nothing");

    check(!Process::open("definitely_not_a_process_xyz").has_value(), "open of a missing process fails");

    return dl::test::report();
}
