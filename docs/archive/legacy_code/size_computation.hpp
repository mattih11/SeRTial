#pragma once

#include "traits.hpp"
#include "concepts.hpp"
#include "../containers/container_traits.hpp"
#include "../containers/fixed_vector.hpp"
#include "../containers/fixed_string.hpp"
#include "../io/varint.hpp"
#include <type_traits>
#include <string>
#include <string_view>

namespace sertial {

// ============================================================================
// Compile-Time Size Computation
// ============================================================================
// Calculate buffer size needed for serialization at compile-time when possible
// - Static types: exact size known at compile-time
// - Dynamic types: must compute at runtime
// - Trailing types: partial compile-time + runtime computation

// ============================================================================
// Helper: Compute serialized size for a value
// ============================================================================

/// Compute size for arithmetic types (compile-time)
template<Arithmetic T>
constexpr std::size_t compute_serialized_size(const T&) noexcept {
    return sizeof(T);
}

/// Compute size for bool (compile-time)
constexpr std::size_t compute_serialized_size(bool) noexcept {
    return 1;
}

/// Compute size for string_view (runtime: varint length + bytes)
inline std::size_t compute_serialized_size(std::string_view str) noexcept {
    return varint_size(str.size()) + str.size();
}

/// Compute size for std::string (runtime)
inline std::size_t compute_serialized_size(const std::string& str) noexcept {
    return compute_serialized_size(std::string_view(str));
}

/// Compute size for C-string (runtime)
inline std::size_t compute_serialized_size(const char* str) noexcept {
    return compute_serialized_size(std::string_view(str));
}

/// Compute size for fixed_string (runtime)
template<std::size_t N>
inline std::size_t compute_serialized_size(const fixed_string<N>& str) noexcept {
    return compute_serialized_size(std::string_view(str));
}

/// Compute size for std::vector (runtime: varint count + sum of element sizes)
template<typename T>
inline std::size_t compute_serialized_size(const std::vector<T>& vec) noexcept {
    std::size_t size = varint_size(vec.size());
    
    if constexpr (Arithmetic<T>) {
        // Optimized: all elements same size
        size += vec.size() * sizeof(T);
    } else {
        // General case: compute each element
        for (const auto& elem : vec) {
            size += compute_serialized_size(elem);
        }
    }
    
    return size;
}

/// Compute size for fixed_vector (runtime: varint count + sum of element sizes)
template<typename T, std::size_t N>
inline std::size_t compute_serialized_size(const fixed_vector<T, N>& vec) noexcept {
    std::size_t size = varint_size(vec.size());
    
    if constexpr (Arithmetic<T>) {
        // Optimized: all elements same size
        size += vec.size() * sizeof(T);
    } else {
        // General case: compute each element
        for (const auto& elem : vec) {
            size += compute_serialized_size(elem);
        }
    }
    
    return size;
}

// ============================================================================
// Compile-Time Maximum Size Bounds
// ============================================================================
// For fixed-capacity containers, we can compute worst-case maximum size

/// Maximum serialized size for fixed_string
template<std::size_t N>
constexpr std::size_t max_serialized_size(const fixed_string<N>&) noexcept {
    // Worst case: full capacity + varint for size N (typically 1-2 bytes)
    return varint_size(N) + N;
}

/// Maximum serialized size for fixed_vector
template<typename T, std::size_t N>
constexpr std::size_t max_serialized_size(const fixed_vector<T, N>&) noexcept {
    if constexpr (Arithmetic<T>) {
        // Arithmetic: exact size per element
        return varint_size(N) + N * sizeof(T);
    } else {
        // Non-arithmetic: would need max_serialized_size recursively
        // For now, return 0 to indicate "not computable at compile-time"
        return 0;
    }
}

/// Maximum serialized size for arithmetic
template<Arithmetic T>
constexpr std::size_t max_serialized_size(const T&) noexcept {
    return sizeof(T);
}

/// Maximum serialized size for bool
constexpr std::size_t max_serialized_size(bool) noexcept {
    return 1;
}

// ============================================================================
// Variadic Size Computation
// ============================================================================

/// Compute total size for multiple values (runtime)
template<typename... Args>
inline std::size_t compute_total_size(const Args&... args) noexcept {
    return (compute_serialized_size(args) + ...);
}

/// Compute maximum total size for multiple values (compile-time when possible)
template<typename... Args>
constexpr std::size_t compute_max_total_size(const Args&... args) noexcept {
    return (max_serialized_size(args) + ...);
}

// ============================================================================
// Size Category Helpers
// ============================================================================

/// Check if type has fully static size (compile-time computable)
template<typename T>
concept HasStaticSize = TypeTraits<T>::category == SizeCategory::Static;

/// Check if type has dynamic size (requires runtime computation)
template<typename T>
concept HasDynamicSize = TypeTraits<T>::category == SizeCategory::Dynamic;

/// Check if type has trailing dynamic size (partial compile-time + runtime)
template<typename T>
concept HasTrailingSize = TypeTraits<T>::category == SizeCategory::Trailing;

// ============================================================================
// Smart Buffer Allocation Helper
// ============================================================================

/// Allocate buffer with computed size (runtime or compile-time bound)
template<typename... Args>
inline std::vector<std::byte> allocate_serialization_buffer(const Args&... args) {
    // Try compile-time maximum first
    constexpr std::size_t max_size = compute_max_total_size(args...);
    
    if constexpr (max_size > 0) {
        // Use compile-time bound
        std::vector<std::byte> buffer;
        buffer.reserve(max_size);
        return buffer;
    } else {
        // Fall back to runtime computation
        std::size_t size = compute_total_size(args...);
        std::vector<std::byte> buffer;
        buffer.reserve(size);
        return buffer;
    }
}

// ============================================================================
// Example Usage Patterns
// ============================================================================

/*
// Example 1: Fixed-capacity containers (compile-time bound)
fixed_string<32> name = "Alice";
fixed_vector<int, 100> scores;
scores.push_back(95);

// Compile-time: reserves max possible size
constexpr std::size_t max_size = compute_max_total_size(name, scores);
// max_size = varint_size(32) + 32 + varint_size(100) + 100*4 = ~435 bytes

// Example 2: Dynamic types (runtime computation)
std::string username = "Bob";
std::vector<double> values = {1.1, 2.2, 3.3};

// Runtime: computes exact size needed
std::size_t exact_size = compute_total_size(username, values);
// exact_size = varint(3) + 3 + varint(3) + 3*8 = ~28 bytes

// Example 3: Smart allocation
auto buffer = allocate_serialization_buffer(name, scores);
// Uses compile-time max_size automatically

auto buffer2 = allocate_serialization_buffer(username, values);
// Falls back to runtime computation automatically
*/

} // namespace sertial
