#pragma once

#include <string>
#include <vector>
#include <cstddef>

namespace sertial {

// ============================================================================
// Schema Export Types - JSON Schema Generation
// ============================================================================
// These types are used ONLY for schema generation and JSON export.
// They use runtime allocations (std::vector, std::string) which is acceptable
// because they are NOT in the serialization hot path.
//
// Used by: SchemaGenerator<T>, visualize_schema.py, visualize_schema_gui.py
// ============================================================================

/// @brief Detailed information about a struct field
/// 
/// Contains all metadata needed for schema visualization and introspection.
/// Note: Uses std::string and is allocated at schema generation time, not
/// during message serialization.
struct FieldInfo {
    std::string name;                ///< Field name from reflection
    std::string type;                ///< C++ type name (e.g., "float", "fixed_vector<uint32_t, 10>")
    std::size_t unpacked_offset;     ///< Offset in C++ struct (with padding)
    std::size_t unpacked_size;       ///< Size in C++ struct (sizeof)
    std::size_t packed_offset;       ///< Offset in serialized format (no padding)
    std::size_t padding_before;      ///< Alignment padding before this field
    bool is_variable_length;         ///< true for fixed_vector, fixed_string, RingBuffer, etc.
    std::size_t element_size;        ///< sizeof(T) for container<T>, 0 for non-containers
    std::size_t max_elements;        ///< Capacity for containers, 0 for fixed types
    
    FieldInfo() = default;
    
    FieldInfo(std::string n, std::string t, std::size_t uoff, std::size_t usz,
              std::size_t poff, std::size_t pad = 0, bool var = false,
              std::size_t elem = 0, std::size_t max_elem = 0)
        : name(std::move(n)), type(std::move(t)), unpacked_offset(uoff),
          unpacked_size(usz), packed_offset(poff), padding_before(pad),
          is_variable_length(var), element_size(elem), max_elements(max_elem) {}
};

/// @brief Describes a contiguous memory region for memcpy optimization
/// 
/// Used to show which fields can be copied together in a single operation.
struct MemcpyRegion {
    std::size_t field_start;         ///< First field index
    std::size_t field_count;         ///< Number of consecutive fields
    std::size_t src_offset;          ///< Offset in struct
    std::size_t dst_offset;          ///< Offset in packed format
    std::size_t size;                ///< Total bytes
    
    MemcpyRegion() = default;
    
    MemcpyRegion(std::size_t fs, std::size_t fc, std::size_t src,
                 std::size_t dst, std::size_t sz)
        : field_start(fs), field_count(fc), src_offset(src),
          dst_offset(dst), size(sz) {}
};

/// @brief Information about a serialization block
/// 
/// Used for schema export to show how the struct is divided into
/// serialization blocks (Fixed, Padding, Dynamic, RuntimeOffset).
struct BlockInfo {
    std::string type;                ///< "Fixed", "Padding", "Dynamic", "RuntimeOffset"
    std::size_t src_offset;          ///< Offset in struct
    std::size_t dst_offset;          ///< Offset in packed format (for Fixed/Dynamic)
    std::size_t size;                ///< Block size in bytes
    std::size_t field_start;         ///< First field index
    std::size_t field_count;         ///< Number of fields
    bool is_dynamic;                 ///< true for Dynamic blocks
    std::size_t element_size;        ///< For Dynamic blocks: sizeof(element)
    std::size_t capacity;            ///< For Dynamic blocks: max elements
    
    BlockInfo() = default;
    
    BlockInfo(std::string t, std::size_t src, std::size_t dst, std::size_t sz,
              std::size_t fs = 0, std::size_t fc = 0, bool dyn = false,
              std::size_t elem = 0, std::size_t cap = 0)
        : type(std::move(t)), src_offset(src), dst_offset(dst), size(sz),
          field_start(fs), field_count(fc), is_dynamic(dyn),
          element_size(elem), capacity(cap) {}
};

/// @brief Complete schema for a type (JSON export)
/// 
/// This is the root object exported to JSON for schema visualization.
/// Contains all metadata about the type's layout, fields, and serialization.
/// 
/// WARNING: Uses std::vector which allocates. This is ONLY for schema export,
/// NOT for serialization hot paths.
struct TypeSchema {
    std::string name;                ///< Type name
    std::string category;            ///< "fixed", "dynamic", "mixed"
    std::size_t unpacked_size;       ///< sizeof(T)
    std::size_t base_packed_size;    ///< Size with no dynamic content (fixed fields only)
    std::size_t max_packed_size;     ///< Worst-case size (all containers at max capacity)
    std::size_t padding_bytes;       ///< Total alignment padding in struct
    bool has_variable_fields;        ///< Contains dynamic containers?
    std::vector<FieldInfo> field_info;     ///< Per-field metadata
    std::vector<BlockInfo> blocks;         ///< Serialization blocks
    std::vector<MemcpyRegion> memcpy_regions;  ///< Optimization info
    
    TypeSchema() = default;
    
    TypeSchema(std::string n, std::string cat, std::size_t usz, std::size_t bps,
               std::size_t mps, std::size_t pad, bool var)
        : name(std::move(n)), category(std::move(cat)), unpacked_size(usz),
          base_packed_size(bps), max_packed_size(mps), padding_bytes(pad),
          has_variable_fields(var) {}
};

} // namespace sertial
