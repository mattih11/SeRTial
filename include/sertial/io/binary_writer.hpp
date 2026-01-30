#pragma once

#include "../core/concepts.hpp"
#include "../core/traits.hpp"
#include "../core/size_computation.hpp"
#include "../containers/container_traits.hpp"
#include "varint.hpp"
#include <vector>
#include <span>
#include <cstring>
#include <concepts>
#include <string_view>

namespace sertial {

// ============================================================================
// BinaryWriter
// ============================================================================
/// Writes binary data to a dynamically growing buffer
/// Supports primitives, strings, containers, and custom types
/// Uses varint encoding for lengths and dynamic-sized values

class BinaryWriter {
public:
    BinaryWriter() = default;
    
    explicit BinaryWriter(std::size_t reserve_size) {
        buffer_.reserve(reserve_size);
    }
    
    // ========================================================================
    // Buffer Management
    // ========================================================================
    
    /// Get the current buffer contents
    std::span<const std::byte> data() const noexcept {
        return buffer_;
    }
    
    /// Get buffer as vector (for move semantics)
    std::vector<std::byte> take_buffer() noexcept {
        return std::move(buffer_);
    }
    
    /// Current size of written data
    std::size_t size() const noexcept {
        return buffer_.size();
    }
    
    /// Clear the buffer
    void clear() noexcept {
        buffer_.clear();
    }
    
    /// Reserve buffer capacity
    void reserve(std::size_t capacity) {
        buffer_.reserve(capacity);
    }
    
    // ========================================================================
    // Primitive Types (Fixed Size)
    // ========================================================================
    
    /// Write arithmetic type (int, float, etc.) in little-endian
    template<Arithmetic T>
    void write(T value) {
        const std::size_t old_size = buffer_.size();
        buffer_.resize(old_size + sizeof(T));
        std::memcpy(buffer_.data() + old_size, &value, sizeof(T));
    }
    
    /// Write boolean as single byte
    void write(bool value) {
        buffer_.push_back(static_cast<std::byte>(value ? 1 : 0));
    }
    
    // ========================================================================
    // Varint Encoding
    // ========================================================================
    
    /// Write unsigned integer as varint
    template<std::unsigned_integral T>
    void write_varint(T value) {
        std::byte temp[10];
        std::size_t bytes = encode_varint(static_cast<uint64_t>(value), temp);
        buffer_.insert(buffer_.end(), temp, temp + bytes);
    }
    
    /// Write signed integer as varint (zigzag encoded)
    template<std::signed_integral T>
    void write_varint(T value) {
        std::byte temp[10];
        std::size_t bytes = encode_varint(value, temp);
        buffer_.insert(buffer_.end(), temp, temp + bytes);
    }
    
    // ========================================================================
    // Strings
    // ========================================================================
    
    /// Write string: length (varint) + UTF-8 bytes
    void write(std::string_view str) {
        write_varint(str.size());
        write_bytes(std::as_bytes(std::span{str.data(), str.size()}));
    }
    
    void write(const std::string& str) {
        write(std::string_view{str});
    }
    
    void write(const char* str) {
        write(std::string_view{str});
    }
    
    /// Write fixed_string
    template<std::size_t N>
    void write(const fixed_string<N>& str) {
        write(std::string_view(str));
    }
    
    // ========================================================================
    // Raw Bytes
    // ========================================================================
    
    /// Write raw byte span
    void write_bytes(std::span<const std::byte> bytes) {
        buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
    }
    
    /// Write raw pointer + size (unsafe)
    void write_bytes_unsafe(const void* data, std::size_t size) {
        const std::byte* bytes = static_cast<const std::byte*>(data);
        buffer_.insert(buffer_.end(), bytes, bytes + size);
    }
    
    // ========================================================================
    // Containers - Dynamic Size
    // ========================================================================
    
    /// Write std::vector or similar dynamic container
    /// Format: count (varint) + elements
    template<typename T>
    requires requires(const T& c) {
        { c.size() } -> std::convertible_to<std::size_t>;
        { c.begin() } -> std::input_iterator;
        { c.end() } -> std::sentinel_for<decltype(c.begin())>;
    }
    void write_container(const T& container) {
        write_varint(container.size());
        for (const auto& elem : container) {
            write(elem);
        }
    }
    
    /// Write std::vector explicitly
    template<typename T>
    void write(const std::vector<T>& vec) {
        write_container(vec);
    }
    
    // ========================================================================
    // Containers - Fixed Capacity
    // ========================================================================
    
    /// Write fixed_vector: count (varint) + elements
    template<typename T, std::size_t N>
    void write(const fixed_vector<T, N>& vec) {
        write_varint(vec.size());
        for (const auto& elem : vec) {
            write(elem);
        }
    }
    
    // ========================================================================
    // Optimized Memcpy Paths
    // ========================================================================
    
    /// Write array of trivially copyable types (optimized memcpy)
    template<TriviallyCopyable T>
    void write_array(const T* data, std::size_t count) {
        const std::size_t bytes = count * sizeof(T);
        const std::size_t old_size = buffer_.size();
        buffer_.resize(old_size + bytes);
        std::memcpy(buffer_.data() + old_size, data, bytes);
    }
    
    /// Write std::span of trivially copyable types
    template<TriviallyCopyable T>
    void write_span(std::span<const T> data) {
        write_array(data.data(), data.size());
    }
    
    // ========================================================================
    // Structs (requires BinaryReflector specialization - Phase 3)
    // ========================================================================
    
    /// Write struct with custom reflector (implemented in Phase 3)
    template<typename T>
    requires HasCustomReflector<T>
    void write_struct(const T& value);
    
    // ========================================================================
    // Optimized Multi-Write (Pre-computed Size)
    // ========================================================================
    
    /// Write multiple values with pre-computed buffer size
    /// Allocates exact amount needed (compile-time bound for fixed containers)
    template<typename... Args>
    void write_all(const Args&... args) {
        // Use runtime computation (args are not constexpr)
        std::size_t needed = compute_total_size(args...);
        buffer_.reserve(buffer_.size() + needed);
        
        // Write all values
        (write(args), ...);
    }
    
private:
    std::vector<std::byte> buffer_;
};
// ============================================================================
// Convenience Functions
// ============================================================================

/// Serialize value to byte vector (auto-computes size)
template<typename T>
inline std::vector<std::byte> to_binary(const T& value) {
    BinaryWriter writer;
    
    // Use runtime computation
    writer.reserve(compute_serialized_size(value));
    writer.write(value);
    return writer.take_buffer();
}

/// Serialize multiple values with optimal pre-allocation
template<typename... Args>
inline std::vector<std::byte> to_binary_batch(const Args&... args) {
    BinaryWriter writer;
    writer.write_all(args...);
    return writer.take_buffer();
}

/// Serialize value with manual capacity override
template<typename T>
inline std::vector<std::byte> to_binary(const T& value, std::size_t reserve) {
    BinaryWriter writer(reserve);
    writer.write(value);
    return writer.take_buffer();
}

} // namespace sertial
