#pragma once

#include "size_category.hpp"
#include "padding.hpp"
#include "../concepts.hpp"
#include "../../containers/container_traits.hpp"
#include <cstddef>
#include <type_traits>
#include <string>

namespace sertial {

// ============================================================================
// TypeTraits - Comprehensive Compile-Time Type Analysis
// ============================================================================

/// @brief Compile-time analysis of types for binary serialization
/// 
/// Provides:
/// - Size classification (Static/Dynamic/Trailing)
/// - Padding analysis (has_padding, packed_size, unpacked_size)
/// - Optimization flags (can_memcpy_whole, is_trivially_copyable)
/// - Custom reflector detection
template<typename T>
struct TypeTraits {
private:
    // ========================================================================
    // Size Category Computation
    // ========================================================================
    
    static constexpr SizeCategory compute_category() {
        if constexpr (std::is_arithmetic_v<T>) {
            return SizeCategory::Static;
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return SizeCategory::Dynamic;
        }
        else if constexpr (is_fixed_capacity_v<T>) {
            // Fixed-capacity containers are dynamic in serialized size
            // (only actual elements written) but bounded
            return SizeCategory::Dynamic;
        }
        else if constexpr (is_std_vector_v<T>) {
            return SizeCategory::Dynamic;
        }
        else if constexpr (is_std_array_v<T>) {
            // std::array is static if element type is static
            using element_t = typename T::value_type;
            return TypeTraits<element_t>::category;
        }
        else if constexpr (std::is_class_v<T>) {
            // User-defined structs - analyze fields
            // For now, treat as Dynamic (would need field recursion for Static)
            return SizeCategory::Dynamic;
        }
        else {
            return SizeCategory::Static;
        }
    }
    
    // ========================================================================
    // Packed Size Computation
    // ========================================================================
    
    static constexpr std::size_t compute_packed_size() {
        if constexpr (std::is_arithmetic_v<T>) {
            return sizeof(T);
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return 0; // Dynamic - must compute at runtime
        }
        else if constexpr (is_fixed_capacity_v<T>) {
            return 0; // Dynamic
        }
        else if constexpr (is_std_vector_v<T>) {
            return 0; // Dynamic
        }
        else if constexpr (std::is_class_v<T>) {
            return detail::compute_packed_struct_size<T>();
        }
        else {
            return sizeof(T);
        }
    }

public:
    // ========================================================================
    // Size Classification
    // ========================================================================
    
    /// @brief Size category of the type
    static constexpr SizeCategory category = compute_category();
    
    /// @brief Size for static-size types (0 for dynamic types)
    static constexpr std::size_t static_size = 
        (category == SizeCategory::Static) ? sizeof(T) : 0;
    
    /// @brief Size of static prefix for trailing types (0 for others)
    static constexpr std::size_t static_prefix_size = 0; // TODO: Implement
    
    // ========================================================================
    // Padding Analysis
    // ========================================================================
    
    /// @brief Does the type have padding bytes?
    static constexpr bool has_padding = detail::compute_has_padding<T>();
    
    /// @brief Size without padding
    static constexpr std::size_t packed_size = compute_packed_size();
    
    /// @brief Size with padding (actual memory size)
    static constexpr std::size_t unpacked_size = sizeof(T);
    
    // ========================================================================
    // Optimization Flags
    // ========================================================================
    
    /// @brief Is the type trivially copyable?
    static constexpr bool is_trivially_copyable = std::is_trivially_copyable_v<T>;
    
    /// @brief Can we safely memcpy the entire type in one operation?
    /// Only true if: no padding, trivially copyable, and static size
    static constexpr bool can_memcpy_whole = 
        !has_padding && 
        is_trivially_copyable && 
        (category == SizeCategory::Static);
    
    /// @brief Does the type have a custom binary reflector?
    static constexpr bool has_custom_reflector = HasCustomReflector<T>;
};

// ============================================================================
// Convenience Type Aliases
// ============================================================================

template<typename T>
inline constexpr SizeCategory size_category_v = TypeTraits<T>::category;

template<typename T>
inline constexpr bool can_memcpy_whole_v = TypeTraits<T>::can_memcpy_whole;

} // namespace sertial
