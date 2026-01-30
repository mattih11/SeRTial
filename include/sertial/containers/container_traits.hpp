#pragma once

#include "fixed_vector.hpp"
#include "fixed_string.hpp"
#include <type_traits>
#include <vector>
#include <string>
#include <cstddef>

namespace sertial {

// ============================================================================
// Fixed Capacity Detection
// ============================================================================

/// @brief Trait to detect fixed-capacity containers
template<typename T>
struct is_fixed_capacity : std::false_type {};

template<typename T, std::size_t N>
struct is_fixed_capacity<fixed_vector<T, N>> : std::true_type {};

template<std::size_t N>
struct is_fixed_capacity<fixed_string<N>> : std::true_type {};

template<typename T>
inline constexpr bool is_fixed_capacity_v = is_fixed_capacity<T>::value;

// ============================================================================
// Fixed Capacity Traits Extraction
// ============================================================================

/// @brief Extract element type and max size from fixed-capacity containers
template<typename T>
struct fixed_capacity_traits;

template<typename T, std::size_t N>
struct fixed_capacity_traits<fixed_vector<T, N>> {
    using element_type = T;
    using container_type = fixed_vector<T, N>;
    static constexpr std::size_t max_size = N;
    static constexpr bool is_string = false;
};

template<std::size_t N>
struct fixed_capacity_traits<fixed_string<N>> {
    using element_type = char;
    using container_type = fixed_string<N>;
    static constexpr std::size_t max_size = N;
    static constexpr bool is_string = true;
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
    FixedCapacity,     // fixed_vector, fixed_string
    DynamicHeap,       // std::vector, std::string
    StaticArray        // std::array, C array
};

template<typename T>
struct container_category {
    static constexpr ContainerCategory value = ContainerCategory::NotContainer;
};

template<typename T, std::size_t N>
struct container_category<fixed_vector<T, N>> {
    static constexpr ContainerCategory value = ContainerCategory::FixedCapacity;
};

template<std::size_t N>
struct container_category<fixed_string<N>> {
    static constexpr ContainerCategory value = ContainerCategory::FixedCapacity;
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

} // namespace sertial
