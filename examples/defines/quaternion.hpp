#pragma once
/// @file quaternion.hpp
/// @brief Quaternion field type for rotations (templated)

#include <rfl.hpp>

namespace examples::defines {

/// @brief Quaternion for rotation (w, x, y, z format)
template<typename Scalar = float>
struct Quaternion {
    Scalar w = Scalar{1};
    Scalar x = Scalar{0};
    Scalar y = Scalar{0};
    Scalar z = Scalar{0};
};

// Common type aliases
using Quaternionf = Quaternion<float>;
using Quaterniond = Quaternion<double>;

} // namespace examples::defines
