#pragma once

#include "../core/concepts.hpp"
#include "../core/traits.hpp"
#include "../io/binary_writer.hpp"
#include "../io/binary_reader.hpp"
#include <optional>

namespace sertial {

template<typename T>
struct BinaryReflector;  // Primary template intentionally undefined

template<typename T>
concept HasBinaryReflector = requires(BinaryWriter& w, BinaryReader& r, const T& value) {
    { BinaryReflector<T>::write(w, value) } -> std::same_as<void>;
    { BinaryReflector<T>::read(r) } -> std::same_as<std::optional<T>>;
};

template<typename T>
requires HasBinaryReflector<T>
inline void write_reflector(BinaryWriter& writer, const T& value) {
    BinaryReflector<T>::write(writer, value);
}

template<typename T>
requires HasBinaryReflector<T>
inline std::optional<T> read_reflector(BinaryReader& reader) {
    return BinaryReflector<T>::read(reader);
}

template<typename T>
requires HasBinaryReflector<T>
inline std::vector<std::byte> serialize(const T& value) {
    BinaryWriter writer;
    BinaryReflector<T>::write(writer, value);
    return writer.take_buffer();
}

template<typename T>
requires HasBinaryReflector<T>
inline std::optional<T> deserialize(std::span<const std::byte> data) {
    BinaryReader reader(data);
    return BinaryReflector<T>::read(reader);
}

template<typename T>
requires HasBinaryReflector<T>
inline std::optional<T> deserialize(const std::vector<std::byte>& data) {
    return deserialize<T>(std::span{data});
}

} // namespace sertial
