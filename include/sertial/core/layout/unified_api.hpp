#pragma once

/// @file unified_api.hpp
/// @brief Implementation layer for high-level serialization API
///
/// This file provides the implementation functions used by io/unified_binary.hpp:
/// - serialize_unified() - Delegates to StructLayout<T>::serialize()
/// - deserialize_unified() - Delegates to StructLayout<T>::deserialize_opt()
///
/// Handles primitives separately (direct memcpy without StructLayout overhead).
///
/// Users should NOT include this file directly - use sertial.hpp or
/// io/unified_binary.hpp instead.

#include "struct_layout.hpp"
#include "../../containers/static_buffer.hpp"
#include <span>
#include <optional>
#include <type_traits>

namespace sertial {

// ============================================================================
// Implementation Layer for High-Level API
// ============================================================================

// ============================================================================
// Primitive Type Handling (bypass StructLayout)
// ============================================================================

template<typename T>
concept PrimitiveType = std::is_arithmetic_v<T> || std::is_enum_v<T>;

/// @brief Serialize primitive to raw pointer
template<PrimitiveType T>
inline std::size_t serialize_to_unified(const T& value, std::byte* dest) {
    std::memcpy(dest, &value, sizeof(T));
    return sizeof(T);
}

/// @brief Serialize primitive to static_buffer
template<PrimitiveType T>
[[nodiscard]] inline auto serialize_unified(const T& value) {
    static_buffer<sizeof(T)> buffer;
    std::memcpy(buffer.data(), &value, sizeof(T));
    buffer.resize(sizeof(T));
    return buffer;
}

/// @brief Deserialize primitive from raw pointer
template<PrimitiveType T>
[[nodiscard]] inline std::optional<T> deserialize_unified(const std::byte* src, std::size_t size) {
    if (size < sizeof(T)) {
        return std::nullopt;
    }
    T result{};
    std::memcpy(&result, src, sizeof(T));
    return result;
}

/// @brief Deserialize primitive from span
template<PrimitiveType T>
[[nodiscard]] inline std::optional<T> deserialize_unified(std::span<const std::byte> src) {
    return deserialize_unified<T>(src.data(), src.size());
}

// ============================================================================
// Struct Type Handling (use StructLayout)
// ============================================================================

/// @brief Serialize to raw pointer (returns bytes written)
/// Compatibility wrapper for existing code
template<typename T>
    requires (!PrimitiveType<T>)
inline std::size_t serialize_to_unified(const T& value, std::byte* dest) {
    using Layout = StructLayout<T>;
    
    // Create a span from the dest pointer with max_packed_size
    // This gives us compile-time size validation via StructLayout
    std::array<std::byte, Layout::max_packed_size> temp_buffer;
    std::span<std::byte, Layout::max_packed_size> dest_span{temp_buffer};
    
    std::size_t bytes_written = Layout::serialize(value, dest_span);
    
    // Copy to dest pointer
    std::memcpy(dest, temp_buffer.data(), bytes_written);
    
    return bytes_written;
}

/// @brief Serialize to static_buffer (zero allocation)
template<typename T>
    requires (!PrimitiveType<T>)
[[nodiscard]] inline auto serialize_unified(const T& value) {
    using Layout = StructLayout<T>;
    
    static_buffer<Layout::max_packed_size> buffer;
    
    // Serialize directly to buffer (no temp copy needed)
    std::span<std::byte, Layout::max_packed_size> dest_span{buffer.data(), Layout::max_packed_size};
    std::size_t bytes_written = Layout::serialize(value, dest_span);
    buffer.resize(bytes_written);
    
    return buffer;
}

/// @brief Deserialize from raw pointer (returns optional)
template<typename T>
    requires (!PrimitiveType<T>)
[[nodiscard]] inline std::optional<T> deserialize_unified(const std::byte* src, std::size_t size) {
    using Layout = StructLayout<T>;
    return Layout::deserialize_opt(std::span{src, size});
}

/// @brief Deserialize from span (returns optional)
template<typename T>
    requires (!PrimitiveType<T>)
[[nodiscard]] inline std::optional<T> deserialize_unified(std::span<const std::byte> src) {
    using Layout = StructLayout<T>;
    return Layout::deserialize_opt(src);
}

} // namespace sertial
