#pragma once

/// @file unified_binary.hpp
/// @brief High-level serialization API (USER-FACING - NOT LEGACY)
///
/// This file provides the RECOMMENDED API for users:
/// - serialize(obj) → returns static_buffer
/// - deserialize<T>(data) → returns optional<T>
///
/// These functions delegate to StructLayout<T> internally for actual
/// serialization. This layer provides convenience wrappers and handles
/// both primitive types and structs transparently.
///
/// Architecture:
///   User Code → unified_binary.hpp → unified_api.hpp → StructLayout<T>
///
/// For direct StructLayout usage (advanced/hot paths):
///   #include <sertial/core/layout/struct_layout.hpp>

#include "../core/layout/struct_layout.hpp"
#include "../core/layout/unified_api.hpp"
#include "../containers/static_buffer.hpp"
#include "../containers/ring_buffer.hpp"
#include <cstring>
#include <span>
#include <optional>

namespace sertial {

// ============================================================================
// High-Level Serialization API
// ============================================================================

// Core implementations are now in core/layout/unified_api.hpp
// (serialize_to_unified, serialize_unified, deserialize_unified)

/// @brief Get the packed size for a value (runtime calculation)
template<typename T>
inline std::size_t packed_size_of(const T& value) {
    using Layout = StructLayout<T>;
    
    if constexpr (!Layout::has_variable_fields) {
        return Layout::base_packed_size;
    } else {
        // For variable-size types, we need to serialize to calculate actual size
        // This is a limitation we can improve later with a calculate_size() method
        auto buffer = serialize_unified(value);
        return buffer.size();
    }
}

// ============================================================================
// Convenience Functions (Backward-Compatible API)
// ============================================================================

/// @brief Serialize to a static buffer (convenience wrapper)
template<typename T>
[[nodiscard]] inline auto serialize(const T& value) {
    return serialize_unified(value);
}

/// @brief Serialize to raw pointer (convenience wrapper)
template<typename T>
inline std::size_t serialize_to(const T& value, std::byte* dest) {
    return serialize_to_unified(value, dest);
}

/// @brief Serialize to static_buffer (convenience wrapper)
template<typename T, std::size_t N>
inline std::size_t serialize_to(const T& value, static_buffer<N>& buffer) {
    const std::size_t size = serialize_to_unified(value, buffer.data());
    buffer.resize(size);
    return size;
}

/// @brief Deserialize from buffer (convenience wrapper, returns optional for compatibility)
template<typename T>
[[nodiscard]] inline std::optional<T> deserialize(std::span<const std::byte> data) {
    return deserialize_unified<T>(data);
}

/// @brief Deserialize from std::vector (convenience wrapper)
template<typename T>
[[nodiscard]] inline std::optional<T> deserialize(const std::vector<std::byte>& vec) {
    return deserialize_unified<T>(std::span{vec.data(), vec.size()});
}

/// @brief Deserialize from static_buffer (convenience wrapper)
template<typename T, std::size_t N>
[[nodiscard]] inline std::optional<T> deserialize(const static_buffer<N>& buffer) {
    return deserialize_unified<T>(std::span{buffer.data(), buffer.size()});
}

/// @brief Deserialize into an existing object
template<typename T>
[[nodiscard]] inline bool deserialize_into(std::span<const std::byte> data, T& out) {
    auto result = deserialize_unified<T>(data);
    if (result.has_value()) {
        out = std::move(*result);
        return true;
    }
    return false;
}

} // namespace sertial
