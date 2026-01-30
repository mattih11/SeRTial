#pragma once

#include "padding.hpp"
#include "../../containers/container_traits.hpp"
#include <cstddef>
#include <type_traits>
#include <array>
#include <tuple>
#include <rfl.hpp>

namespace sertial {

// ============================================================================
// Field Information (for JSON serialization)
// ============================================================================

/// @brief Information about a single field in a struct
struct FieldInfo {
    std::size_t index = 0;          ///< Field index (0-based)
    std::size_t offset = 0;         ///< Offset in original struct (with padding)
    std::size_t size = 0;           ///< Size of the field type
    std::size_t packed_offset = 0;  ///< Offset in packed representation
    std::size_t padding_before = 0; ///< Padding bytes before this field
    
    // Variable-length container info
    bool is_variable_length = false;   ///< Is this a variable-length container?
    std::size_t element_size = 0;      ///< Size of each element (for containers)
    std::size_t max_elements = 0;      ///< Maximum elements (for fixed-capacity containers)
    std::size_t header_size = 0;       ///< Size of length prefix in serialized form
};

/// @brief Information about a contiguous memory region for memcpy
struct MemcpyRegion {
    std::size_t src_offset = 0;     ///< Offset in source struct
    std::size_t dst_offset = 0;     ///< Offset in destination buffer
    std::size_t size = 0;           ///< Number of bytes to copy
    std::size_t field_start = 0;    ///< First field index in this region
    std::size_t field_count = 0;    ///< Number of fields in this region
};

/// @brief Information about a serialization block
struct BlockInfo {
    std::string type;               ///< "Fixed", "Padding", "Dynamic", "RuntimeOffset"
    std::size_t src_offset = 0;     ///< Offset in struct
    std::size_t dst_offset = 0;     ///< Offset in serialized buffer (0 for dynamic/runtime)
    std::size_t size = 0;           ///< Size in bytes (0 for dynamic)
    std::size_t field_index = 0;    ///< Field index (for Dynamic blocks)
    std::size_t field_start = 0;    ///< First field in block
    std::size_t field_count = 0;    ///< Number of fields in block
    bool is_variable = false;       ///< True for Dynamic blocks
};

/// @brief Complete schema for a type - JSON serializable via rfl::json
struct TypeSchema {
    std::string name;
    std::string category;
    
    // Layout info
    std::size_t sizeof_bytes = 0;
    std::size_t packed_size = 0;
    std::size_t padding_bytes = 0;
    std::size_t field_count = 0;
    
    // Flags
    bool has_padding = false;
    bool can_single_memcpy = false;
    std::size_t memcpy_region_count = 0;
    
    // Hybrid memory map info
    bool has_variable_fields = false;
    std::size_t base_packed_size = 0;
    std::size_t fixed_block_count = 0;
    std::size_t dynamic_block_count = 0;
    std::size_t runtime_offset_block_count = 0;
    
    // Field details (from MemoryMap<T>)
    std::vector<std::string> field_names;
    std::vector<std::string> field_types;
    std::vector<FieldInfo> field_info;
    std::vector<MemcpyRegion> memcpy_regions;
    std::vector<BlockInfo> blocks;          ///< Serialization blocks in execution order
};

// ============================================================================
// Variable-Length Field Detection
// ============================================================================

namespace detail {

/// @brief Detect if a type is a variable-length container (fixed_vector, fixed_string, std::vector, std::string)
template<typename T>
struct is_variable_length_field : std::false_type {};

template<typename T, std::size_t N>
struct is_variable_length_field<fixed_vector<T, N>> : std::true_type {};

template<std::size_t N>
struct is_variable_length_field<fixed_string<N>> : std::true_type {};

template<typename T, typename A>
struct is_variable_length_field<std::vector<T, A>> : std::true_type {};

template<>
struct is_variable_length_field<std::string> : std::true_type {};

template<typename T>
inline constexpr bool is_variable_length_field_v = is_variable_length_field<T>::value;

/// @brief Get element size for a container type (0 for non-containers)
template<typename T>
struct variable_length_element_size {
    static constexpr std::size_t value = 0;
};

template<typename T, std::size_t N>
struct variable_length_element_size<fixed_vector<T, N>> {
    static constexpr std::size_t value = sizeof(T);
};

template<std::size_t N>
struct variable_length_element_size<fixed_string<N>> {
    static constexpr std::size_t value = sizeof(char);
};

template<typename T, typename A>
struct variable_length_element_size<std::vector<T, A>> {
    static constexpr std::size_t value = sizeof(T);
};

template<>
struct variable_length_element_size<std::string> {
    static constexpr std::size_t value = sizeof(char);
};

template<typename T>
inline constexpr std::size_t variable_length_element_size_v = variable_length_element_size<T>::value;

/// @brief Get max elements for a fixed-capacity container (0 for unbounded)
template<typename T>
struct variable_length_max_elements {
    static constexpr std::size_t value = 0;
};

template<typename T, std::size_t N>
struct variable_length_max_elements<fixed_vector<T, N>> {
    static constexpr std::size_t value = N;
};

template<std::size_t N>
struct variable_length_max_elements<fixed_string<N>> {
    static constexpr std::size_t value = N;
};

template<typename T>
inline constexpr std::size_t variable_length_max_elements_v = variable_length_max_elements<T>::value;

} // namespace detail (variable-length)

// ============================================================================
// Compile-Time Field Layout Computation
// ============================================================================

namespace detail {

/// @brief Count fields in a NamedTuple
template<typename... Fields>
constexpr std::size_t named_tuple_field_count(rfl::NamedTuple<Fields...>*) {
    return sizeof...(Fields);
}

/// @brief Count fields in a reflectable type
template<typename T>
constexpr std::size_t field_count_impl() {
    if constexpr (has_named_tuple_t_v<T>) {
        using NT = rfl::named_tuple_t<T>;
        return named_tuple_field_count(static_cast<NT*>(nullptr));
    } else {
        return 0;
    }
}

/// @brief Build array of field sizes from NamedTuple type
template<typename... Fields>
constexpr auto build_field_sizes_from_nt(rfl::NamedTuple<Fields...>*) {
    return std::array<std::size_t, sizeof...(Fields)>{
        sizeof(field_type_t<Fields>)...
    };
}

/// @brief Build array of field alignments from NamedTuple type
template<typename... Fields>
constexpr auto build_field_alignments_from_nt(rfl::NamedTuple<Fields...>*) {
    return std::array<std::size_t, sizeof...(Fields)>{
        alignof(field_type_t<Fields>)...
    };
}

/// @brief Build vector of field names from NamedTuple type (runtime)
template<typename... Fields>
auto build_field_names_from_nt(rfl::NamedTuple<Fields...>*) {
    return std::vector<std::string>{ std::string(Fields::name())... };
}

/// @brief Build vector of field type names from NamedTuple type (runtime)
template<typename... Fields>
auto build_field_type_names_from_nt(rfl::NamedTuple<Fields...>*) {
    return std::vector<std::string>{ std::string(rfl::type_name_t<field_type_t<Fields>>().str())... };
}

/// @brief Build array of is_variable_length flags from NamedTuple type
template<typename... Fields>
constexpr auto build_field_is_variable_from_nt(rfl::NamedTuple<Fields...>*) {
    return std::array<bool, sizeof...(Fields)>{
        is_variable_length_field_v<field_type_t<Fields>>...
    };
}

/// @brief Build array of element sizes from NamedTuple type
template<typename... Fields>
constexpr auto build_field_element_sizes_from_nt(rfl::NamedTuple<Fields...>*) {
    return std::array<std::size_t, sizeof...(Fields)>{
        variable_length_element_size_v<field_type_t<Fields>>...
    };
}

/// @brief Build array of max elements from NamedTuple type
template<typename... Fields>
constexpr auto build_field_max_elements_from_nt(rfl::NamedTuple<Fields...>*) {
    return std::array<std::size_t, sizeof...(Fields)>{
        variable_length_max_elements_v<field_type_t<Fields>>...
    };
}

template<typename T>
constexpr auto build_field_sizes() {
    if constexpr (has_named_tuple_t_v<T>) {
        using NT = rfl::named_tuple_t<T>;
        return build_field_sizes_from_nt(static_cast<NT*>(nullptr));
    } else {
        return std::array<std::size_t, 0>{};
    }
}

template<typename T>
constexpr auto build_field_alignments() {
    if constexpr (has_named_tuple_t_v<T>) {
        using NT = rfl::named_tuple_t<T>;
        return build_field_alignments_from_nt(static_cast<NT*>(nullptr));
    } else {
        return std::array<std::size_t, 0>{};
    }
}

template<typename T>
constexpr auto build_field_is_variable() {
    if constexpr (has_named_tuple_t_v<T>) {
        using NT = rfl::named_tuple_t<T>;
        return build_field_is_variable_from_nt(static_cast<NT*>(nullptr));
    } else {
        return std::array<bool, 0>{};
    }
}

template<typename T>
constexpr auto build_field_element_sizes() {
    if constexpr (has_named_tuple_t_v<T>) {
        using NT = rfl::named_tuple_t<T>;
        return build_field_element_sizes_from_nt(static_cast<NT*>(nullptr));
    } else {
        return std::array<std::size_t, 0>{};
    }
}

template<typename T>
constexpr auto build_field_max_elements() {
    if constexpr (has_named_tuple_t_v<T>) {
        using NT = rfl::named_tuple_t<T>;
        return build_field_max_elements_from_nt(static_cast<NT*>(nullptr));
    } else {
        return std::array<std::size_t, 0>{};
    }
}

/// @brief Compute field offsets based on sizes and alignments
template<std::size_t N>
constexpr auto compute_field_offsets(
    const std::array<std::size_t, N>& sizes,
    const std::array<std::size_t, N>& alignments
) {
    std::array<std::size_t, N> offsets{};
    std::size_t current = 0;
    
    for (std::size_t i = 0; i < N; ++i) {
        // Align to field's alignment requirement
        std::size_t align = alignments[i];
        std::size_t aligned = (current + align - 1) & ~(align - 1);
        offsets[i] = aligned;
        current = aligned + sizes[i];
    }
    
    return offsets;
}

/// @brief Compute padding before each field
template<std::size_t N>
constexpr auto compute_padding_before(
    const std::array<std::size_t, N>& sizes,
    const std::array<std::size_t, N>& offsets
) {
    std::array<std::size_t, N> padding{};
    std::size_t expected = 0;
    
    for (std::size_t i = 0; i < N; ++i) {
        padding[i] = offsets[i] - expected;
        expected = offsets[i] + sizes[i];
    }
    
    return padding;
}

/// @brief Compute packed offsets (no padding)
template<std::size_t N>
constexpr auto compute_packed_offsets(const std::array<std::size_t, N>& sizes) {
    std::array<std::size_t, N> offsets{};
    std::size_t current = 0;
    
    for (std::size_t i = 0; i < N; ++i) {
        offsets[i] = current;
        current += sizes[i];
    }
    
    return offsets;
}

/// @brief Count memcpy regions needed (consecutive fields without padding between)
template<std::size_t N>
constexpr std::size_t count_memcpy_regions(const std::array<std::size_t, N>& padding_before) {
    if (N == 0) return 0;
    
    std::size_t regions = 1;  // At least one region
    for (std::size_t i = 1; i < N; ++i) {
        if (padding_before[i] > 0) {
            ++regions;  // Padding breaks the region
        }
    }
    return regions;
}

/// @brief Build memcpy regions array
template<std::size_t N, std::size_t MaxRegions>
constexpr auto build_memcpy_regions(
    const std::array<std::size_t, N>& sizes,
    const std::array<std::size_t, N>& offsets,
    const std::array<std::size_t, N>& packed_offsets,
    const std::array<std::size_t, N>& padding_before
) {
    std::array<MemcpyRegion, MaxRegions> regions{};
    
    if (N == 0) return regions;
    
    std::size_t region_idx = 0;
    std::size_t region_start_field = 0;
    std::size_t region_src_start = offsets[0];
    std::size_t region_dst_start = packed_offsets[0];
    std::size_t region_size = sizes[0];
    
    for (std::size_t i = 1; i < N; ++i) {
        if (padding_before[i] > 0) {
            // End current region
            regions[region_idx] = MemcpyRegion{
                region_src_start,
                region_dst_start,
                region_size,
                region_start_field,
                i - region_start_field
            };
            ++region_idx;
            
            // Start new region
            region_start_field = i;
            region_src_start = offsets[i];
            region_dst_start = packed_offsets[i];
            region_size = sizes[i];
        } else {
            // Extend current region
            region_size += sizes[i];
        }
    }
    
    // Final region
    regions[region_idx] = MemcpyRegion{
        region_src_start,
        region_dst_start,
        region_size,
        region_start_field,
        N - region_start_field
    };
    
    return regions;
}

} // namespace detail

// ============================================================================
// MemoryMap Template - Compile-Time Layout Analysis
// ============================================================================

/// @brief Compile-time memory layout analysis for a type
template<typename T>
struct MemoryMap {
    // ========================================================================
    // Type Properties
    // ========================================================================
    
    /// @brief Number of fields in the struct
    static constexpr std::size_t field_count = detail::field_count_impl<T>();
    
    /// @brief Total size with padding (sizeof)
    static constexpr std::size_t unpacked_size = sizeof(T);
    
    /// @brief Total size without padding
    static constexpr std::size_t packed_size = packed_size_v<T>;
    
    /// @brief Padding bytes total
    static constexpr std::size_t padding_bytes = unpacked_size - packed_size;
    
    /// @brief Does the struct have any padding?
    static constexpr bool has_padding = (padding_bytes > 0);
    
    // ========================================================================
    // Field Layout (compile-time arrays)
    // ========================================================================
    
    /// @brief Array of field sizes
    static constexpr auto field_sizes = detail::build_field_sizes<T>();
    
    /// @brief Array of field alignments
    static constexpr auto field_alignments = detail::build_field_alignments<T>();
    
    /// @brief Array of field offsets (in struct, with padding)
    static constexpr auto field_offsets = 
        detail::compute_field_offsets(field_sizes, field_alignments);
    
    /// @brief Array of packed offsets (no padding)
    static constexpr auto packed_offsets = 
        detail::compute_packed_offsets(field_sizes);
    
    /// @brief Array of padding bytes before each field
    static constexpr auto padding_before = 
        detail::compute_padding_before(field_sizes, field_offsets);
    
    // ========================================================================
    // Variable-Length Field Detection (compile-time arrays)
    // ========================================================================
    
    /// @brief Array of is_variable_length flags
    static constexpr auto field_is_variable = detail::build_field_is_variable<T>();
    
    /// @brief Array of element sizes (for containers, 0 otherwise)
    static constexpr auto field_element_sizes = detail::build_field_element_sizes<T>();
    
    /// @brief Array of max elements (for fixed-capacity containers, 0 otherwise)
    static constexpr auto field_max_elements = detail::build_field_max_elements<T>();
    
    // ========================================================================
    // Memcpy Optimization
    // ========================================================================
    
    /// @brief Can we copy the entire struct with one memcpy?
    static constexpr bool can_single_memcpy = 
        !has_padding && std::is_trivially_copyable_v<T>;
    
    /// @brief Number of memcpy operations needed (consecutive field groups)
    static constexpr std::size_t memcpy_region_count = 
        can_single_memcpy ? 1 : detail::count_memcpy_regions(padding_before);
    
    /// @brief Array of memcpy regions
    static constexpr auto memcpy_regions = 
        detail::build_memcpy_regions<field_count, (field_count > 0 ? field_count : 1)>(
            field_sizes, field_offsets, packed_offsets, padding_before
        );
    
    // ========================================================================
    // Runtime Field Info Generation
    // ========================================================================
    
    /// @brief Get field names (runtime, for JSON export)
    static auto get_field_names() {
        if constexpr (detail::has_named_tuple_t_v<T>) {
            using NT = rfl::named_tuple_t<T>;
            return detail::build_field_names_from_nt(static_cast<NT*>(nullptr));
        } else {
            return std::vector<std::string>{};
        }
    }
    
    /// @brief Get field type names (runtime, for JSON export)
    static auto get_field_type_names() {
        if constexpr (detail::has_named_tuple_t_v<T>) {
            using NT = rfl::named_tuple_t<T>;
            return detail::build_field_type_names_from_nt(static_cast<NT*>(nullptr));
        } else {
            return std::vector<std::string>{};
        }
    }
    
    /// @brief Get field info for all fields (for JSON export)
    static auto get_field_infos() {
        std::vector<FieldInfo> infos;
        infos.reserve(field_count);
        
        for (std::size_t i = 0; i < field_count; ++i) {
            FieldInfo info;
            info.index = i;
            info.offset = field_offsets[i];
            info.size = field_sizes[i];
            info.packed_offset = packed_offsets[i];
            info.padding_before = padding_before[i];
            info.is_variable_length = field_is_variable[i];
            info.element_size = field_element_sizes[i];
            info.max_elements = field_max_elements[i];
            // header_size: varint length prefix (estimate 1-2 bytes for typical sizes)
            info.header_size = field_is_variable[i] ? 4 : 0;  // Using uint32_t for length
            infos.push_back(info);
        }
        return infos;
    }
    
    /// @brief Get memcpy regions (for JSON export)
    static auto get_memcpy_regions() {
        std::vector<MemcpyRegion> regions;
        regions.reserve(memcpy_region_count);
        
        for (std::size_t i = 0; i < memcpy_region_count; ++i) {
            regions.push_back(memcpy_regions[i]);
        }
        return regions;
    }
    
    /// @brief Get complete schema for this type (for JSON export)
    /// Uses rfl::type_name_t<T> to get the type name automatically
    static TypeSchema get_schema() {
        std::string full_name = rfl::type_name_t<T>().str();
        
        // Extract short name (last component after ::)
        std::string name = full_name;
        auto last_colon = full_name.rfind("::");
        if (last_colon != std::string::npos) {
            name = full_name.substr(last_colon + 2);
        }
        
        // Extract category from namespace (e.g., "geometry" from "...::geometry::Point2D")
        std::string category;
        if (last_colon != std::string::npos) {
            auto prev_colon = full_name.rfind("::", last_colon - 1);
            if (prev_colon != std::string::npos) {
                category = full_name.substr(prev_colon + 2, last_colon - prev_colon - 2);
            }
        }
        
        return TypeSchema{
            name,
            category,
            unpacked_size,
            packed_size,
            padding_bytes,
            field_count,
            has_padding,
            can_single_memcpy,
            memcpy_region_count,
            false,  // has_variable_fields (populated by hybrid schema)
            0,      // base_packed_size
            0,      // fixed_block_count
            0,      // dynamic_block_count
            0,      // runtime_offset_block_count
            get_field_names(),
            get_field_type_names(),
            get_field_infos(),
            get_memcpy_regions(),
            {}      // blocks (populated by hybrid schema)
        };
    }
    
    /// @brief Get complete schema with custom name/category (for JSON export)
    static TypeSchema get_schema(const std::string& name, const std::string& category = "") {
        return TypeSchema{
            name,
            category,
            unpacked_size,
            packed_size,
            padding_bytes,
            field_count,
            has_padding,
            can_single_memcpy,
            memcpy_region_count,
            false,  // has_variable_fields
            0,      // base_packed_size
            0,      // fixed_block_count
            0,      // dynamic_block_count
            0,      // runtime_offset_block_count
            get_field_names(),
            get_field_type_names(),
            get_field_infos(),
            get_memcpy_regions(),
            {}      // blocks
        };
    }
};

// ============================================================================
// Convenience Aliases
// ============================================================================

template<typename T>
inline constexpr std::size_t field_count_v = MemoryMap<T>::field_count;

template<typename T>
inline constexpr std::size_t memcpy_region_count_v = MemoryMap<T>::memcpy_region_count;

template<typename T>
inline constexpr bool can_single_memcpy_v = MemoryMap<T>::can_single_memcpy;

} // namespace sertial
