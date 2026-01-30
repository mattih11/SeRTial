#pragma once

#include "memory_map.hpp"
#include "../../traits/container_detection.hpp"
#include <cstddef>

namespace sertial {

// ============================================================================
// Hybrid Memory Map - Single Source of Truth
// ============================================================================
// Extends MemoryMap with variable-field awareness
// Uses existing MemoryMap regions but adds runtime size calculation
// ============================================================================

/// @brief Hybrid Memory Map - combines compile-time fixed regions with runtime variable fields
template<typename T>
struct HybridMemoryMap {
    using MM = MemoryMap<T>;
    
    // Reuse MemoryMap's compile-time analysis
    static constexpr bool can_single_memcpy = MM::can_single_memcpy;
    static constexpr std::size_t memcpy_region_count = MM::memcpy_region_count;
    static constexpr auto memcpy_regions = MM::memcpy_regions;
    
    // Check if struct has variable fields
    static constexpr bool has_variable_fields = detail::struct_has_fixed_containers<T>();
    
    // For fixed-size types, packed size is known at compile time  
    static constexpr std::size_t base_packed_size = has_variable_fields ? 0 : MM::packed_size;
    
    // For simple testing, expose some info
    static constexpr std::size_t copy_region_count = memcpy_region_count;
    static constexpr auto copy_regions = memcpy_regions;
    static constexpr std::size_t variable_field_count = has_variable_fields ? 1 : 0;  // Simplified
    
    // Runtime size calculation (returns packed_size for fixed types)
    static std::size_t calculate_packed_size([[maybe_unused]] const T& value) {
        if constexpr (!has_variable_fields) {
            return MM::packed_size;
        } else {
            // For variable-size types, would need to iterate fields and sum sizes
            // This requires more complex runtime analysis
            // For now, return base size (proof of concept)
            return MM::packed_size;
        }
    }
};

} // namespace sertial
