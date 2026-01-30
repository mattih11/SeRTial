#pragma once
/// @file pose3d.hpp
/// @brief 3D pose (position + orientation) field type (templated)

#include "point3d.hpp"
#include "quaternion.hpp"

namespace examples::defines {

/// @brief 3D pose: position + orientation
/// @tparam TPoint Point type (e.g., Point3D<float>)
/// @tparam TQuat Quaternion type (e.g., Quaternion<float>)
template<typename TPoint = Point3D<float>, typename TQuat = Quaternion<float>>
struct Pose3D {
    TPoint position;
    TQuat orientation;
};

// Common type aliases
using Pose3Df = Pose3D<Point3Df, Quaternionf>;
using Pose3Dd = Pose3D<Point3Dd, Quaterniond>;

} // namespace examples::defines
