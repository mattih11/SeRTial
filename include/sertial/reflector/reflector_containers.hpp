#pragma once

#include "binary_reflector.hpp"
#include "../io/binary_writer.hpp"
#include "../io/binary_reader.hpp"
#include "../containers/fixed_vector.hpp"
#include <vector>
#include <array>
#include <optional>

namespace sertial {

// ============================================================================
// Container Type Reflectors
// ============================================================================

// std::vector<T> - arithmetic types use optimized memcpy path
template<Arithmetic T>
struct BinaryReflector<std::vector<T>> {
    static void write(BinaryWriter& writer, const std::vector<T>& value) {
        writer.write_varint(value.size());
        writer.write_array(value.data(), value.size());
    }
    
    static std::optional<std::vector<T>> read(BinaryReader& reader) {
        auto size_opt = reader.read_varint<std::size_t>();
        if (!size_opt) return std::nullopt;
        
        std::size_t size = *size_opt;
        std::vector<T> result(size);
        
        if (!reader.read_array(result.data(), size)) {
            return std::nullopt;
        }
        
        return result;
    }
};

// std::vector<T> - non-arithmetic types use element-wise serialization
template<typename T>
requires (!Arithmetic<T> && HasBinaryReflector<T>)
struct BinaryReflector<std::vector<T>> {
    static void write(BinaryWriter& writer, const std::vector<T>& value) {
        writer.write_varint(value.size());
        for (const auto& elem : value) {
            BinaryReflector<T>::write(writer, elem);
        }
    }
    
    static std::optional<std::vector<T>> read(BinaryReader& reader) {
        auto size_opt = reader.read_varint<std::size_t>();
        if (!size_opt) return std::nullopt;
        
        std::size_t size = *size_opt;
        std::vector<T> result;
        result.reserve(size);
        
        for (std::size_t i = 0; i < size; ++i) {
            auto elem = BinaryReflector<T>::read(reader);
            if (!elem) return std::nullopt;
            result.push_back(std::move(*elem));
        }
        
        return result;
    }
};

// fixed_vector<T, N> - arithmetic types use optimized memcpy path
template<Arithmetic T, std::size_t N>
struct BinaryReflector<fixed_vector<T, N>> {
    static void write(BinaryWriter& writer, const fixed_vector<T, N>& value) {
        writer.write_varint(value.size());
        writer.write_array(value.data(), value.size());
    }
    
    static std::optional<fixed_vector<T, N>> read(BinaryReader& reader) {
        auto size_opt = reader.read_varint<std::size_t>();
        if (!size_opt) return std::nullopt;
        
        std::size_t size = *size_opt;
        if (size > N) return std::nullopt;
        
        fixed_vector<T, N> result;
        result.resize(size);  // Safe: size <= N
        
        if (!reader.read_array(result.data(), size)) {
            return std::nullopt;
        }
        
        return result;
    }
};

// fixed_vector<T, N> - non-arithmetic types use element-wise serialization
template<typename T, std::size_t N>
requires (!Arithmetic<T> && HasBinaryReflector<T>)
struct BinaryReflector<fixed_vector<T, N>> {
    static void write(BinaryWriter& writer, const fixed_vector<T, N>& value) {
        writer.write_varint(value.size());
        for (const auto& elem : value) {
            BinaryReflector<T>::write(writer, elem);
        }
    }
    
    static std::optional<fixed_vector<T, N>> read(BinaryReader& reader) {
        auto size_opt = reader.read_varint<std::size_t>();
        if (!size_opt) return std::nullopt;
        
        std::size_t size = *size_opt;
        if (size > N) return std::nullopt;  // Overflow check
        
        fixed_vector<T, N> result;
        for (std::size_t i = 0; i < size; ++i) {
            auto elem = BinaryReflector<T>::read(reader);
            if (!elem) return std::nullopt;
            result.push_back(std::move(*elem));
        }
        
        return result;
    }
};

// std::array<T, N> - arithmetic types use optimized memcpy path
template<Arithmetic T, std::size_t N>
struct BinaryReflector<std::array<T, N>> {
    static void write(BinaryWriter& writer, const std::array<T, N>& value) {
        writer.write_array(value.data(), N);
    }
    
    static std::optional<std::array<T, N>> read(BinaryReader& reader) {
        std::array<T, N> result;
        if (!reader.read_array(result.data(), N)) {
            return std::nullopt;
        }
        return result;
    }
};

// std::array<T, N> - non-arithmetic types use element-wise serialization
// Note: Arrays serialize all N elements (fixed size, no length prefix)
template<typename T, std::size_t N>
requires (!Arithmetic<T> && HasBinaryReflector<T>)
struct BinaryReflector<std::array<T, N>> {
    static void write(BinaryWriter& writer, const std::array<T, N>& value) {
        // No size prefix - array size is known at compile-time
        for (const auto& elem : value) {
            BinaryReflector<T>::write(writer, elem);
        }
    }
    
    static std::optional<std::array<T, N>> read(BinaryReader& reader) {
        std::array<T, N> result;
        for (std::size_t i = 0; i < N; ++i) {
            auto elem = BinaryReflector<T>::read(reader);
            if (!elem) return std::nullopt;
            result[i] = std::move(*elem);
        }
        return result;
    }
};

} // namespace sertial
