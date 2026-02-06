#pragma once

#include "../core/traits/hybrid_memory_map.hpp"
#include "../containers/static_buffer.hpp"
#include "../containers/ring_buffer.hpp"
#include <cstring>
#include <span>
#include <optional>

namespace sertial {

// ============================================================================
// RingBuffer Serialization Helpers
// ============================================================================

namespace detail {

/// @brief Serialize RingBuffer with wrap-around handling
/// @return Bytes written (length prefix + data)
template<typename T, std::size_t N>
inline std::size_t serialize_ring_buffer(const RingBuffer<T, N>& buf, std::byte* dest) {
    // Write length prefix
    uint32_t length = static_cast<uint32_t>(buf.size());
    std::memcpy(dest, &length, sizeof(uint32_t));
    
    if (length == 0) {
        return sizeof(uint32_t);
    }
    
    std::byte* data_dest = dest + sizeof(uint32_t);
    constexpr std::size_t elem_size = sizeof(T);
    
    // Check if data wraps around
    if (buf.is_wrapped()) {
        // Two-region copy: tail → end, then start → head
        std::size_t first_chunk = buf.capacity() - buf.tail_index();
        
        // Copy tail → end of buffer (oldest data)
        std::memcpy(data_dest, 
                   buf.data_unsafe() + buf.tail_index(), 
                   first_chunk * elem_size);
        
        // Copy start → head (newest data)
        std::size_t second_chunk = length - first_chunk;
        std::memcpy(data_dest + first_chunk * elem_size,
                   buf.data_unsafe(),
                   second_chunk * elem_size);
    } else {
        // Single contiguous region: tail → head
        std::memcpy(data_dest,
                   buf.data_unsafe() + buf.tail_index(),
                   length * elem_size);
    }
    
    return sizeof(uint32_t) + length * elem_size;
}

/// @brief Deserialize RingBuffer (always produces non-wrapped state)
/// @return true on success, false on error
template<typename T, std::size_t N>
inline bool deserialize_ring_buffer(RingBuffer<T, N>& buf, const std::byte* src, std::size_t available) {
    if (available < sizeof(uint32_t)) {
        return false;
    }
    
    uint32_t length;
    std::memcpy(&length, src, sizeof(uint32_t));
    
    if (length > N) {
        return false;  // Exceeds capacity
    }
    
    const std::byte* data_src = src + sizeof(uint32_t);
    std::size_t data_size = length * sizeof(T);
    
    if (available < sizeof(uint32_t) + data_size) {
        return false;
    }
    
    // Deserialize to non-wrapped state (head=length, tail=0)
    if (data_size > 0) {
        std::memcpy(buf.data_unsafe(), data_src, data_size);
    }
    buf.set_size_unsafe(length);
    
    return true;
}

} // namespace detail

// ============================================================================
// Unified Binary Serialization using HybridMemoryMap
// ============================================================================
// Single unified approach for both fixed and variable-size types
// Uses block-based serialization for optimal performance
// - ALL types use static_buffer with compile-time max_packed_size
// - Zero heap allocations for all serialization operations
// ============================================================================

/// @brief Serialize to raw pointer (returns bytes written)
template<typename T>
inline std::size_t serialize_to_unified(const T& value, std::byte* dest) {
    using HMM = HybridMemoryMap<T>;
    
    if constexpr (detail::has_named_tuple_t_v<T>) {
        if constexpr (!HMM::has_variable_fields) {
            // Pure fixed - direct memcpy using block info
            const auto* src = reinterpret_cast<const std::byte*>(&value);
            
            for (std::size_t i = 0; i < HMM::fixed_block_count; ++i) {
                const auto& block = HMM::fixed_blocks[i];
                std::memcpy(dest + block.dst_offset, src + block.src_offset, block.size);
            }
            
            return HMM::base_packed_size;
        } else {
        // Variable fields - execute blocks in order
        const auto* src = reinterpret_cast<const std::byte*>(&value);
        auto nt = rfl::to_named_tuple(value);
        std::size_t current_offset = 0;
        
        // Execute blocks in optimal order
        for (std::size_t i = 0; i < HMM::total_blocks; ++i) {
            const auto& descriptor = HMM::execution_order[i];
            
            switch (descriptor.type) {
                case detail::BlockType::Fixed: {
                    const auto& block = HMM::fixed_blocks[descriptor.index];
                    std::memcpy(dest + block.dst_offset, src + block.src_offset, block.size);
                    current_offset = block.dst_offset + block.size;
                    break;
                }
                
                case detail::BlockType::Padding:
                    // Skip padding - not serialized
                    break;
                
                case detail::BlockType::Dynamic: {
                    const auto& block = HMM::dynamic_blocks[descriptor.index];
                    
                    // Serialize variable field with length prefix
                    detail::visit_field_by_index(nt, block.field_index,
                        [&](const auto& field) {
                            using FieldType = std::decay_t<decltype(field)>;
                            
                            // Check if this is a RingBuffer (special handling)
                            if constexpr (detail::is_ring_buffer_v<FieldType>) {
                                std::size_t bytes_written = detail::serialize_ring_buffer(field, dest + current_offset);
                                current_offset += bytes_written;
                            }
                            // Regular fixed-capacity containers
                            else if constexpr (detail::is_fixed_container_impl<FieldType>::value) {
                                // Write length prefix (uint32_t)
                                uint32_t length = static_cast<uint32_t>(field.size());
                                std::memcpy(dest + current_offset, &length, sizeof(uint32_t));
                                current_offset += sizeof(uint32_t);
                                
                                // Write data
                                std::size_t data_size = field.size() * sizeof(typename FieldType::value_type);
                                if (data_size > 0) {
                                    std::memcpy(dest + current_offset, field.data(), data_size);
                                    current_offset += data_size;
                                }
                            }
                            return 0; // dummy return
                        });
                    break;
                }
                
                case detail::BlockType::RuntimeOffset: {
                    const auto& block = HMM::runtime_offset_blocks[descriptor.index];
                    std::memcpy(dest + current_offset, src + block.src_offset, block.size);
                    current_offset += block.size;
                    break;
                }
            }
        }
        
        return current_offset;
        }
    } else {
        // Fallback for non-reflectable types
        std::memcpy(dest, &value, sizeof(T));
        return sizeof(T);
    }
}

/// @brief Serialize to a static buffer (zero allocation)
template<typename T>
[[nodiscard]] inline auto serialize_unified(const T& value) {
    using HMM = HybridMemoryMap<T>;
    
    // For non-reflectable types, use sizeof(T) as buffer size
    constexpr std::size_t max_size = []() constexpr {
        if constexpr (detail::has_named_tuple_t_v<T>) {
            return HMM::max_packed_size;
        } else {
            return sizeof(T);
        }
    }();
    
    static_buffer<max_size> buffer;
    const std::size_t actual_size = serialize_to_unified(value, buffer.data());
    buffer.resize(actual_size);
    
    return buffer;
}

/// @brief Deserialize from buffer
template<typename T>
[[nodiscard]] inline T deserialize_unified(const std::byte* data, std::size_t size) {
    return HybridMemoryMap<T>::deserialize(data, size);
}

/// @brief Deserialize from static_buffer
template<typename T, std::size_t N>
[[nodiscard]] inline T deserialize_unified(const static_buffer<N>& buffer) {
    return HybridMemoryMap<T>::deserialize(buffer.data(), buffer.size());
}

/// @brief Get the packed size for a value
template<typename T>
constexpr std::size_t packed_size_of(const T& value) {
    return HybridMemoryMap<T>::calculate_packed_size(value);
}

// ============================================================================
// Convenience Functions (Backward-Compatible API)
// ============================================================================

// No wrapper needed - serialize() returns static_buffer directly
// static_buffer already has .size(), .view(), .data() methods

/// @brief Serialize to a static buffer (convenience wrapper)
template<typename T>
[[nodiscard]] inline auto serialize(const T& value) {
    return serialize_unified(value);
}

/// @brief Serialize to raw pointer (convenience wrapper)
template<typename T>
inline std::size_t serialize_to(const T& value, std::byte* dest) {
    return serialize_to_unified(value, dest);
}

/// @brief Serialize to static_buffer (convenience wrapper)
template<typename T, std::size_t N>
inline std::size_t serialize_to(const T& value, static_buffer<N>& buffer) {
    const std::size_t size = serialize_to_unified(value, buffer.data());
    buffer.resize(size);
    return size;
}

/// @brief Deserialize from buffer (convenience wrapper, returns optional for compatibility)
template<typename T>
[[nodiscard]] inline std::optional<T> deserialize(std::span<const std::byte> data) {
    try {
        return deserialize_unified<T>(data.data(), data.size());
    } catch (...) {
        return std::nullopt;
    }
}

/// @brief Deserialize from static_buffer (convenience wrapper)
template<typename T, std::size_t N>
[[nodiscard]] inline std::optional<T> deserialize(const static_buffer<N>& buffer) {
    try {
        return deserialize_unified<T>(buffer);
    } catch (...) {
        return std::nullopt;
    }
}

/// @brief Deserialize into an existing object
template<typename T>
[[nodiscard]] inline bool deserialize_into(std::span<const std::byte> data, T& out) {
    try {
        out = deserialize_unified<T>(data.data(), data.size());
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace sertial
