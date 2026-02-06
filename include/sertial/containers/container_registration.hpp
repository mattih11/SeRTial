#pragma once

#include <cstddef>
#include <concepts>
#include <type_traits>

namespace sertial {

// Forward declarations - actual definitions must be included before using the concept
template<typename T, std::size_t N> class fixed_vector;
template<std::size_t N> class fixed_string;
template<typename T, std::size_t N> class RingBuffer;

// ============================================================================
// Core Concept: Serializable Fixed-Capacity Container
// ============================================================================

/**
 * @brief Concept defining requirements for a serializable fixed-capacity container
 * 
 * A container satisfies SerializableContainer if it:
 * 1. Has a value_type (element type)
 * 2. Exposes compile-time max_size
 * 3. Provides runtime size() query
 * 4. Provides contiguous data access via data()
 * 
 * @tparam T Container type to check
 * 
 * @note This concept enables automatic trait derivation - no manual specializations needed
 * @note Prevents nested containers (data() must return value_type*, not another container)
 */
template<typename T>
concept SerializableContainer = requires(const T& c, T& mut_c) {
    // Required nested type
    typename T::value_type;
    
    // Compile-time maximum capacity (containers use max_size_v)
    { T::max_size_v } -> std::convertible_to<std::size_t>;
    
    // Runtime size query
    { c.size() } -> std::same_as<std::size_t>;
    
    // Contiguous data access (const)
    { c.data() } -> std::convertible_to<const typename T::value_type*>;
    
    // Mutable data access (for deserialization)
    { mut_c.data_unsafe() } -> std::convertible_to<typename T::value_type*>;
    
    // Unsafe size setter (for deserialization)
    { mut_c.set_size_unsafe(std::size_t{}) } -> std::same_as<void>;
} && 
// Prevent nested containers: value_type must NOT be a container itself
!requires { typename T::value_type::max_size_v; };

// ============================================================================
// Automatic Trait Extraction
// ============================================================================

/**
 * @brief Extracts container metadata automatically from types satisfying SerializableContainer
 * 
 * Provides all information needed for serialization without manual specialization:
 * - element_type: Type of elements stored
 * - max_size: Compile-time maximum capacity
 * - element_size: Size of each element in bytes
 * - is_variable_length: Always true for containers
 * 
 * @tparam T Container type (must satisfy SerializableContainer)
 */
template<SerializableContainer T>
struct container_metadata {
    using element_type = typename T::value_type;
    
    static constexpr std::size_t max_size = T::max_size_v;
    static constexpr std::size_t element_size = sizeof(element_type);
    
    static constexpr bool is_variable_length = true;
    static constexpr bool is_fixed_capacity = true;
    static constexpr bool is_serializable = true;
};

// ============================================================================
// Convenience Traits (Backward Compatibility)
// ============================================================================

/**
 * @brief Check if type is a serializable fixed-capacity container
 * @tparam T Type to check
 */
template<typename T>
inline constexpr bool is_serializable_container_v = SerializableContainer<T>;

/**
 * @brief Get element type of container (only valid if is_serializable_container_v<T>)
 * @tparam T Container type
 */
template<SerializableContainer T>
using container_element_t = typename container_metadata<T>::element_type;

/**
 * @brief Get compile-time maximum capacity of container
 * @tparam T Container type
 */
template<SerializableContainer T>
inline constexpr std::size_t container_max_size_v = container_metadata<T>::max_size;

/**
 * @brief Get element size in bytes
 * @tparam T Container type
 */
template<SerializableContainer T>
inline constexpr std::size_t container_element_size_v = container_metadata<T>::element_size;

// ============================================================================
// Specialization Registration Helpers
// ============================================================================

/**
 * @brief Helper to verify a container satisfies the concept at compile time
 * @tparam T Container type to check
 * 
 * Usage:
 * @code
 * static_assert(verify_container<fixed_vector<int, 10>>());
 * @endcode
 */
template<typename T>
consteval bool verify_container() {
    return SerializableContainer<T>;
}

/**
 * @brief Helper to check if element type is serializable (non-nested)
 * @tparam T Element type to check
 * 
 * Returns false if T is itself a container (prevents nesting)
 */
template<typename T>
consteval bool is_serializable_element() {
    return !requires { typename T::max_size; };  // Not a container
}

// ============================================================================
// Static Assertions for Common Containers
// ============================================================================
// NOTE: These assertions are moved to container_traits.hpp where containers are fully defined

} // namespace sertial
