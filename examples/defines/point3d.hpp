#pragma once
/// @file point3d.hpp
/// @brief 3D point field type (templated)

#include <rfl.hpp>

namespace examples::defines {

/// @brief 3D point with configurable scalar type
template<typename Scalar = float>
struct Point3D {
    Scalar x = Scalar{0};
    Scalar y = Scalar{0};
    Scalar z = Scalar{0};
};

// Common type aliases
using Point3Df = Point3D<float>;
using Point3Dd = Point3D<double>;

} // namespace examples::defines
