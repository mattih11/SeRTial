#pragma once

#include "../../include/sertial/containers/ring_buffer.hpp"
#include <cstdint>

namespace examples::defines {

/**
 * @brief Example message demonstrating RingBuffer serialization
 * 
 * SensorHistory maintains a circular buffer of the last N sensor readings.
 * Perfect for real-time systems that need recent history without unbounded growth.
 * 
 * Serialization behavior:
 * - Only size() elements are serialized (not full capacity)
 * - Linearized in logical order (oldest → newest)
 * - Wrap-around handled automatically
 * - Format: [length:4][readings:size*sizeof(float)]
 */
struct SensorHistory {
    uint32_t sensor_id;
    sertial::RingBuffer<float, 100> readings;  ///< Last 100 readings (FIFO)
    uint64_t timestamp;
};

}  // namespace examples::defines
