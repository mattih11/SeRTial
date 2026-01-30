#pragma once

#include "binary_reflector.hpp"
#include "../io/binary_writer.hpp"
#include "../io/binary_reader.hpp"
#include "../containers/fixed_string.hpp"
#include <string>
#include <string_view>
#include <optional>

namespace sertial {

// ============================================================================
// String Type Reflectors
// ============================================================================

// std::string
template<>
struct BinaryReflector<std::string> {
    static void write(BinaryWriter& writer, const std::string& value) {
        writer.write(value);
    }
    
    static std::optional<std::string> read(BinaryReader& reader) {
        return reader.read_string();
    }
};

// std::string_view - Note: serialize only, cannot deserialize (lifetime issues)
// For deserialization, use read_string_view() directly on BinaryReader

// fixed_string<N>
template<std::size_t N>
struct BinaryReflector<fixed_string<N>> {
    static void write(BinaryWriter& writer, const fixed_string<N>& value) {
        writer.write(value);
    }
    
    static std::optional<fixed_string<N>> read(BinaryReader& reader) {
        return reader.read_fixed_string<N>();
    }
};

// C-string (const char*) - Write only
// Note: Cannot deserialize to const char* safely (lifetime/ownership)
struct CStringReflector {
    static void write(BinaryWriter& writer, const char* value) {
        writer.write(value);
    }
    
    // No read() - use std::string or fixed_string for deserialization
};

} // namespace sertial
