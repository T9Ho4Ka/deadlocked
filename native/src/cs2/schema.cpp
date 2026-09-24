#include "cs2/schema.hpp"

#include <cstdio>

#include "os/process.hpp"

namespace dl::cs2 {
namespace {

/// Layout of the game's schema structures. These are the numbers that go stale when valve
/// reorganises things, so they are named rather than sprinkled through the code.
namespace layout {
/// CSchemaSystem
constexpr std::uintptr_t type_scopes_length = 0x1F0;
constexpr std::uintptr_t type_scopes_vector = 0x1F8;

/// CSchemaSystemTypeScope
constexpr std::uintptr_t scope_name = 0x08;
constexpr std::uintptr_t scope_table = 0x560;
constexpr std::uintptr_t table_buckets = 0x90;
constexpr std::uintptr_t table_free_list = 0x20;
constexpr std::size_t bucket_count = 1024;
constexpr std::size_t bucket_stride = 24;
constexpr std::uintptr_t bucket_first_uncommitted = 0x28;
constexpr std::uintptr_t element_data = 0x10;
constexpr std::uintptr_t element_next = 0x08;

/// CSchemaClassInfo
constexpr std::uintptr_t class_name = 0x08;
constexpr std::uintptr_t class_size = 0x20;
constexpr std::uintptr_t class_field_count = 0x24;
constexpr std::uintptr_t class_fields = 0x30;
constexpr std::size_t field_stride = 0x20;
constexpr std::uintptr_t field_offset = 0x10;

/// a class with more fields than this is a misread, not a real class
constexpr std::int16_t max_field_count = 20000;
}  // namespace layout

/// every pointer walked here comes out of another process, so each walk is capped
constexpr std::size_t max_chain_steps = 100'000;

}  // namespace

Class::Class(const os::Process& process, std::uintptr_t address) {
    name_ = process.read_string(process.read<std::uintptr_t>(address + layout::class_name));
    size_ = process.read<std::int32_t>(address + layout::class_size);

    const auto field_count = process.read<std::int16_t>(address + layout::class_field_count);
    if (field_count <= 0 || field_count > layout::max_field_count) {
        return;
    }

    const auto fields = process.read<std::uintptr_t>(address + layout::class_fields);
    fields_.reserve(static_cast<std::size_t>(field_count));
    for (std::size_t i = 0; i < static_cast<std::size_t>(field_count); ++i) {
        const std::uintptr_t field = fields + layout::field_stride * i;
        std::string name = process.read_string(process.read<std::uintptr_t>(field));
        const auto offset = process.read<std::int32_t>(field + layout::field_offset);
        fields_.emplace(std::move(name), static_cast<std::uintptr_t>(offset));
    }
}

std::optional<std::uintptr_t> Class::get(const std::string& field) const {
    const auto found = fields_.find(field);
    if (found == fields_.end()) {
        return std::nullopt;
    }
    return found->second;
}

ModuleScope::ModuleScope(const os::Process& process, std::uintptr_t address) {
    name_ = process.read_string(address + layout::scope_name);

    const std::uintptr_t buckets = address + layout::scope_table + layout::table_buckets;
    for (std::size_t i = 0; i < layout::bucket_count; ++i) {
        std::uintptr_t element = process.read<std::uintptr_t>(
            buckets + i * layout::bucket_stride + layout::bucket_first_uncommitted);

        for (std::size_t step = 0; step < max_chain_steps && element != 0; ++step) {
            const auto data = process.read<std::uintptr_t>(element + layout::element_data);
            if (data != 0) {
                Class entry(process, data);
                classes_.insert_or_assign(entry.name(), std::move(entry));
            }
            element = process.read<std::uintptr_t>(element + layout::element_next);
        }
    }

    // classes that were allocated but not yet committed to a bucket live in the free list
    std::uintptr_t blob =
        process.read<std::uintptr_t>(address + layout::scope_table + layout::table_free_list);
    for (std::size_t step = 0; step < max_chain_steps && blob != 0; ++step) {
        const auto data = process.read<std::uintptr_t>(blob + layout::element_data);
        if (process.in_data_range(data)) {
            Class entry(process, data);
            classes_.insert_or_assign(entry.name(), std::move(entry));
        }
        blob = process.read<std::uintptr_t>(blob);
    }
}

const Class* ModuleScope::get_class(const std::string& name) const {
    const auto found = classes_.find(name);
    return found == classes_.end() ? nullptr : &found->second;
}

std::optional<std::uintptr_t> ModuleScope::get(const std::string& class_name,
                                               const std::string& field) const {
    const Class* entry = get_class(class_name);
    if (entry == nullptr) {
        std::fprintf(stderr, "could not find class %s\n", class_name.c_str());
        return std::nullopt;
    }
    const std::optional<std::uintptr_t> offset = entry->get(field);
    if (!offset.has_value()) {
        std::fprintf(stderr, "could not find field %s in class %s\n", field.c_str(),
                     class_name.c_str());
    }
    return offset;
}

std::optional<Schema> Schema::create(const os::Process& process, std::uintptr_t schema_module) {
    const std::optional<std::uintptr_t> found =
        process.scan("48 8b 05 ? ? ? ? 48 8d 15 ? ? ? ? 4c 89 e9", schema_module);
    if (!found.has_value()) {
        return std::nullopt;
    }

    const std::uintptr_t system =
        process.read<std::uintptr_t>(process.get_relative_address(*found, 3, 7));

    const auto length = process.read<std::int32_t>(system + layout::type_scopes_length);
    const auto vector = process.read<std::uintptr_t>(system + layout::type_scopes_vector);
    if (length <= 0) {
        return std::nullopt;
    }

    Schema schema;
    for (std::size_t i = 0; i < static_cast<std::size_t>(length); ++i) {
        ModuleScope scope(process, process.read<std::uintptr_t>(vector + i * 8));
        schema.scopes_.insert_or_assign(scope.name(), std::move(scope));
    }
    return schema;
}

const ModuleScope* Schema::get_library(const std::string& library) const {
    const auto found = scopes_.find(library);
    return found == scopes_.end() ? nullptr : &found->second;
}

std::optional<std::uintptr_t> Schema::get(const std::string& library,
                                          const std::string& class_name,
                                          const std::string& field) const {
    const ModuleScope* scope = get_library(library);
    if (scope == nullptr) {
        return std::nullopt;
    }
    return scope->get(class_name, field);
}

}  // namespace dl::cs2
