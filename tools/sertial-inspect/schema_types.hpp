#pragma once

#include <sertial/core/layout/struct_layout.hpp>
#include <sertial/integration/schema_generator.hpp>
#include <rfl.hpp>

namespace sertial::tools {

// We don't need to redefine anything!
// We already have:
// - StructLayout<T> with full reflector support
// - SchemaOutput from schema_generator.hpp
// - All types are already rfl-compatible

// Just use the existing types directly:
using SchemaOutput = sertial::SchemaOutput;

} // namespace sertial::tools
