#pragma once

#include <cstddef>
#include <type_traits>
#include <array>
#include <rfl.hpp>

// Forward declarations for container types (to avoid circular includes)
namespace sertial {
    template<std::size_t N> class fixed_string;
    template<typename T, std::size_t N> class fixed_vector;
}

namespace sertial {

// ============================================================================
// Padding Analysis
// ============================================================================
// Uses rfl::named_tuple_t to compute the packed size of a struct (sum of field
// sizes without alignment padding), then compares to sizeof(T) to detect padding.

namespace detail {

// ============================================================================
// Container Type Detection (early, for padding.hpp only)
// ============================================================================

template<typename T>
struct is_container_type : std::false_type {};

template<std::size_t N>
struct is_container_type<fixed_string<N>> : std::true_type {};

template<typename T, std::size_t N>
struct is_container_type<fixed_vector<T, N>> : std::true_type {};

template<typename T, std::size_t N>
struct is_container_type<std::array<T, N>> : std::true_type {};

template<typename T>
inline constexpr bool is_container_type_v = is_container_type<T>::value;

// ============================================================================
// Field Type Extraction from rfl::Field
// ============================================================================

/// @brief Extract the value type from an rfl::Field<Name, Type>
template<typename Field>
struct field_type;

template<rfl::internal::StringLiteral Name, typename Type>
struct field_type<rfl::Field<Name, Type>> {
    using type = Type;
};

template<typename Field>
using field_type_t = typename field_type<Field>::type;

// ============================================================================
// Named Tuple Field Size Summation
// ============================================================================

/// @brief Sum the sizes of all fields in a NamedTuple type
template<typename NamedTuple>
struct sum_named_tuple_fields;

// Empty tuple case - 0 fields
template<>
struct sum_named_tuple_fields<rfl::NamedTuple<>> {
    static constexpr std::size_t value = 0;
};

// Non-empty tuple case
template<typename First, typename... Rest>
struct sum_named_tuple_fields<rfl::NamedTuple<First, Rest...>> {
    static constexpr std::size_t value = 
        sizeof(field_type_t<First>) + sum_named_tuple_fields<rfl::NamedTuple<Rest...>>::value;
};

template<typename NamedTuple>
inline constexpr std::size_t sum_named_tuple_fields_v = sum_named_tuple_fields<NamedTuple>::value;

// ============================================================================
// Named Tuple Detection - Simple class check only
// ============================================================================

/// @brief Check if a type T could potentially be a reflectable struct
/// This just checks if it's a class, non-container, non-string.
/// The actual named_tuple_t usage is guarded by if constexpr in calling code.
template<typename T>
inline constexpr bool is_reflectable_struct_v = 
    std::is_class_v<T> && 
    !is_container_type_v<T> &&
    !std::is_same_v<T, std::string>;

/// @brief Backward-compatible alias - checks if type could have named_tuple_t
template<typename T>
inline constexpr bool has_named_tuple_t_v = is_reflectable_struct_v<T>;

// ============================================================================
// Packed Size Computation
// ============================================================================

/// @brief Compute the packed size of a struct using reflection
/// @return Sum of all field sizes (no alignment padding)
template<typename T>
constexpr std::size_t compute_packed_struct_size() {
    if constexpr (has_named_tuple_t_v<T>) {
        using NT = rfl::named_tuple_t<T>;
        return sum_named_tuple_fields_v<NT>;
    } else {
        // Not reflectable - fallback to sizeof (assume no padding)
        return sizeof(T);
    }
}

// ============================================================================
// Padding Detection
// ============================================================================

/// @brief Check if a type has internal padding
/// @return true if sizeof(T) > packed_size (sum of field sizes)
template<typename T>
constexpr bool compute_has_padding() {
    if constexpr (std::is_arithmetic_v<T>) {
        return false;
    }
    else if constexpr (!std::is_class_v<T>) {
        return false;
    }
    else if constexpr (has_named_tuple_t_v<T>) {
        return sizeof(T) != compute_packed_struct_size<T>();
    }
    else {
        // Not reflectable - can't determine, assume no padding
        return false;
    }
}

} // namespace detail

// ============================================================================
// Public API
// ============================================================================

/// @brief Check if a type has padding (compile-time)
template<typename T>
inline constexpr bool has_padding_v = detail::compute_has_padding<T>();

/// @brief Get the packed size of a type (without padding)
template<typename T>
inline constexpr std::size_t packed_size_v = 
    std::is_class_v<T> ? detail::compute_packed_struct_size<T>() : sizeof(T);

/// @brief Get the unpacked size of a type (actual memory size with padding)
template<typename T>
inline constexpr std::size_t unpacked_size_v = sizeof(T);

/// @brief Calculate padding bytes in a type
template<typename T>
inline constexpr std::size_t padding_bytes_v = sizeof(T) - packed_size_v<T>;

} // namespace sertial
