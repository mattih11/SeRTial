#pragma once

#include "../containers/fixed_vector.hpp"
#include "../containers/fixed_string.hpp"
#include "../core/traits/padding.hpp"
#include <type_traits>

namespace sertial::detail {

// ============================================================================
// Fixed Container Detection (for compile-time checks)
// ============================================================================

/// @brief Check if type is a fixed container with runtime size
/// 
/// fixed_vector and fixed_string are trivially copyable but have runtime size().
/// Using optimized memcpy on them would waste space by copying full capacity
/// instead of just the used portion.
template<typename T>
struct is_fixed_container_impl : std::false_type {};

template<typename T, std::size_t N>
struct is_fixed_container_impl<fixed_vector<T, N>> : std::true_type {};

template<std::size_t N>
struct is_fixed_container_impl<fixed_string<N>> : std::true_type {};

/// @brief Boolean constant for fixed container detection
template<typename T>
inline constexpr bool is_fixed_container_v = is_fixed_container_impl<T>::value;

/// @brief Check if any field in a NamedTuple is a fixed container
template<typename... Fields>
constexpr bool has_fixed_containers(rfl::NamedTuple<Fields...>*) {
    return (is_fixed_container_v<field_type_t<Fields>> || ...);
}

/// @brief Check if a struct contains any fields that are fixed containers
/// 
/// @return true if T has any fixed_vector or fixed_string fields
template<typename T>
constexpr bool struct_has_fixed_containers() {
    if constexpr (has_named_tuple_t_v<T>) {
        using NT = rfl::named_tuple_t<T>;
        return has_fixed_containers(static_cast<NT*>(nullptr));
    }
    return false;
}

} // namespace sertial::detail
