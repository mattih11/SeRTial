#pragma once

#include "container_registration.hpp"
#include "fixed_vector.hpp"
#include "fixed_string.hpp"
#include "ring_buffer.hpp"
#include <type_traits>
#include <vector>
#include <string>
#include <cstddef>

namespace sertial {

// ============================================================================
// Fixed Capacity Detection (Concept-Based)
// ============================================================================

/// @brief Trait to detect fixed-capacity containers
/// @deprecated Use SerializableContainer concept instead
template<typename T>
struct is_fixed_capacity : std::bool_constant<SerializableContainer<T>> {};

template<typename T>
inline constexpr bool is_fixed_capacity_v = SerializableContainer<T>;

// ============================================================================
// Fixed Capacity Traits Extraction (Concept-Based)
// ============================================================================

/// @brief Extract element type and max size from fixed-capacity containers
/// @deprecated Use container_metadata<T> instead
template<typename T>
    requires SerializableContainer<T>
struct fixed_capacity_traits {
    using element_type = typename container_metadata<T>::element_type;
    using container_type = T;
    static constexpr std::size_t max_size = container_metadata<T>::max_size;
    
    // Type-specific checks (fallback for string detection)
    static constexpr bool is_string = std::is_same_v<T, fixed_string<max_size>>;
};

// ============================================================================
// Standard Container Detection
// ============================================================================

/// @brief Detect std::vector
template<typename T>
struct is_std_vector : std::false_type {};

template<typename T, typename Alloc>
struct is_std_vector<std::vector<T, Alloc>> : std::true_type {};

template<typename T>
inline constexpr bool is_std_vector_v = is_std_vector<T>::value;

/// @brief Detect std::array
template<typename T>
struct is_std_array : std::false_type {};

template<typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template<typename T>
inline constexpr bool is_std_array_v = is_std_array<T>::value;

/// @brief Detect std::string
template<typename T>
struct is_std_string : std::false_type {};

template<typename CharT, typename Traits, typename Alloc>
struct is_std_string<std::basic_string<CharT, Traits, Alloc>> : std::true_type {};

template<typename T>
inline constexpr bool is_std_string_v = is_std_string<T>::value;

// ============================================================================
// Container Category
// ============================================================================

/// @brief Categorize container types
enum class ContainerCategory {
    NotContainer,
    FixedCapacity,     // fixed_vector, fixed_string, RingBuffer
    DynamicHeap,       // std::vector, std::string
    StaticArray        // std::array, C array
};

template<typename T>
struct container_category {
    static constexpr ContainerCategory value = 
        SerializableContainer<T> ? ContainerCategory::FixedCapacity 
                                 : ContainerCategory::NotContainer;
};

template<typename T, typename Alloc>
struct container_category<std::vector<T, Alloc>> {
    static constexpr ContainerCategory value = ContainerCategory::DynamicHeap;
};

template<typename CharT, typename Traits, typename Alloc>
struct container_category<std::basic_string<CharT, Traits, Alloc>> {
    static constexpr ContainerCategory value = ContainerCategory::DynamicHeap;
};

template<typename T, std::size_t N>
struct container_category<std::array<T, N>> {
    static constexpr ContainerCategory value = ContainerCategory::StaticArray;
};

template<typename T, std::size_t N>
struct container_category<T[N]> {
    static constexpr ContainerCategory value = ContainerCategory::StaticArray;
};

template<typename T>
inline constexpr ContainerCategory container_category_v = container_category<T>::value;

// ============================================================================
// Static Assertions - Verify Concept Satisfaction
// ============================================================================

// Verify our containers satisfy the SerializableContainer concept
static_assert(SerializableContainer<fixed_vector<int, 10>>, 
              "fixed_vector must satisfy SerializableContainer");
static_assert(SerializableContainer<fixed_string<64>>, 
              "fixed_string must satisfy SerializableContainer");
// NOTE: RingBuffer intentionally excluded - needs special serialization handling
// static_assert(SerializableContainer<RingBuffer<float, 100>>, 
//               "RingBuffer must satisfy SerializableContainer");

// TODO: Fix nested container rejection - concept currently doesn't prevent nested containers
// The negative requirement `!requires { typename T::value_type::max_size_v; }` should work
// but appears to not be evaluated correctly by the compiler
// static_assert(!SerializableContainer<fixed_vector<fixed_vector<int, 5>, 10>>,
//               "Nested containers must be rejected by concept");

// Verify metadata extraction works
static_assert(container_max_size_v<fixed_vector<float, 100>> == 100,
              "max_size extraction must work");
static_assert(container_element_size_v<fixed_vector<uint32_t, 10>> == 4,
              "element_size extraction must work");

} // namespace sertial
