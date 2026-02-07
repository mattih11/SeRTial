#pragma once

#include "../core/layout/struct_layout.hpp"
#include "../core/layout/struct_layout_reflector.hpp"  // Reflector for StructLayout
#include "../containers/reflectors.hpp"  // Enable rfl reflection for our containers
#include <rfl.hpp>
#include <rfl/json.hpp>
#include <string>

namespace sertial {

// ============================================================================
// Schema Export - StructLayout with full metadata via rfl::Reflector
// ============================================================================
// Uses rfl::Reflector<StructLayout<T>> to expose all compile-time metadata
// (field names, sizes, offsets, capacities, blocks, etc.) + nested type schema
// ============================================================================

/// @brief Export JSON Schema (type definition) for StructLayout<T>
/// 
/// This generates the JSON Schema describing the structure of StructLayout<T>,
/// showing what fields exist and their types. NO actual values are included.
/// 
/// Use this for schema documentation and type introspection.
template<typename T>
std::string export_schema() {
    // StructLayout<T> has a reflector that exposes:
    // - All field metadata (names, types, sizes, offsets, alignments)
    // - Container metadata (capacities, element_sizes)
    // - Block information (fixed/dynamic/runtime_offset counts)
    // - Packed size calculations (base_packed_size, max_packed_size)
    // - The actual type schema nested inside (type_schema field)
    
    // Generate JSON schema - includes all metadata from reflector
    return rfl::json::to_schema<StructLayout<T>>();
}

/// @brief Export actual runtime data from StructLayout<T>
/// 
/// This generates a JSON object with POPULATED values from StructLayout<T>.
/// All compile-time metadata (field_names, sizes, offsets, capacities, etc.)
/// is included with actual values - ready for Python tools to consume.
/// 
/// Use this for visualization tools that need the actual metadata values.
template<typename T>
std::string export_layout_data() {
    // Create StructLayout instance (all data is compile-time, so default constructor works)
    StructLayout<T> layout;
    
    // Use rfl::json::write to serialize the actual data via our reflector
    // This produces: {"name": "MyType", "sizeof_bytes": 42, "field_names": ["a","b"], ...}
    return rfl::json::write(layout);
}

} // namespace sertial
