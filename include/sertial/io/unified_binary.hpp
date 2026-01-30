#pragma once

#include "../core/traits/hybrid_memory_map.hpp"
#include "../containers/static_buffer.hpp"
#include <cstring>
#include <span>
#include <optional>

namespace sertial {

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
                            if constexpr (detail::is_fixed_container_impl<FieldType>::value) {
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
