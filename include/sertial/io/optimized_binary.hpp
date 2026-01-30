#pragma once

#include "../core/traits/memory_map.hpp"
#include "../traits/container_detection.hpp"
#include "../containers/static_buffer.hpp"
#include <cstring>
#include <span>
#include <optional>

namespace sertial {

// ============================================================================
// Optimized Binary Serialization using MemoryMap<T> regions
// ZERO ALLOCATION - All operations use static buffers
// ============================================================================

/// @brief Serialize a trivially copyable struct to a static_buffer
/// Returns a static_buffer sized exactly for the type's packed size
/// @example auto buf = serialize(my_position);
template<typename T>
requires std::is_trivially_copyable_v<T>
[[nodiscard]] inline auto serialize(const T& value) {
    using MM = MemoryMap<T>;
    
    static_buffer<MM::packed_size> buffer(MM::packed_size);
    const std::byte* src = reinterpret_cast<const std::byte*>(&value);
    
    if constexpr (MM::can_single_memcpy) {
        std::memcpy(buffer.data(), src, MM::packed_size);
    } else {
        for (std::size_t i = 0; i < MM::memcpy_region_count; ++i) {
            const auto& region = MM::memcpy_regions[i];
            std::memcpy(
                buffer.data() + region.dst_offset,
                src + region.src_offset,
                region.size
            );
        }
    }
    
    return buffer;
}

/// @brief Serialize into an existing static_buffer
/// Returns the number of bytes written, or 0 if buffer too small
template<typename T, std::size_t N>
requires std::is_trivially_copyable_v<T>
inline std::size_t serialize_to(const T& value, static_buffer<N>& buffer) {
    using MM = MemoryMap<T>;
    
    static_assert(N >= MM::packed_size, "Buffer too small for type");
    
    const std::byte* src = reinterpret_cast<const std::byte*>(&value);
    
    if constexpr (MM::can_single_memcpy) {
        std::memcpy(buffer.data(), src, MM::packed_size);
    } else {
        for (std::size_t i = 0; i < MM::memcpy_region_count; ++i) {
            const auto& region = MM::memcpy_regions[i];
            std::memcpy(
                buffer.data() + region.dst_offset,
                src + region.src_offset,
                region.size
            );
        }
    }
    
    buffer.resize(MM::packed_size);
    return MM::packed_size;
}

/// @brief Serialize to a raw byte pointer (caller must ensure sufficient space)
/// Returns the number of bytes written
/// 
/// **IMPORTANT**: Structs with fixed containers (fixed_vector/fixed_string) should
/// use the reflector API instead, as this path copies the entire capacity.
/// This is caught at compile-time via static_assert.
template<typename T>
requires std::is_trivially_copyable_v<T>
inline std::size_t serialize_to(const T& value, std::byte* dest) {
    using MM = MemoryMap<T>;
    
    // Compile-time check: fail if struct contains fixed containers
    // (they need reflector path for proper size() handling)
    static_assert(!detail::struct_has_fixed_containers<T>(),
        "Structs with fixed_vector/fixed_string must use BinaryReflector API "
        "for proper runtime size handling. Use write_reflector(writer, value) instead.");
    
    // Safe to use optimized memcpy - no variable-size fields
    const std::byte* src = reinterpret_cast<const std::byte*>(&value);
    
    if constexpr (MM::can_single_memcpy) {
        std::memcpy(dest, src, MM::packed_size);
    } else {
        for (std::size_t i = 0; i < MM::memcpy_region_count; ++i) {
            const auto& region = MM::memcpy_regions[i];
            std::memcpy(
                dest + region.dst_offset,
                src + region.src_offset,
                region.size
            );
        }
    }
    
    return MM::packed_size;
}

/// @brief Serialize to a span (with bounds checking)
/// Returns the number of bytes written, or 0 if buffer too small
template<typename T>
requires std::is_trivially_copyable_v<T>
inline std::size_t serialize_to(const T& value, std::span<std::byte> dest) {
    using MM = MemoryMap<T>;
    
    if (dest.size() < MM::packed_size) {
        return 0;  // Buffer too small
    }
    
    return serialize_to(value, dest.data());
}

// ============================================================================
// Deserialization
// ============================================================================

/// @brief Deserialize a trivially copyable struct from a byte span
/// Reconstructs the struct from packed data (inserting padding where needed)
/// 
/// **IMPORTANT**: Structs with fixed containers (fixed_vector/fixed_string) should
/// use the reflector API instead, as this path expects fixed packed size.
/// This is caught at compile-time via static_assert.
template<typename T>
requires std::is_trivially_copyable_v<T>
[[nodiscard]] inline std::optional<T> deserialize(std::span<const std::byte> data) {
    using MM = MemoryMap<T>;
    
    // Compile-time check: fail if struct contains fixed containers
    static_assert(!detail::struct_has_fixed_containers<T>(),
        "Structs with fixed_vector/fixed_string must use BinaryReflector API "
        "for proper runtime size handling. Use read_reflector<T>(reader) instead.");
    
    // Safe to use optimized memcpy - no variable-size fields
    if (data.size() < MM::packed_size) {
        return std::nullopt;  // Not enough data
    }
    
    T value{};  // Zero-initialize to clear padding bytes
    std::byte* dest = reinterpret_cast<std::byte*>(&value);
    
    if constexpr (MM::can_single_memcpy) {
        std::memcpy(dest, data.data(), MM::packed_size);
    } else {
        for (std::size_t i = 0; i < MM::memcpy_region_count; ++i) {
            const auto& region = MM::memcpy_regions[i];
            std::memcpy(
                dest + region.src_offset,     // Destination in struct
                data.data() + region.dst_offset,  // Source in packed buffer
                region.size
            );
        }
    }
    
    return value;
}

/// @brief Deserialize from a static_buffer
template<typename T, std::size_t N>
requires std::is_trivially_copyable_v<T>
[[nodiscard]] inline std::optional<T> deserialize(const static_buffer<N>& buffer) {
    return deserialize<T>(buffer.view());
}

/// @brief Deserialize directly into an existing object (avoids construction)
/// Returns true if successful
template<typename T>
requires std::is_trivially_copyable_v<T>
[[nodiscard]] inline bool deserialize_into(std::span<const std::byte> data, T& out) {
    using MM = MemoryMap<T>;
    
    if (data.size() < MM::packed_size) {
        return false;
    }
    
    std::byte* dest = reinterpret_cast<std::byte*>(&out);
    
    if constexpr (MM::can_single_memcpy) {
        std::memcpy(dest, data.data(), MM::packed_size);
    } else {
        for (std::size_t i = 0; i < MM::memcpy_region_count; ++i) {
            const auto& region = MM::memcpy_regions[i];
            std::memcpy(
                dest + region.src_offset,
                data.data() + region.dst_offset,
                region.size
            );
        }
    }
    
    return true;
}

/// @brief Deserialize from static_buffer into existing object
template<typename T, std::size_t N>
requires std::is_trivially_copyable_v<T>
[[nodiscard]] inline bool deserialize_into(const static_buffer<N>& buffer, T& out) {
    return deserialize_into(buffer.view(), out);
}

// ============================================================================
// Static Streaming Writer (for multiple messages in one buffer)
// ============================================================================

/// @brief Fixed-capacity streaming writer - ZERO ALLOCATION
/// @tparam Capacity Maximum buffer size in bytes
template<std::size_t Capacity>
class StaticWriter {
public:
    StaticWriter() = default;
    
    /// @brief Write a trivially copyable struct
    template<typename T>
    requires std::is_trivially_copyable_v<T>
    bool write(const T& value) {
        using MM = MemoryMap<T>;
        
        if (buffer_.remaining() < MM::packed_size) {
            return false;  // Would overflow
        }
        
        const std::size_t old_size = buffer_.size();
        buffer_.resize(old_size + MM::packed_size);
        
        serialize_to(value, buffer_.data() + old_size);
        return true;
    }
    
    /// @brief Write multiple values
    template<typename... Ts>
    bool write_all(const Ts&... values) {
        return (write(values) && ...);
    }
    
    /// @brief Get view of written data
    [[nodiscard]] std::span<const std::byte> view() const noexcept { 
        return buffer_.view(); 
    }
    
    /// @brief Get the buffer
    [[nodiscard]] const static_buffer<Capacity>& buffer() const noexcept { 
        return buffer_; 
    }
    
    /// @brief Current size
    [[nodiscard]] std::size_t size() const noexcept { return buffer_.size(); }
    
    /// @brief Remaining capacity
    [[nodiscard]] std::size_t remaining() const noexcept { return buffer_.remaining(); }
    
    /// @brief Clear the buffer
    void clear() noexcept { buffer_.clear(); }
    
private:
    static_buffer<Capacity> buffer_;
};

// ============================================================================
// Static Streaming Reader
// ============================================================================

/// @brief Streaming reader from any byte span - ZERO ALLOCATION
class StaticReader {
public:
    explicit StaticReader(std::span<const std::byte> data) 
        : data_(data), pos_(0) {}
    
    template<std::size_t N>
    explicit StaticReader(const static_buffer<N>& buffer)
        : data_(buffer.view()), pos_(0) {}
    
    /// @brief Read a trivially copyable struct
    template<typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] std::optional<T> read() {
        using MM = MemoryMap<T>;
        
        if (remaining() < MM::packed_size) {
            return std::nullopt;
        }
        
        T value{};
        std::byte* dest = reinterpret_cast<std::byte*>(&value);
        const std::byte* src = data_.data() + pos_;
        
        if constexpr (MM::can_single_memcpy) {
            std::memcpy(dest, src, MM::packed_size);
        } else {
            for (std::size_t i = 0; i < MM::memcpy_region_count; ++i) {
                const auto& region = MM::memcpy_regions[i];
                std::memcpy(
                    dest + region.src_offset,
                    src + region.dst_offset,
                    region.size
                );
            }
        }
        
        pos_ += MM::packed_size;
        return value;
    }
    
    /// @brief Read into existing object (avoids construction)
    template<typename T>
    requires std::is_trivially_copyable_v<T>
    bool read_into(T& out) {
        using MM = MemoryMap<T>;
        
        if (remaining() < MM::packed_size) {
            return false;
        }
        
        deserialize_into(std::span{data_.data() + pos_, MM::packed_size}, out);
        pos_ += MM::packed_size;
        return true;
    }
    
    /// @brief Bytes remaining
    [[nodiscard]] std::size_t remaining() const noexcept { return data_.size() - pos_; }
    
    /// @brief Current position
    [[nodiscard]] std::size_t position() const noexcept { return pos_; }
    
    /// @brief Skip bytes
    bool skip(std::size_t bytes) {
        if (remaining() < bytes) return false;
        pos_ += bytes;
        return true;
    }
    
    /// @brief Reset to beginning
    void reset() noexcept { pos_ = 0; }
    
private:
    std::span<const std::byte> data_;
    std::size_t pos_;
};

// ============================================================================
// Type Aliases for Common Writer Sizes
// ============================================================================

using StaticWriter64   = StaticWriter<64>;
using StaticWriter128  = StaticWriter<128>;
using StaticWriter256  = StaticWriter<256>;
using StaticWriter512  = StaticWriter<512>;
using StaticWriter1K   = StaticWriter<1024>;
using StaticWriter4K   = StaticWriter<4096>;
using StaticWriter64K  = StaticWriter<65536>;

// ============================================================================
// Compile-Time Statistics
// ============================================================================

/// @brief Get serialization stats at compile time
template<typename T>
struct SerializationStats {
    static constexpr std::size_t original_size = sizeof(T);
    static constexpr std::size_t packed_size = MemoryMap<T>::packed_size;
    static constexpr std::size_t padding_eliminated = original_size - packed_size;
    static constexpr std::size_t memcpy_calls = MemoryMap<T>::memcpy_region_count;
    static constexpr bool is_single_memcpy = MemoryMap<T>::can_single_memcpy;
    
    // Buffer type needed to serialize this type
    using buffer_type = static_buffer<packed_size>;
};

// ============================================================================
// Legacy Compatibility Aliases (deprecated)
// ============================================================================

template<typename T>
requires std::is_trivially_copyable_v<T>
[[nodiscard, deprecated("Use serialize() instead")]] 
inline auto serialize_static(const T& value) {
    return serialize(value);
}

template<typename T>
requires std::is_trivially_copyable_v<T>
[[nodiscard, deprecated("Use serialize() instead")]] 
inline auto serialize_optimized(const T& value) {
    return serialize(value);
}

template<typename T>
requires std::is_trivially_copyable_v<T>
[[nodiscard, deprecated("Use serialize_to() instead")]] 
inline std::size_t serialize_optimized_to(const T& value, std::byte* dest) {
    return serialize_to(value, dest);
}

template<typename T>
requires std::is_trivially_copyable_v<T>
[[nodiscard, deprecated("Use deserialize<T>() instead")]] 
inline std::optional<T> deserialize_optimized(std::span<const std::byte> data) {
    return deserialize<T>(data);
}

template<typename T, std::size_t N>
requires std::is_trivially_copyable_v<T>
[[nodiscard, deprecated("Use deserialize<T>() instead")]] 
inline std::optional<T> deserialize_optimized(const static_buffer<N>& buffer) {
    return deserialize<T>(buffer);
}

} // namespace sertial
