#pragma once

#include "../containers/container_registration.hpp"
#include "../core/traits/padding.hpp"
#include <type_traits>

namespace sertial::detail {

// ============================================================================
// Fixed Container Detection (Concept-Based)
// ============================================================================
// This file now provides backward-compatible aliases to the new concept-based
// registration system in container_registration.hpp

/// @brief Check if type is a fixed container with runtime size
/// @deprecated Use SerializableContainer concept instead
template<typename T>
struct is_fixed_container_impl : std::bool_constant<SerializableContainer<T>> {};

/// @brief Boolean constant for fixed container detection
template<typename T>
inline constexpr bool is_fixed_container_v = SerializableContainer<T>;

/// @brief Get capacity of fixed container at compile-time
/// @deprecated Use container_max_size_v<T> instead
template<typename T>
struct fixed_container_capacity {
    static constexpr std::size_t value = 0;
};

template<SerializableContainer T>
struct fixed_container_capacity<T> {
    static constexpr std::size_t value = container_max_size_v<T>;
};

template<typename T>
inline constexpr std::size_t fixed_container_capacity_v = fixed_container_capacity<T>::value;

/// @brief Get element size of fixed container at compile-time
/// @deprecated Use container_element_size_v<T> instead
template<typename T>
struct fixed_container_element_size {
    static constexpr std::size_t value = 0;
};

template<SerializableContainer T>
struct fixed_container_element_size<T> {
    static constexpr std::size_t value = container_element_size_v<T>;
};

template<typename T>
inline constexpr std::size_t fixed_container_element_size_v = fixed_container_element_size<T>::value;

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
