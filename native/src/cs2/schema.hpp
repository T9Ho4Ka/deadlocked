#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace dl::os {
class Process;
}

namespace dl::cs2 {

/// One class the game's schema system knows about, with its field offsets by name.
class Class {
public:
    Class() = default;
    Class(const os::Process& process, std::uintptr_t address);

    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] std::int32_t size() const { return size_; }
    [[nodiscard]] std::optional<std::uintptr_t> get(const std::string& field) const;

private:
    std::string name_;
    std::unordered_map<std::string, std::uintptr_t> fields_;
    std::int32_t size_ = 0;
};

/// The classes one game library registered.
class ModuleScope {
public:
    ModuleScope() = default;
    ModuleScope(const os::Process& process, std::uintptr_t address);

    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] const Class* get_class(const std::string& name) const;
    /// Field offset, complaining to stderr about whichever half was not found.
    [[nodiscard]] std::optional<std::uintptr_t> get(const std::string& class_name,
                                                    const std::string& field) const;
    [[nodiscard]] std::size_t class_count() const { return classes_.size(); }

private:
    std::string name_;
    std::unordered_map<std::string, Class> classes_;
};

/// The game's schema system: every class of every loaded library, with the field offsets
/// that change between game updates. Ported from cheat/src/cs2/schema.rs.
class Schema {
public:
    /// Reads the whole schema out of the running game. Empty when the schema system cannot
    /// be located, which usually means the pattern needs updating after a game patch.
    static std::optional<Schema> create(const os::Process& process,
                                        std::uintptr_t schema_module);

    [[nodiscard]] const ModuleScope* get_library(const std::string& library) const;
    [[nodiscard]] std::optional<std::uintptr_t> get(const std::string& library,
                                                    const std::string& class_name,
                                                    const std::string& field) const;
    [[nodiscard]] std::size_t scope_count() const { return scopes_.size(); }

private:
    std::unordered_map<std::string, ModuleScope> scopes_;
};

}  // namespace dl::cs2
