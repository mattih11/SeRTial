#pragma once
/// examples/example_collection.hpp
/// Standalone header that defines the example MessageCollection.
/// Used as the REGISTRY_HEADER for sertial_generate_schema() smoke-tests.

#include <sertial/integration/message_collection.hpp>
#include "defines/defines.hpp"
#include "defines/sensor_history.hpp"
#include "messages/messages.hpp"

using namespace sertial;
using namespace examples::defines;
using namespace examples::messages;

using ExampleMessages = MessageCollection<
    Point3D<float>,
    Point3D<double>,
    Quaternion<float>,
    Timestamp<>,
    Header<>,
    Position<>,
    PositionDouble,
    PointCloud<>,
    PointCloudSmall,
    PointCloudMedium,
    CameraInfo<>,
    Imu<>,
    ImuDouble,
    SensorHistory
>;
