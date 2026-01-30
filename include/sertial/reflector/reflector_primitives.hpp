#pragma once

#include "binary_reflector.hpp"
#include "../io/binary_writer.hpp"
#include "../io/binary_reader.hpp"
#include <cstdint>
#include <optional>

namespace sertial {

// Boolean
template<>
struct BinaryReflector<bool> {
    static void write(BinaryWriter& writer, bool value) { writer.write(value); }
    static std::optional<bool> read(BinaryReader& reader) { return reader.read_bool(); }
};

// Signed integers
template<> struct BinaryReflector<int8_t> {
    static void write(BinaryWriter& writer, int8_t value) { writer.write(value); }
    static std::optional<int8_t> read(BinaryReader& reader) { return reader.read<int8_t>(); }
};
template<> struct BinaryReflector<int16_t> {
    static void write(BinaryWriter& writer, int16_t value) { writer.write(value); }
    static std::optional<int16_t> read(BinaryReader& reader) { return reader.read<int16_t>(); }
};
template<> struct BinaryReflector<int32_t> {
    static void write(BinaryWriter& writer, int32_t value) { writer.write(value); }
    static std::optional<int32_t> read(BinaryReader& reader) { return reader.read<int32_t>(); }
};
template<> struct BinaryReflector<int64_t> {
    static void write(BinaryWriter& writer, int64_t value) { writer.write(value); }
    static std::optional<int64_t> read(BinaryReader& reader) { return reader.read<int64_t>(); }
};

// Unsigned integers
template<> struct BinaryReflector<uint8_t> {
    static void write(BinaryWriter& writer, uint8_t value) { writer.write(value); }
    static std::optional<uint8_t> read(BinaryReader& reader) { return reader.read<uint8_t>(); }
};
template<> struct BinaryReflector<uint16_t> {
    static void write(BinaryWriter& writer, uint16_t value) { writer.write(value); }
    static std::optional<uint16_t> read(BinaryReader& reader) { return reader.read<uint16_t>(); }
};
template<> struct BinaryReflector<uint32_t> {
    static void write(BinaryWriter& writer, uint32_t value) { writer.write(value); }
    static std::optional<uint32_t> read(BinaryReader& reader) { return reader.read<uint32_t>(); }
};
template<> struct BinaryReflector<uint64_t> {
    static void write(BinaryWriter& writer, uint64_t value) { writer.write(value); }
    static std::optional<uint64_t> read(BinaryReader& reader) { return reader.read<uint64_t>(); }
};

// Floating point
template<> struct BinaryReflector<float> {
    static void write(BinaryWriter& writer, float value) { writer.write(value); }
    static std::optional<float> read(BinaryReader& reader) { return reader.read<float>(); }
};
template<> struct BinaryReflector<double> {
    static void write(BinaryWriter& writer, double value) { writer.write(value); }
    static std::optional<double> read(BinaryReader& reader) { return reader.read<double>(); }
};

// Character
template<> struct BinaryReflector<char> {
    static void write(BinaryWriter& writer, char value) { writer.write(static_cast<uint8_t>(value)); }
    static std::optional<char> read(BinaryReader& reader) {
        auto byte = reader.read<uint8_t>();
        return byte ? std::optional<char>(static_cast<char>(*byte)) : std::nullopt;
    }
};

} // namespace sertial
