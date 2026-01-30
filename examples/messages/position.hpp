#pragma once

#include <rfl.hpp>
#include "header.hpp"
#include "../defines/pose3d.hpp"

namespace examples::messages {

/// Position message with header and 3D pose
template<typename THeader = Header<>, 
         typename TPose = defines::Pose3D<>>
struct Position {
    THeader header;
    TPose pose;
};

/// Position with double precision
using PositionDouble = Position<
    Header<defines::Timestamp<>>,
    defines::Pose3D<defines::Point3D<double>, defines::Quaternion<double>>
>;

} // namespace examples::messages
