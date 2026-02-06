#pragma once

#include "core/traits.hpp"
#include "core/endian.hpp"
#include "core/layout/struct_layout.hpp"  // Single source of truth
#include "containers/static_buffer.hpp"   // For to_buffer() compatibility
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
    
    /// @brief StructLayout for the serialized type (single source of truth)
    using Layout = StructLayout<T>;
    
    // ========================================================================
    // Size Information (Compile-Time)
    // ========================================================================
    
    /// @brief Base packed size (fixed fields only)
    static constexpr std::size_t base_packed_size = Layout::base_packed_size;
    
    /// @brief Maximum packed size (with all containers at max capacity)
    static constexpr std::size_t max_packed_size = Layout::max_packed_size;
    
    /// @brief Does the type have variable-size fields?
    static constexpr bool has_variable_fields = Layout::has_variable_fields;
    
    /// @brief Number of fields in the struct
    static constexpr std::size_t field_count = Layout::num_fields;
    
    // ========================================================================
    // Buffer Configuration (GUARANTEED Compile-Time)
    // ========================================================================
    
    /// @brief Is this type bounded (compile-time max size)?
    static constexpr bool is_bounded = true;  // Guaranteed by concept
    
    /// @brief Maximum buffer size needed for serialization
    static constexpr std::size_t max_buffer_size = max_packed_size;
    
    /// @brief Stack-allocated buffer type (clean alias)
    using buffer_type = typename Layout::buffer_type;
    
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
        // Direct call to StructLayout (single delegation layer)
        return Layout::serialize(value, std::span{buffer});
    }
    
    /// @brief Serialize to a static_buffer (compatibility)
    /// 
    /// @param value The value to serialize
    /// @return static_buffer containing the serialized bytes
    [[nodiscard]] static auto to_buffer(const T& value) {
        static_buffer<max_buffer_size> result;
        std::size_t size = Layout::serialize(value, std::span{result.data(), max_buffer_size});
        result.resize(size);
        return result;
    }
    
    // ========================================================================
    // Deserialization
    // ========================================================================
    
    /// @brief Deserialize from a byte span
    /// 
    /// @param data The serialized data
    /// @return Result containing the deserialized value or an error
    [[nodiscard]] static DeserializeResult<T> deserialize(std::span<const std::byte> data) {
        // Direct call to StructLayout (single delegation layer)
        auto opt = Layout::deserialize_opt(data);
        if (opt) {
            return DeserializeResult<T>{std::move(*opt)};
        }
        return MessageError("Deserialization failed");
    }
    
    /// @brief Deserialize from a byte span with endianness conversion
    /// 
    /// @param data The serialized data
    /// @param source_endian The endianness of the source data
    /// @return Result containing the deserialized value or an error
    /// 
    /// @note LIMITATION: Endianness conversion for variable-size fields (fixed_vector/fixed_string)
    /// is not yet implemented. This currently only works correctly for fixed-size structs.
    /// See TODO in hybrid_memory_map.hpp for planned implementation.
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
            // Check if this type has variable-size fields
            if constexpr (StructLayout<T>::has_variable_fields) {
                return MessageError("Endianness conversion for variable-size fields not yet implemented");
            }
            
            // Make a mutable copy for in-place conversion
            std::array<std::byte, max_buffer_size> temp;
            if (data.size() > max_buffer_size) {
                return MessageError("Data size exceeds buffer capacity");
            }
            std::copy(data.begin(), data.end(), temp.begin());
            
            // Swap endianness in-place (only works for fixed-size types)
            swap_endianness<T>(std::span<std::byte>{temp.data(), data.size()});
            
            // Deserialize from converted data
            auto result = Layout::deserialize_opt(std::span<const std::byte>{temp.data(), data.size()});
            if (result) {
                return DeserializeResult<T>{std::move(*result)};
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
        // Direct call to StructLayout (single delegation layer)
        return Layout::deserialize(out, data);
    }
    
    // ========================================================================
    // Utilities
    // ========================================================================
    
    /// @brief Compute the exact serialized size for a value (runtime)
    /// 
    /// @param value The value to measure
    /// @return Exact number of bytes needed
    [[nodiscard]] static std::size_t compute_size(const T& value) {
        // For fixed-size types, this is compile-time constant
        // For variable-size types, this evaluates actual container sizes
        return Layout::calculate_packed_size(value);
    }
    
    /// @brief Print memory layout information to stdout
    static void print_info() {
        std::cout << "Message<" << typeid(T).name() << ">:\n";
        std::cout << "  base_packed_size:  " << base_packed_size << " bytes\n";
        std::cout << "  max_packed_size:   " << max_packed_size << " bytes\n";
        std::cout << "  has_variable_fields: " << (has_variable_fields ? "yes" : "no") << "\n";
        std::cout << "  field_count:       " << field_count << "\n";
        std::cout << "  is_bounded:        yes (guaranteed)\n";
    }
};

// ============================================================================
// High-Level Convenience Functions (Message<T> based)
// ============================================================================

// Note: Low-level serialize_unified/deserialize_unified are in unified_binary.hpp
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
