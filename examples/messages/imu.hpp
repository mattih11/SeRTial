#pragma once

#include <rfl.hpp>
#include "header.hpp"
#include "../defines/point3d.hpp"
#include "../defines/quaternion.hpp"

namespace examples::messages {

/// IMU (Inertial Measurement Unit) message
template<typename THeader = Header<>,
         typename TVector3 = defines::Point3D<>,
         typename TQuaternion = defines::Quaternion<>>
struct Imu {
    THeader header;
    
    TQuaternion orientation;      // Orientation (quaternion)
    TVector3 angular_velocity;    // Angular velocity (rad/s)
    TVector3 linear_acceleration; // Linear acceleration (m/s^2)
};

/// IMU with double precision
using ImuDouble = Imu<
    Header<defines::Timestamp<>>,
    defines::Point3D<double>,
    defines::Quaternion<double>
>;

} // namespace examples::messages
