#pragma once

#include "../containers/container_registration.hpp"
#include "../core/traits/padding.hpp"
#include <type_traits>

namespace sertial::detail {

// ============================================================================
// Fixed Container Detection (Concept-Based)
// ============================================================================
// Container detection now uses SerializableContainer concept from container_registration.hpp
// For traits, use: container_max_size_v<T>, container_element_size_v<T>

// ============================================================================
// Container Type Detection
// ============================================================================

/// @brief Get container type name for schema metadata
template<typename T>
struct container_type_name {
    static constexpr const char* value = "";
};

template<typename T, std::size_t N>
struct container_type_name<sertial::fixed_vector<T, N>> {
    static constexpr const char* value = "fixed_vector";
};

template<std::size_t N>
struct container_type_name<sertial::fixed_string<N>> {
    static constexpr const char* value = "fixed_string";
};

template<typename T, std::size_t N>
struct container_type_name<sertial::RingBuffer<T, N>> {
    static constexpr const char* value = "ring_buffer";
};

template<typename T>
inline constexpr const char* container_type_name_v = container_type_name<T>::value;

/// @brief Check if any field in a NamedTuple is a fixed container
template<typename... Fields>
constexpr bool has_fixed_containers(rfl::NamedTuple<Fields...>*) {
    using namespace detail;
    return (SerializableContainer<field_type_t<Fields>> || ...);
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
