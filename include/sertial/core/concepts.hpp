#pragma once

#include <concepts>
#include <type_traits>

namespace sertial {

// Forward declarations
template<typename T>
struct BinaryReflector;

template<typename T>
struct TypeTraits;

enum class SizeCategory;

// ============================================================================
// Basic Type Concepts
// ============================================================================

template<typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template<typename T>
concept Integral = std::is_integral_v<T>;

template<typename T>
concept FloatingPoint = std::is_floating_point_v<T>;

template<typename T>
concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

template<typename T>
concept StandardLayout = std::is_standard_layout_v<T>;

// ============================================================================
// Custom Reflector Concept
// ============================================================================

template<typename T>
concept HasCustomReflector = requires {
    { BinaryReflector<T>::has_custom_binary } -> std::convertible_to<bool>;
    requires BinaryReflector<T>::has_custom_binary == true;
};

// ============================================================================
// Container Concepts
// ============================================================================

template<typename T>
concept FixedCapacityContainer = requires(T t) {
    { T::max_size_v } -> std::convertible_to<std::size_t>;
    { t.size() } -> std::convertible_to<std::size_t>;
    { t.data() };
};

template<typename T>
concept DynamicContainer = requires(T t) {
    typename T::value_type;
    { t.size() } -> std::convertible_to<std::size_t>;
    { t.begin() };
    { t.end() };
};

template<typename T>
concept ContiguousContainer = DynamicContainer<T> && requires(T t) {
    { t.data() };
};

// ============================================================================
// Size Category Concepts (forward declarations need TypeTraits)
// ============================================================================

// Note: These will be fully defined after TypeTraits is available
template<typename T>
concept StaticSizeType = std::is_arithmetic_v<T>; // Simplified for now

template<typename T>
concept DynamicSizeType = !StaticSizeType<T>;

// ============================================================================
// Serialization Concepts
// ============================================================================

template<typename T>
concept MemcpySafe = TriviallyCopyable<T> && StandardLayout<T>;

template<typename T>
concept Serializable = requires {
    // Either has custom reflector or is a struct/class we can reflect
    requires HasCustomReflector<T> || std::is_class_v<T> || std::is_arithmetic_v<T>;
};

} // namespace sertial
