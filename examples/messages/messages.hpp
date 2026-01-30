#pragma once

/// Example message types for SeRTial serialization
/// 
/// All messages use templated field types from examples/defines/
/// allowing flexible precision (float/double) and composition.
///
/// Usage:
///   #include <examples/messages/messages.hpp>
///   
///   using namespace examples::messages;
///   Position<> pos;  // Default float precision
///   PositionDouble pos_d;  // Double precision alias

#include "header.hpp"
#include "position.hpp"
#include "pointcloud.hpp"
#include "camera.hpp"
#include "imu.hpp"
