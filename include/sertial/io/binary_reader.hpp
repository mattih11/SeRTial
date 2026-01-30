#pragma once

#include "../core/concepts.hpp"
#include "../core/traits.hpp"
#include "../containers/container_traits.hpp"
#include "../containers/fixed_vector.hpp"
#include "../containers/fixed_string.hpp"
#include "varint.hpp"
#include <vector>
#include <span>
#include <cstring>
#include <concepts>
#include <string>
#include <string_view>
#include <optional>

namespace sertial {

// ============================================================================
// BinaryReader
// ============================================================================
/// Reads binary data from a buffer with bounds checking
/// Supports primitives, strings, containers, and custom types
/// Uses varint decoding for lengths and dynamic-sized values
/// Provides zero-copy string_view access where possible

class BinaryReader {
public:
    /// Construct reader from byte buffer
    explicit BinaryReader(std::span<const std::byte> buffer) noexcept
        : buffer_(buffer), pos_(0) {}
    
    // ========================================================================
    // Buffer State
    // ========================================================================
    
    /// Current read position
    std::size_t position() const noexcept {
        return pos_;
    }
    
    /// Total buffer size
    std::size_t size() const noexcept {
        return buffer_.size();
    }
    
    /// Bytes remaining
    std::size_t remaining() const noexcept {
        return buffer_.size() - pos_;
    }
    
    /// Check if at end of buffer
    bool at_end() const noexcept {
        return pos_ >= buffer_.size();
    }
    
    /// Check if N bytes are available
    bool has_bytes(std::size_t n) const noexcept {
        return pos_ + n <= buffer_.size();
    }
    
    /// Reset to beginning
    void reset() noexcept {
        pos_ = 0;
    }
    
    /// Seek to position
    bool seek(std::size_t position) noexcept {
        if (position > buffer_.size()) {
            return false;
        }
        pos_ = position;
        return true;
    }
    
    // ========================================================================
    // Primitive Types (Fixed Size)
    // ========================================================================
    
    /// Read arithmetic type (int, float, etc.) in little-endian
    template<Arithmetic T>
    std::optional<T> read() {
        if (!has_bytes(sizeof(T))) {
            return std::nullopt;
        }
        
        T value;
        std::memcpy(&value, buffer_.data() + pos_, sizeof(T));
        pos_ += sizeof(T);
        return value;
    }
    
    /// Read boolean
    std::optional<bool> read_bool() {
        if (!has_bytes(1)) {
            return std::nullopt;
        }
        
        bool value = static_cast<uint8_t>(buffer_[pos_]) != 0;
        ++pos_;
        return value;
    }
    
    // ========================================================================
    // Varint Decoding
    // ========================================================================
    
    /// Read unsigned varint
    template<std::unsigned_integral T>
    std::optional<T> read_varint() {
        auto result = decode_varint(buffer_.subspan(pos_));
        if (!result.success) {
            return std::nullopt;
        }
        
        pos_ += result.bytes;
        return static_cast<T>(result.value);
    }
    
    /// Read signed varint (zigzag encoded)
    template<std::signed_integral T>
    std::optional<T> read_varint() {
        auto [value, bytes] = decode_varint_signed<T>(buffer_.subspan(pos_));
        if (bytes == 0) {
            return std::nullopt;
        }
        
        pos_ += bytes;
        return value;
    }
    
    // ========================================================================
    // Strings
    // ========================================================================
    
    /// Read string: length (varint) + UTF-8 bytes
    /// Returns std::string (allocates)
    std::optional<std::string> read_string() {
        auto length_opt = read_varint<std::size_t>();
        if (!length_opt) {
            return std::nullopt;
        }
        
        std::size_t length = *length_opt;
        if (!has_bytes(length)) {
            return std::nullopt;
        }
        
        std::string result(length, '\0');
        std::memcpy(result.data(), buffer_.data() + pos_, length);
        pos_ += length;
        return result;
    }
    
    /// Read string as string_view (zero-copy, lifetime tied to buffer)
    std::optional<std::string_view> read_string_view() {
        auto length_opt = read_varint<std::size_t>();
        if (!length_opt) {
            return std::nullopt;
        }
        
        std::size_t length = *length_opt;
        if (!has_bytes(length)) {
            return std::nullopt;
        }
        
        const char* str_data = reinterpret_cast<const char*>(buffer_.data() + pos_);
        pos_ += length;
        return std::string_view{str_data, length};
    }
    
    /// Read into fixed_string (no allocation)
    template<std::size_t N>
    std::optional<fixed_string<N>> read_fixed_string() {
        auto length_opt = read_varint<std::size_t>();
        if (!length_opt) {
            return std::nullopt;
        }
        
        std::size_t length = *length_opt;
        if (length > N || !has_bytes(length)) {
            return std::nullopt;
        }
        
        fixed_string<N> result;
        std::memcpy(result.data_unsafe(), buffer_.data() + pos_, length);
        result.set_size_unsafe(length);
        pos_ += length;
        return result;
    }
    
    // ========================================================================
    // Raw Bytes
    // ========================================================================
    
    /// Read raw bytes into span (must be pre-allocated)
    bool read_bytes(std::span<std::byte> dest) {
        if (!has_bytes(dest.size())) {
            return false;
        }
        
        std::memcpy(dest.data(), buffer_.data() + pos_, dest.size());
        pos_ += dest.size();
        return true;
    }
    
    /// Peek at bytes without advancing position (zero-copy)
    std::optional<std::span<const std::byte>> peek_bytes(std::size_t count) const {
        if (!has_bytes(count)) {
            return std::nullopt;
        }
        
        return buffer_.subspan(pos_, count);
    }
    
    /// Skip N bytes
    bool skip(std::size_t count) noexcept {
        if (!has_bytes(count)) {
            return false;
        }
        
        pos_ += count;
        return true;
    }
    
    // ========================================================================
    // Containers - Dynamic Size
    // ========================================================================
    
    /// Read std::vector with element reader function
    /// Format: count (varint) + elements
    template<typename T, typename ReaderFunc>
    std::optional<std::vector<T>> read_vector(ReaderFunc&& read_element) {
        auto count_opt = read_varint<std::size_t>();
        if (!count_opt) {
            return std::nullopt;
        }
        
        std::size_t count = *count_opt;
        std::vector<T> result;
        result.reserve(count);
        
        for (std::size_t i = 0; i < count; ++i) {
            auto elem_opt = read_element(*this);
            if (!elem_opt) {
                return std::nullopt;
            }
            result.push_back(std::move(*elem_opt));
        }
        
        return result;
    }
    
    /// Read std::vector<T> where T has read() support
    template<typename T>
    std::optional<std::vector<T>> read_vector() {
        return read_vector<T>([](BinaryReader& r) { return r.read<T>(); });
    }
    
    // ========================================================================
    // Containers - Fixed Capacity
    // ========================================================================
    
    /// Read fixed_vector: count (varint) + elements
    template<typename T, std::size_t N, typename ReaderFunc>
    std::optional<fixed_vector<T, N>> read_fixed_vector(ReaderFunc&& read_element) {
        auto count_opt = read_varint<std::size_t>();
        if (!count_opt) {
            return std::nullopt;
        }
        
        std::size_t count = *count_opt;
        if (count > N) {
            return std::nullopt;
        }
        
        fixed_vector<T, N> result;
        for (std::size_t i = 0; i < count; ++i) {
            auto elem_opt = read_element(*this);
            if (!elem_opt) {
                return std::nullopt;
            }
            result.push_back(std::move(*elem_opt));
        }
        
        return result;
    }
    
    /// Read fixed_vector<T, N> where T has read() support
    template<typename T, std::size_t N>
    std::optional<fixed_vector<T, N>> read_fixed_vector() {
        return read_fixed_vector<T, N>([](BinaryReader& r) { return r.read<T>(); });
    }
    
    // ========================================================================
    // Optimized Memcpy Paths
    // ========================================================================
    
    /// Read array of trivially copyable types (optimized memcpy)
    template<TriviallyCopyable T>
    bool read_array(T* dest, std::size_t count) {
        const std::size_t bytes = count * sizeof(T);
        if (!has_bytes(bytes)) {
            return false;
        }
        
        std::memcpy(dest, buffer_.data() + pos_, bytes);
        pos_ += bytes;
        return true;
    }
    
    /// Read into std::span of trivially copyable types
    template<TriviallyCopyable T>
    bool read_span(std::span<T> dest) {
        return read_array(dest.data(), dest.size());
    }
    
    // ========================================================================
    // Structs (requires BinaryReflector specialization - Phase 3)
    // ========================================================================
    
    /// Read struct with custom reflector (implemented in Phase 3)
    template<typename T>
    requires HasCustomReflector<T>
    std::optional<T> read_struct();
    
private:
    std::span<const std::byte> buffer_;
    std::size_t pos_;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/// Deserialize value from byte buffer
template<typename T>
inline std::optional<T> from_binary(std::span<const std::byte> data) {
    BinaryReader reader(data);
    return reader.read<T>();
}

/// Deserialize value from byte vector
template<typename T>
inline std::optional<T> from_binary(const std::vector<std::byte>& data) {
    return from_binary<T>(std::span{data});
}

} // namespace sertial
