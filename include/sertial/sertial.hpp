#pragma once

// ============================================================================
// SeRTial - Zero-Allocation Binary Serialization Library
// ============================================================================
// 
// A high-performance C++20 binary serialization library using compile-time
// reflection with reflect-cpp. Features zero runtime allocation through
// stack-allocated buffers with compile-time size computation.
//
// Quick Start:
//   #include <sertial/sertial.hpp>
//
//   struct Player { uint32_t id; float x, y, z; };
//
//   Player p{42, 1.0f, 2.0f, 3.0f};
//   auto result = sertial::serialize(p);
//   auto restored = sertial::deserialize<Player>(result.view());
//
// For more control:
//   using Msg = sertial::Message<Player>;
//   static_assert(Msg::packed_size == 16);
//   static_assert(!Msg::has_padding);

// Main API
#include "message.hpp"

// Core modules (usually not needed directly)
#include "core/concepts.hpp"
#include "core/traits.hpp"

// Containers
#include "containers/fixed_string.hpp"
#include "containers/fixed_vector.hpp"
#include "containers/static_buffer.hpp"
#include "containers/container_traits.hpp"

// I/O (low-level)
#include "io/binary_writer.hpp"
#include "io/binary_reader.hpp"
#include "io/varint.hpp"
#include "io/optimized_binary.hpp"

// Reflector system
#include "reflector/binary_reflector.hpp"

// Integration
#include "integration/message_collection.hpp"
#include "integration/schema_generator.hpp"
#include "integration/runtime_test.hpp"

// Debug utilities (optional - for development/testing)
// #include "debug/print_utils.hpp"  // Uncomment for debug utilities

namespace sertial {

// Version information
inline constexpr int VERSION_MAJOR = 1;
inline constexpr int VERSION_MINOR = 0;
inline constexpr int VERSION_PATCH = 0;
inline constexpr const char* VERSION_STRING = "1.0.0";

} // namespace sertial
