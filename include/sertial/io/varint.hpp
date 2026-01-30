#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <concepts>

namespace sertial {

// ============================================================================
// Varint Encoding/Decoding
// ============================================================================
// Variable-length integer encoding using LEB128 (Little Endian Base 128)
// - Each byte uses 7 bits for data, 1 bit for continuation
// - Small numbers use fewer bytes (efficient for lengths, counts)
// - Unsigned: supports full uint64_t range
// - Signed: uses zigzag encoding to efficiently handle negative numbers

/// Zigzag encoding: maps signed integers to unsigned for efficient varint encoding
/// Positive: 0->0, 1->2, 2->4, ...
/// Negative: -1->1, -2->3, -3->5, ...
template<std::signed_integral T>
constexpr auto zigzag_encode(T value) -> std::make_unsigned_t<T> {
    using U = std::make_unsigned_t<T>;
    constexpr int bits = sizeof(T) * 8 - 1;
    return static_cast<U>((value << 1) ^ (value >> bits));
}

/// Zigzag decoding: reverse of zigzag_encode
template<std::unsigned_integral U>
constexpr auto zigzag_decode(U value) -> std::make_signed_t<U> {
    using T = std::make_signed_t<U>;
    return static_cast<T>((value >> 1) ^ -(value & 1));
}

// ============================================================================
// Varint Size Calculation
// ============================================================================

/// Calculate how many bytes a varint-encoded value will require
constexpr std::size_t varint_size(uint64_t value) noexcept {
    if (value == 0) return 1;
    
    std::size_t bytes = 0;
    while (value != 0) {
        value >>= 7;
        ++bytes;
    }
    return bytes;
}

/// Calculate varint size for signed integers (using zigzag)
template<std::signed_integral T>
constexpr std::size_t varint_size(T value) noexcept {
    return varint_size(zigzag_encode(value));
}

// ============================================================================
// Varint Encoding
// ============================================================================

/// Encode unsigned integer as varint
/// Returns number of bytes written
/// Buffer must have at least 10 bytes for uint64_t (worst case)
inline std::size_t encode_varint(uint64_t value, std::span<std::byte> buffer) noexcept {
    std::size_t pos = 0;
    
    while (value >= 0x80) {
        buffer[pos++] = static_cast<std::byte>((value & 0x7F) | 0x80);
        value >>= 7;
    }
    
    buffer[pos++] = static_cast<std::byte>(value & 0x7F);
    return pos;
}

/// Encode signed integer as varint (using zigzag)
template<std::signed_integral T>
inline std::size_t encode_varint(T value, std::span<std::byte> buffer) noexcept {
    return encode_varint(zigzag_encode(value), buffer);
}

/// Encode unsigned integer with specified size
template<std::unsigned_integral T>
inline std::size_t encode_varint(T value, std::span<std::byte> buffer) noexcept {
    return encode_varint(static_cast<uint64_t>(value), buffer);
}

// ============================================================================
// Varint Decoding
// ============================================================================

/// Result of varint decoding
struct VarintResult {
    uint64_t value;      ///< Decoded value
    std::size_t bytes;   ///< Number of bytes consumed
    bool success;        ///< Whether decoding succeeded
};

/// Decode unsigned varint
/// Returns {value, bytes_consumed, success}
inline VarintResult decode_varint(std::span<const std::byte> buffer) noexcept {
    if (buffer.empty()) {
        return {0, 0, false};
    }
    
    uint64_t result = 0;
    std::size_t shift = 0;
    std::size_t pos = 0;
    
    while (pos < buffer.size()) {
        uint8_t byte_val = static_cast<uint8_t>(buffer[pos]);
        
        // Check for overflow (max 10 bytes for uint64_t)
        if (pos >= 10 || shift >= 64) {
            return {0, 0, false};
        }
        
        result |= static_cast<uint64_t>(byte_val & 0x7F) << shift;
        ++pos;
        
        // Check if this is the last byte
        if ((byte_val & 0x80) == 0) {
            return {result, pos, true};
        }
        
        shift += 7;
    }
    
    // Incomplete varint (buffer ended before finding terminal byte)
    return {0, 0, false};
}

/// Decode signed varint (using zigzag)
template<std::signed_integral T>
inline auto decode_varint_signed(std::span<const std::byte> buffer) noexcept 
    -> std::pair<T, std::size_t> 
{
    auto [value, bytes, success] = decode_varint(buffer);
    if (!success) {
        return {0, 0};
    }
    
    using U = std::make_unsigned_t<T>;
    return {zigzag_decode(static_cast<U>(value)), bytes};
}

// ============================================================================
// Convenience Wrappers
// ============================================================================

/// Encode varint to a byte pointer (unsafe, no bounds checking)
/// Returns number of bytes written
inline std::size_t encode_varint_unsafe(uint64_t value, std::byte* dest) noexcept {
    std::byte* ptr = dest;
    
    while (value >= 0x80) {
        *ptr++ = static_cast<std::byte>((value & 0x7F) | 0x80);
        value >>= 7;
    }
    
    *ptr++ = static_cast<std::byte>(value & 0x7F);
    return ptr - dest;
}

/// Decode varint from a byte pointer (unsafe, no bounds checking)
/// Returns {value, bytes_consumed}
inline std::pair<uint64_t, std::size_t> decode_varint_unsafe(const std::byte* src) noexcept {
    uint64_t result = 0;
    std::size_t shift = 0;
    std::size_t bytes = 0;
    
    while (true) {
        uint8_t byte_val = static_cast<uint8_t>(src[bytes]);
        result |= static_cast<uint64_t>(byte_val & 0x7F) << shift;
        ++bytes;
        
        if ((byte_val & 0x80) == 0) {
            break;
        }
        
        shift += 7;
        
        // Safety: prevent infinite loop
        if (bytes >= 10) {
            break;
        }
    }
    
    return {result, bytes};
}

} // namespace sertial
