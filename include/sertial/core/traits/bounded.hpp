#pragma once

#include "padding.hpp"
#include "../concepts.hpp"
#include "../../containers/container_traits.hpp"
#include "../../containers/fixed_string.hpp"
#include "../../containers/fixed_vector.hpp"
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <array>
#include <rfl.hpp>

namespace sertial {

// ============================================================================
// Bounded Serialization Size Computation
// ============================================================================
// Compute the MAXIMUM serialized size at compile-time.
// This allows stack-allocated buffers with guaranteed no runtime allocation.
//
// Requirements for bounded types:
// - Arithmetic types: sizeof(T)
// - fixed_string<N>: sizeof(size_type) + N
// - fixed_vector<T, N>: sizeof(size_type) + N * max_serialized_size<T>
// - std::array<T, N>: N * max_serialized_size<T>
// - Structs: sum of max_serialized_size for all fields (via rfl::named_tuple_t)
//
// FORBIDDEN (unbounded):
// - std::string
// - std::vector
// - Any struct containing unbounded types

// Size type for length prefixes in variable-length containers
// Defined here to avoid circular includes
using bounded_size_type = uint32_t;

namespace detail {

// Forward declaration
template<typename T>
struct max_size_calculator;

// ============================================================================
// Unbounded Type Detection
// ============================================================================

/// @brief Check if a type is unbounded (no compile-time max size)
template<typename T, typename = void>
struct is_unbounded_type : std::false_type {};

// std::string is unbounded
template<typename CharT, typename Traits, typename Alloc>
struct is_unbounded_type<std::basic_string<CharT, Traits, Alloc>> : std::true_type {};

// std::vector is unbounded
template<typename T, typename Alloc>
struct is_unbounded_type<std::vector<T, Alloc>> : std::true_type {};

template<typename T>
inline constexpr bool is_unbounded_type_v = is_unbounded_type<T>::value;

// ============================================================================
// Fixed String Detection (specific check)
// ============================================================================

template<typename T>
struct is_fixed_string : std::false_type {};

template<std::size_t N>
struct is_fixed_string<fixed_string<N>> : std::true_type {};

template<typename T>
inline constexpr bool is_fixed_string_v = is_fixed_string<T>::value;

// ============================================================================
// Fixed Vector Detection (specific check)
// ============================================================================

template<typename T>
struct is_fixed_vector : std::false_type {};

template<typename E, std::size_t N>
struct is_fixed_vector<fixed_vector<E, N>> : std::true_type {};

template<typename T>
inline constexpr bool is_fixed_vector_v = is_fixed_vector<T>::value;

// ============================================================================
// Bounded Type Detection (Recursive)
// ============================================================================

// Forward declaration for recursive check
template<typename T>
struct is_bounded_impl;

// Check if all fields in a NamedTuple are bounded
template<typename... Fields>
constexpr bool all_fields_bounded(rfl::NamedTuple<Fields...>*) {
    return (is_bounded_impl<field_type_t<Fields>>::value && ...);
}

template<typename T>
struct is_bounded_impl {
    static constexpr bool value = []() {
        // Unbounded types fail
        if constexpr (is_unbounded_type_v<T>) {
            return false;
        }
        // Arithmetic types are bounded
        else if constexpr (std::is_arithmetic_v<T>) {
            return true;
        }
        // fixed_string is bounded
        else if constexpr (is_fixed_string_v<T>) {
            return true;
        }
        // fixed_vector is bounded if element type is bounded
        else if constexpr (is_fixed_vector_v<T>) {
            using element_t = typename T::value_type;
            return is_bounded_impl<element_t>::value;
        }
        // std::array is bounded if element type is bounded
        else if constexpr (is_std_array_v<T>) {
            using element_t = typename T::value_type;
            return is_bounded_impl<element_t>::value;
        }
        // Structs are bounded if all fields are bounded
        else if constexpr (has_named_tuple_t_v<T>) {
            using NT = rfl::named_tuple_t<T>;
            return all_fields_bounded(static_cast<NT*>(nullptr));
        }
        // Unknown types are not bounded
        else {
            return false;
        }
    }();
};

template<typename T>
inline constexpr bool is_bounded_v = is_bounded_impl<T>::value;

// ============================================================================
// Maximum Serialized Size Computation (Compile-Time)
// ============================================================================

// Compute max size for fields in a NamedTuple
template<typename... Fields>
constexpr std::size_t sum_max_field_sizes(rfl::NamedTuple<Fields...>*) {
    return (max_size_calculator<field_type_t<Fields>>::value + ...);
}

// Count fields in a NamedTuple
template<typename... Fields>
constexpr std::size_t count_named_tuple_fields(rfl::NamedTuple<Fields...>*) {
    return sizeof...(Fields);
}

template<typename T>
struct max_size_calculator {
    static constexpr std::size_t value = []() {
        // Arithmetic types
        if constexpr (std::is_arithmetic_v<T>) {
            return sizeof(T);
        }
        // fixed_string<N>: length prefix + max N chars
        else if constexpr (is_fixed_string_v<T>) {
            constexpr std::size_t N = T::max_size_v;
            return sizeof(bounded_size_type) + N;
        }
        // fixed_vector<E, N>: length prefix + N * max_size(E)
        else if constexpr (is_fixed_vector_v<T>) {
            using element_t = typename T::value_type;
            constexpr std::size_t N = T::max_size_v;
            return sizeof(bounded_size_type) + N * max_size_calculator<element_t>::value;
        }
        // std::array<E, N>: N * max_size(E) (no length prefix)
        else if constexpr (is_std_array_v<T>) {
            using element_t = typename T::value_type;
            constexpr std::size_t N = std::tuple_size_v<T>;
            return N * max_size_calculator<element_t>::value;
        }
        // Structs: just sum of max sizes of all fields (no header needed - type is known)
        else if constexpr (has_named_tuple_t_v<T>) {
            using NT = rfl::named_tuple_t<T>;
            // No length prefix needed - we know the type at compile time
            return sum_max_field_sizes(static_cast<NT*>(nullptr));
        }
        // Unbounded or unknown: return 0 (will fail is_bounded check anyway)
        else {
            return std::size_t{0};
        }
    }();
};

} // namespace detail

// ============================================================================
// Public API
// ============================================================================

/// @brief Concept: Type has bounded serialized size (compile-time computable)
template<typename T>
concept BoundedSerializable = detail::is_bounded_v<T>;

/// @brief Check if type is bounded (compile-time)
template<typename T>
inline constexpr bool is_bounded_v = detail::is_bounded_v<T>;

/// @brief Check if type is unbounded (uses heap containers)
template<typename T>
inline constexpr bool is_unbounded_v = detail::is_unbounded_type_v<T>;

/// @brief Maximum serialized size for a bounded type
/// @note Only valid if is_bounded_v<T> is true
template<typename T>
inline constexpr std::size_t max_serialized_size_v = detail::max_size_calculator<T>::value;

// ============================================================================
// Static Assertions for Debugging
// ============================================================================

/// @brief Helper to generate clear compile error for unbounded types
template<typename T>
struct BoundedTypeChecker {
    static_assert(is_bounded_v<T>,
        "Type is not bounded-serializable. "
        "Use fixed_string<N> instead of std::string, "
        "and fixed_vector<T, N> instead of std::vector<T>.");
    
    static constexpr bool valid = is_bounded_v<T>;
};

} // namespace sertial
