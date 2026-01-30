#pragma once

#include "../core/traits/hybrid_memory_map.hpp"
#include "optimized_binary.hpp"
#include <cstring>
#include <span>
#include <optional>
#include <vector>

namespace sertial {

// ============================================================================
// Unified Optimized Binary Serialization
// ============================================================================
// Single source of truth using HybridMemoryMap
// Currently delegates to optimized_binary for fixed-size types
// Future: Will add variable-field support
// ============================================================================

/// @brief Serialize to a dynamically-sized buffer
template<typename T>
[[nodiscard]] inline std::vector<std::byte> serialize_unified(const T& value) {
    using HMM = HybridMemoryMap<T>;
    if constexpr (!HMM::has_variable_fields) {
        // Use existing optimized path for fixed types
        auto buf = sertial::serialize(value);
        return std::vector<std::byte>(buf.begin(), buf.end());
    } else {
        // TODO: Implement variable field serialization
        return {};
    }
}

/// @brief Serialize to raw pointer (returns bytes written)
template<typename T>
inline std::size_t serialize_to_unified(const T& value, std::byte* dest) {
    using HMM = HybridMemoryMap<T>;
    if constexpr (!HMM::has_variable_fields) {
        return sertial::serialize_to(value, dest);
    } else {
        // TODO: Implement variable field serialization
        return 0;
    }
}

/// @brief Get the packed size for a value
template<typename T>
constexpr std::size_t packed_size_of(const T& value) {
    return HybridMemoryMap<T>::calculate_packed_size(value);
}

} // namespace sertial
