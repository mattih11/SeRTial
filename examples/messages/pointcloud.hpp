#pragma once

#include <rfl.hpp>
#include "header.hpp"
#include "../defines/point3d.hpp"
#include <sertial/containers/fixed_vector.hpp>

namespace examples::messages {

/// Single 3D point with intensity
template<typename T = float>
struct PointXYZI {
    T x = T{0};
    T y = T{0};
    T z = T{0};
    T intensity = T{0};
};

/// Small point cloud (up to 256 points) - real-time friendly
template<typename THeader = Header<>,
         typename TPoint = defines::Point3D<>,
         size_t MaxPoints = 256>
struct PointCloud {
    THeader header;
    sertial::fixed_vector<TPoint, MaxPoints> points;
};

/// Convenience aliases for common configurations
using PointCloudSmall = PointCloud<Header<>, defines::Point3D<>, 256>;
using PointCloudMedium = PointCloud<Header<>, defines::Point3D<>, 1024>;
using PointCloudLarge = PointCloud<Header<>, defines::Point3D<>, 4096>;

/// Point cloud with intensity data
template<typename THeader = Header<>,
         typename TPointIntensity = PointXYZI<>,
         size_t MaxPoints = 512>
struct PointCloudIntensity {
    THeader header;
    sertial::fixed_vector<TPointIntensity, MaxPoints> points;
};

} // namespace examples::messages
