#pragma once

#include <cstddef>

namespace sertial {

// ============================================================================
// Size Category
// ============================================================================

/// @brief Categorization of type sizes for serialization optimization
/// 
/// Types are classified by how their serialized size can be determined:
/// - Static: Size known at compile-time (primitives, fixed-size structs)
/// - Dynamic: Size depends on runtime data (strings, vectors, varlen structs)
/// - Trailing: Static prefix + dynamic suffix (optimization opportunity)
enum class SizeCategory {
    Static,    ///< Fixed size known at compile time (can memcpy if no padding)
    Dynamic,   ///< Variable size determined at runtime
    Trailing   ///< Static prefix with dynamic suffix (optimization opportunity)
};

// ============================================================================
// Size Category Helpers
// ============================================================================

/// @brief Check if a SizeCategory allows compile-time size computation
constexpr bool is_static_category(SizeCategory cat) noexcept {
    return cat == SizeCategory::Static;
}

/// @brief Check if a SizeCategory requires runtime size computation
constexpr bool is_dynamic_category(SizeCategory cat) noexcept {
    return cat == SizeCategory::Dynamic || cat == SizeCategory::Trailing;
}

} // namespace sertial
