#pragma once

#include "core/traits.hpp"
#include "core/endian.hpp"
#include "io/optimized_binary.hpp"
#include <array>
#include <cstddef>
#include <span>
#include <optional>
#include <string>
#include <bit>

namespace sertial {

// ============================================================================
// Error Type
// ============================================================================

/// @brief Error type for deserialization failures
struct MessageError {
    std::string what;
    
    MessageError() = default;
    explicit MessageError(std::string msg) : what(std::move(msg)) {}
    explicit MessageError(const char* msg) : what(msg) {}
};

// ============================================================================
// Result Type (C++20 compatible alternative to std::expected)
// ============================================================================

/// @brief Simple result type for operations that can fail
template<typename T>
class DeserializeResult {
    std::optional<T> value_;
    MessageError error_;
    
public:
    DeserializeResult(T val) : value_(std::move(val)) {}
    DeserializeResult(MessageError err) : error_(std::move(err)) {}
    
    explicit operator bool() const noexcept { return value_.has_value(); }
    
    T& operator*() & { return *value_; }
    const T& operator*() const& { return *value_; }
    T&& operator*() && { return std::move(*value_); }
    
    T* operator->() { return &*value_; }
    const T* operator->() const { return &*value_; }
    
    const MessageError& error() const& { return error_; }
};

// ============================================================================
// Message<T> - Zero-Allocation Binary Serialization Interface
// ============================================================================

/// @brief High-level interface for binary serialization of type T
/// 
/// Features:
/// - Compile-time buffer sizing (GUARANTEED no runtime allocation)
/// - Stack-allocated buffers via buffer_type
/// - Padding and memory layout analysis
/// - Optimized memcpy paths
/// 
/// REQUIREMENTS:
/// - T must be BoundedSerializable (all containers have compile-time max sizes)
/// - Use fixed_string<N> instead of std::string
/// - Use fixed_vector<T, N> instead of std::vector<T>
/// 
/// @tparam T The type to serialize (must satisfy BoundedSerializable)
/// 
/// @example
/// ```cpp
/// struct Player { 
///     uint32_t id; 
///     float x, y, z;
///     fixed_string<32> name;  // NOT std::string!
/// };
/// 
/// Player p{42, 1.0f, 2.0f, 3.0f, "Alice"};
/// auto result = Message<Player>::serialize(p);
/// send(result.view());
/// 
/// auto restored = Message<Player>::deserialize(received);
/// ```
template<typename T>
    requires BoundedSerializable<T>
struct Message {
    // Static assertion for clear error message
    static_assert(is_bounded_v<T>,
        "Message<T> requires T to be BoundedSerializable. "
        "Use fixed_string<N> instead of std::string, "
        "and fixed_vector<T, N> instead of std::vector<T>. "
        "All containers must have compile-time known maximum sizes.");

    // ========================================================================
    // Type Information (Compile-Time)
    // ========================================================================
    
    /// @brief The serialized type
    using value_type = T;
    
    /// @brief TypeTraits for the serialized type
    using traits = TypeTraits<T>;
    
    /// @brief MemoryMap for the serialized type
    using memory_map = MemoryMap<T>;
    
    // ========================================================================
    // Size Information (Compile-Time)
    // ========================================================================
    
    /// @brief Packed binary size (sum of field sizes, no padding)
    static constexpr std::size_t packed_size = traits::packed_size;
    
    /// @brief Actual struct size in memory (with padding)
    static constexpr std::size_t unpacked_size = traits::unpacked_size;
    
    /// @brief Padding bytes in the struct
    static constexpr std::size_t padding_bytes = unpacked_size - packed_size;
    
    /// @brief Does the struct have internal padding?
    static constexpr bool has_padding = traits::has_padding;
    
    // ========================================================================
    // Memory Layout Information (Compile-Time)
    // ========================================================================
    
    /// @brief Number of fields in the struct
    static constexpr std::size_t field_count = memory_map::field_count;
    
    /// @brief Number of memcpy operations needed for optimal serialization
    static constexpr std::size_t memcpy_count = memory_map::memcpy_region_count;
    
    /// @brief Can we serialize the entire struct with one memcpy?
    static constexpr bool can_single_memcpy = memory_map::can_single_memcpy;
    
    // ========================================================================
    // Buffer Configuration (GUARANTEED Compile-Time)
    // ========================================================================
    
    /// @brief Is this type bounded (compile-time max size)?
    static constexpr bool is_bounded = true;  // Guaranteed by concept
    
    /// @brief Maximum buffer size needed for serialization (EXACT, not heuristic)
    /// This is computed at compile-time from the struct definition.
    /// The buffer is guaranteed to fit any valid serialization.
    /// Uses MemoryMap::packed_size since we use optimized serialization.
    static constexpr std::size_t max_buffer_size = memory_map::packed_size;
    
    /// @brief Stack-allocated buffer type (zero heap allocation)
    using buffer_type = std::array<std::byte, max_buffer_size>;
    
    // ========================================================================
    // Serialization Result
    // ========================================================================
    
    /// @brief Result of serialization containing buffer and actual size
    struct Result {
        buffer_type buffer{};       ///< The serialized data (stack-allocated)
        std::size_t size = 0;       ///< Actual bytes written
        
        /// @brief Get a view of the serialized data
        [[nodiscard]] std::span<const std::byte> view() const noexcept {
            return {buffer.data(), size};
        }
        
        /// @brief Get a mutable view of the serialized data
        [[nodiscard]] std::span<std::byte> data() noexcept {
            return {buffer.data(), size};
        }
        
        /// @brief Check if serialization succeeded (size > 0)
        [[nodiscard]] explicit operator bool() const noexcept {
            return size > 0;
        }
    };
    
    // ========================================================================
    // Serialization
    // ========================================================================
    
    /// @brief Serialize a value to a Result (stack-allocated buffer)
    /// 
    /// @param value The value to serialize
    /// @return Result containing buffer and actual size
    [[nodiscard]] static Result serialize(const T& value) {
        Result result;
        result.size = serialize_to(value, result.buffer);
        return result;
    }
    
    /// @brief Serialize a value into a provided buffer
    /// 
    /// @param value The value to serialize
    /// @param buffer The buffer to write to
    /// @return Number of bytes written
    static std::size_t serialize_to(const T& value, buffer_type& buffer) {
        // Use optimized memcpy-based serialization
        return sertial::serialize_to(value, buffer.data());
    }
    
    /// @brief Serialize to a static_buffer (zero allocation)
    /// 
    /// @param value The value to serialize
    /// @return static_buffer containing the serialized bytes
    [[nodiscard]] static auto to_buffer(const T& value) {
        return sertial::serialize(value);
    }
    
    // ========================================================================
    // Deserialization
    // ========================================================================
    
    /// @brief Deserialize from a byte span
    /// 
    /// @param data The serialized data
    /// @return Result containing the deserialized value or an error
    [[nodiscard]] static DeserializeResult<T> deserialize(std::span<const std::byte> data) {
        auto result = sertial::deserialize<T>(data);
        if (result) {
            return *result;
        }
        return MessageError("Deserialization failed");
    }
    
    /// @brief Deserialize from a byte span with endianness conversion
    /// 
    /// @param data The serialized data
    /// @param source_endian The endianness of the source data
    /// @return Result containing the deserialized value or an error
    /// 
    /// @note This performs zero-cost endianness conversion using compile-time
    /// field information. The conversion happens in-place on a copy of the data
    /// before deserialization.
    /// 
    /// @example
    /// ```cpp
    /// auto buffer = receive_from_network();
    /// auto msg = Message<MyMessage>::deserialize(buffer, std::endian::big);
    /// ```
    [[nodiscard]] static DeserializeResult<T> deserialize(
        std::span<const std::byte> data, 
        std::endian source_endian) 
    {
        // Only convert if endianness differs
        if (source_endian != std::endian::native) {
            // Make a mutable copy for in-place conversion
            std::array<std::byte, max_buffer_size> temp;
            if (data.size() > max_buffer_size) {
                return MessageError("Data size exceeds buffer capacity");
            }
            std::copy(data.begin(), data.end(), temp.begin());
            
            // Swap endianness in-place
            swap_endianness<T>(std::span<std::byte>{temp.data(), data.size()});
            
            // Deserialize from converted data
            auto result = sertial::deserialize<T>(std::span<const std::byte>{temp.data(), data.size()});
            if (result) {
                return *result;
            }
            return MessageError("Deserialization failed after endian conversion");
        }
        
        // No conversion needed
        return deserialize(data);
    }
    
    /// @brief Deserialize from a static_buffer
    /// 
    /// @param buffer The serialized data
    /// @return Result containing the deserialized value or an error
    template<std::size_t N>
    [[nodiscard]] static DeserializeResult<T> deserialize(const static_buffer<N>& buffer) {
        return deserialize(buffer.view());
    }
    
    /// @brief Deserialize from a Result
    /// 
    /// @param result A previous serialization result
    /// @return Result containing the deserialized value or an error
    [[nodiscard]] static DeserializeResult<T> deserialize(const Result& result) {
        return deserialize(result.view());
    }
    
    /// @brief Deserialize into an existing object (avoids construction)
    /// 
    /// @param data The serialized data
    /// @param out The object to deserialize into
    /// @return true if successful, false otherwise
    [[nodiscard]] static bool deserialize_into(
        std::span<const std::byte> data, T& out) 
    {
        return sertial::deserialize_into(data, out);
    }
    
    // ========================================================================
    // Utilities
    // ========================================================================
    
    /// @brief Compute the exact serialized size for a value (runtime)
    /// 
    /// @param value The value to measure
    /// @return Exact number of bytes needed
    [[nodiscard]] static std::size_t compute_size([[maybe_unused]] const T& value) {
        // With optimized serialization, size is known at compile-time
        return memory_map::packed_size;
    }
    
    /// @brief Print memory layout information to stdout
    static void print_info() {
        std::cout << "Message<" << typeid(T).name() << ">:\n";
        std::cout << "  packed_size:      " << packed_size << " bytes\n";
        std::cout << "  unpacked_size:    " << unpacked_size << " bytes\n";
        std::cout << "  padding_bytes:    " << padding_bytes << " bytes\n";
        std::cout << "  has_padding:      " << (has_padding ? "yes" : "no") << "\n";
        std::cout << "  field_count:      " << field_count << "\n";
        std::cout << "  memcpy_count:     " << memcpy_count << "\n";
        std::cout << "  can_single_memcpy:" << (can_single_memcpy ? "yes" : "no") << "\n";
        std::cout << "  max_buffer_size:  " << max_buffer_size << " bytes (exact)\n";
        std::cout << "  is_bounded:       yes (guaranteed)\n";
    }
};

// ============================================================================
// High-Level Convenience Functions (Message<T> based)
// ============================================================================

// Note: Low-level serialize/deserialize are in optimized_binary.hpp
// These provide the Message<T> interface with Result types

/// @brief Serialize with Message<T> interface (returns Result with buffer)
template<BoundedSerializable T>
[[nodiscard]] auto message_serialize(const T& value) {
    return Message<T>::serialize(value);
}

/// @brief Deserialize with Message<T> interface (returns DeserializeResult)
template<BoundedSerializable T>
[[nodiscard]] auto message_deserialize(std::span<const std::byte> data) {
    return Message<T>::deserialize(data);
}

} // namespace sertial
