#pragma once

#include "struct_layout.hpp"
#include <rfl.hpp>
#include <string>
#include <vector>

// ============================================================================
// rfl::Reflector for StructLayout<T>
// ============================================================================
// Exposes StructLayout's compile-time metadata for schema generation.
// This allows rfl::json::to_schema to see all our metadata fields.
// ============================================================================

namespace rfl {

template<typename T>
struct Reflector<sertial::StructLayout<T>> {
    // Helper to extract field names from NamedTuple
    template<typename... Fields>
    static std::vector<std::string> extract_field_names(::rfl::NamedTuple<Fields...>*) {
        return std::vector<std::string>{
            std::string(Fields::name_.str())...
        };
    }
    
    // Helper to extract field type names
    template<typename... Fields>
    static std::vector<std::string> extract_field_types(::rfl::NamedTuple<Fields...>*) {
        return std::vector<std::string>{
            std::string(type_name_t<typename Fields::Type>().str())...
        };
    }
    
    // ReflType: struct with all metadata we want to export
    // ⚠️  MAINTENANCE NOTE: When adding fields to StructLayout, update:
    //     1. This ReflType struct (add corresponding runtime field)
    //     2. EXPECTED_FIELDS array below (add field name for documentation)
    //     3. EXPECTED_FIELD_COUNT constant (increment by 1)
    //     4. from() function (populate the new field from StructLayout)
    //     5. static_assert checks in from() (verify new StructLayout member exists)
    struct ReflType {
        std::string name;
        std::size_t sizeof_bytes;
        std::size_t base_packed_size;
        std::size_t max_packed_size;
        bool has_variable_fields;
        std::size_t field_count;
        
        // Field arrays (converted from std::array to std::vector for JSON)
        std::vector<std::string> field_names;
        std::vector<std::string> field_types;
        std::vector<std::size_t> field_sizes;
        std::vector<std::size_t> field_offsets;
        std::vector<std::size_t> field_alignments;
        std::vector<bool> field_is_variable;
        std::vector<std::size_t> element_sizes;
        std::vector<std::size_t> capacities;
        
        // Block counts
        std::size_t fixed_block_count;
        std::size_t padding_block_count;
        std::size_t dynamic_block_count;
        std::size_t runtime_offset_block_count;
        std::size_t total_blocks;
        
        // Underlying type schema (the actual struct T)
        std::string type_schema;
    };
    
    // ========================================================================
    // Compile-Time Consistency Checks
    // ========================================================================
    // These assertions ensure the reflector stays synchronized with StructLayout.
    // If StructLayout gains new metadata fields, these will trigger compile errors.
    
    // Verify we're exposing all expected metadata fields
    static constexpr std::size_t EXPECTED_FIELD_COUNT = 20;
    
    static_assert(
        rfl::internal::num_fields<ReflType> == EXPECTED_FIELD_COUNT,
        "ReflType field count mismatch! Expected 20 fields. "
        "If you added/removed fields in ReflType, update EXPECTED_FIELD_COUNT and EXPECTED_FIELDS."
    );
    
    // Document expected fields for maintainability (update this when adding fields!)
    static constexpr const char* EXPECTED_FIELDS[] = {
        "name", "sizeof_bytes", "base_packed_size", "max_packed_size",
        "has_variable_fields", "field_count", "field_names", "field_types",
        "field_sizes", "field_offsets", "field_alignments", "field_is_variable",
        "element_sizes", "capacities", "fixed_block_count", "padding_block_count",
        "dynamic_block_count", "runtime_offset_block_count", "total_blocks", "type_schema"
    };
    
    static_assert(
        sizeof(EXPECTED_FIELDS) / sizeof(EXPECTED_FIELDS[0]) == EXPECTED_FIELD_COUNT,
        "EXPECTED_FIELDS documentation array must list all ReflType fields. "
        "If you added a field to ReflType, add its name to EXPECTED_FIELDS array."
    );
    
    // Convert StructLayout's compile-time data to runtime struct
    static ReflType from(const sertial::StructLayout<T>&) {
        using Layout = sertial::StructLayout<T>;
        
        // Compile-time check: Ensure StructLayout has the expected constexpr members
        // If any of these fail, StructLayout's interface changed - update the reflector!
        static_assert(requires { Layout::num_fields; }, 
                      "StructLayout must have num_fields");
        static_assert(requires { Layout::base_packed_size; }, 
                      "StructLayout must have base_packed_size");
        static_assert(requires { Layout::max_packed_size; }, 
                      "StructLayout must have max_packed_size");
        static_assert(requires { Layout::has_variable_fields; }, 
                      "StructLayout must have has_variable_fields");
        static_assert(requires { Layout::field_sizes; }, 
                      "StructLayout must have field_sizes array");
        static_assert(requires { Layout::field_offsets; }, 
                      "StructLayout must have field_offsets array");
        static_assert(requires { Layout::field_alignments; }, 
                      "StructLayout must have field_alignments array");
        static_assert(requires { Layout::field_is_variable; }, 
                      "StructLayout must have field_is_variable array");
        static_assert(requires { Layout::element_sizes; }, 
                      "StructLayout must have element_sizes array");
        static_assert(requires { Layout::capacities; }, 
                      "StructLayout must have capacities array");
        static_assert(requires { Layout::fixed_block_count; }, 
                      "StructLayout must have fixed_block_count");
        static_assert(requires { Layout::dynamic_block_count; }, 
                      "StructLayout must have dynamic_block_count");
        static_assert(requires { Layout::runtime_offset_block_count; }, 
                      "StructLayout must have runtime_offset_block_count");
        static_assert(requires { Layout::total_blocks; }, 
                      "StructLayout must have total_blocks");
        
        ReflType result;
        result.name = std::string(type_name_t<T>().str());
        result.sizeof_bytes = sizeof(T);
        result.base_packed_size = Layout::base_packed_size;
        result.max_packed_size = Layout::max_packed_size;
        result.has_variable_fields = Layout::has_variable_fields;
        result.field_count = Layout::num_fields;
        
        // Extract field names and types using reflect-cpp's introspection
        if constexpr (Layout::num_fields > 0) {
            using NT = ::rfl::named_tuple_t<T>;
            result.field_names = extract_field_names(static_cast<NT*>(nullptr));
            result.field_types = extract_field_types(static_cast<NT*>(nullptr));
            
            // Convert constexpr arrays to vectors for JSON serialization
            result.field_sizes = std::vector<std::size_t>(
                Layout::field_sizes.begin(), 
                Layout::field_sizes.begin() + Layout::num_fields
            );
            result.field_offsets = std::vector<std::size_t>(
                Layout::field_offsets.begin(),
                Layout::field_offsets.begin() + Layout::num_fields
            );
            result.field_alignments = std::vector<std::size_t>(
                Layout::field_alignments.begin(),
                Layout::field_alignments.begin() + Layout::num_fields
            );
            result.field_is_variable = std::vector<bool>(
                Layout::field_is_variable.begin(),
                Layout::field_is_variable.begin() + Layout::num_fields
            );
            result.element_sizes = std::vector<std::size_t>(
                Layout::element_sizes.begin(),
                Layout::element_sizes.begin() + Layout::num_fields
            );
            result.capacities = std::vector<std::size_t>(
                Layout::capacities.begin(),
                Layout::capacities.begin() + Layout::num_fields
            );
        }
        
        // Block metadata
        result.fixed_block_count = Layout::fixed_block_count;
        result.padding_block_count = Layout::padding_block_count;
        result.dynamic_block_count = Layout::dynamic_block_count;
        result.runtime_offset_block_count = Layout::runtime_offset_block_count;
        result.total_blocks = Layout::total_blocks;
        
        // Include the schema of the underlying type T
        result.type_schema = json::to_schema<T>();
        
        return result;
    }
    
    // Note: We don't need to() because StructLayout is read-only for schema export
};

} // namespace rfl
