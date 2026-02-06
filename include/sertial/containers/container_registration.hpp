#pragma once

#include <cstddef>
#include <concepts>
#include <type_traits>
#include <span>
#include <array>
#include <rfl.hpp>
#include <rfl/NamedTuple.hpp>

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
// Serialization View System (Single Source of Truth)
// ============================================================================

/**
 * @brief Compile-time serialization strategy for containers
 * 
 * Provides zero-allocation, type-safe memory views for serialization.
 * Most containers return a single span, but containers with non-contiguous
 * storage (e.g., RingBuffer wrap-around) can return multiple spans.
 * 
 * @tparam T Container type
 * 
 * @return Array of spans covering all serializable data
 *         - span[0]: Primary data region (always used)
 *         - span[1]: Secondary region (empty for contiguous containers)
 * 
 * Design rationale:
 * - Fixed-size array (no allocation)
 * - Compile-time dispatch via template specialization
 * - Empty spans have size()==0, safe to ignore in loops
 * - Eliminates special-case branching in serialization code
 */
template<SerializableContainer T>
struct serialization_view_provider {
    using element_type = typename T::value_type;
    
    /**
     * @brief Get serialization memory views for a container
     * 
     * Default: Single contiguous span from data() to data()+size()
     * 
     * @param container Container instance to serialize
     * @return Array of spans (span[1] empty for contiguous containers)
     */
    static constexpr auto get_spans(const T& container) {
        using SpanType = std::span<const element_type>;
        return std::array<SpanType, 2>{
            SpanType{container.data(), container.size()},
            SpanType{}  // Empty span
        };
    }
    
    /**
     * @brief Get number of non-empty spans (for compile-time optimization)
     * 
     * @return Number of spans to iterate (1 for contiguous, 2 for wrapped)
     */
    static constexpr std::size_t span_count = 1;
};

/**
 * @brief Specialization for RingBuffer (wrap-around handling)
 * 
 * When wrapped:
 *   span[0]: [tail_index, capacity) - data at end of buffer
 *   span[1]: [0, head_index) - data at start of buffer
 * 
 * When not wrapped:
 *   span[0]: [tail_index, head_index) - contiguous data
 *   span[1]: empty
 */
template<typename T, std::size_t N>
struct serialization_view_provider<RingBuffer<T, N>> {
    using element_type = T;
    using SpanType = std::span<const element_type>;
    
    static constexpr auto get_spans(const RingBuffer<T, N>& rb) {
        if (rb.is_wrapped()) {
            // Two spans: tail→end, start→head
            std::size_t tail = rb.tail_index();
            std::size_t head = rb.head_index();
            std::size_t capacity = N;
            
            return std::array<SpanType, 2>{
                SpanType{rb.data_unsafe() + tail, capacity - tail},  // Tail to end
                SpanType{rb.data_unsafe(), head}                      // Start to head
            };
        } else {
            // Single span: tail→head
            std::size_t tail = rb.tail_index();
            std::size_t count = rb.size();
            
            return std::array<SpanType, 2>{
                SpanType{rb.data_unsafe() + tail, count},
                SpanType{}  // Empty
            };
        }
    }
    
    static constexpr std::size_t span_count = 2;  // May use both spans
};

/**
 * @brief Convenience function to get serialization spans
 * 
 * @tparam T Container type (must satisfy SerializableContainer)
 * @param container Container instance
 * @return Array of spans covering serializable data
 */
template<SerializableContainer T>
constexpr auto get_serialization_spans(const T& container) {
    return serialization_view_provider<T>::get_spans(container);
}

// ============================================================================
// Container Type Names (for Schema Metadata)
// ============================================================================

/**
 * @brief Get container type name for schema metadata
 * 
 * Provides human-readable names for serialized containers in schema output.
 * Default returns empty string (non-container types).
 * 
 * @tparam T Type to get name for
 */
template<typename T>
struct container_type_name {
    static constexpr const char* value = "";
};

template<typename T, std::size_t N>
struct container_type_name<fixed_vector<T, N>> {
    static constexpr const char* value = "fixed_vector";
};

template<std::size_t N>
struct container_type_name<fixed_string<N>> {
    static constexpr const char* value = "fixed_string";
};

template<typename T, std::size_t N>
struct container_type_name<RingBuffer<T, N>> {
    static constexpr const char* value = "ring_buffer";
};

/**
 * @brief Convenience variable template for container type names
 */
template<typename T>
inline constexpr const char* container_type_name_v = container_type_name<T>::value;

// ============================================================================
// Struct Analysis Helpers  
// ============================================================================

namespace detail {

/// @brief Extract value type from rfl::Field<Name, Type>
template<typename Field>
struct extract_field_type;

template<rfl::internal::StringLiteral Name, typename Type>
struct extract_field_type<rfl::Field<Name, Type>> {
    using type = Type;
};

template<typename Field>
using field_value_t = typename extract_field_type<Field>::type;

} // namespace detail

/**
 * @brief Check if any field in a NamedTuple is a SerializableContainer
 * 
 * Uses fold expression with SerializableContainer concept for compile-time check.
 * 
 * @tparam Fields Field types from rfl::NamedTuple
 * @return true if any field satisfies SerializableContainer
 */
template<typename... Fields>
constexpr bool has_serializable_containers(rfl::NamedTuple<Fields...>*) {
    return (SerializableContainer<detail::field_value_t<Fields>> || ...);
}

/**
 * @brief Check if a struct contains any SerializableContainer fields
 * 
 * Analyzes struct via rfl::named_tuple_t reflection.
 * 
 * @tparam T Struct type to analyze
 * @return true if T has any fixed_vector/fixed_string/RingBuffer fields
 * 
 * @note Works with any type that has rfl::named_tuple_t defined
 */
template<typename T>
constexpr bool struct_has_serializable_containers() {
    if constexpr (requires { typename rfl::named_tuple_t<T>; }) {
        using NT = rfl::named_tuple_t<T>;
        return has_serializable_containers(static_cast<NT*>(nullptr));
    }
    return false;
}

// ============================================================================
// Static Assertions for Common Containers
// ============================================================================
// NOTE: These assertions are moved to container_traits.hpp where containers are fully defined

} // namespace sertial
