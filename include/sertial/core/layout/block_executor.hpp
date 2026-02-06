#pragma once

#include "block_types.hpp"
#include "../concepts.hpp"
#include "../../containers/container_registration.hpp"
#include <cstddef>
#include <span>
#include <cstring>

namespace sertial {
namespace detail {

// ============================================================================
// Block Executor - Symmetric Serialize/Deserialize Per Block Type
// ============================================================================
// This file co-locates serialization and deserialization logic for each block
// type, ensuring symmetry and making it easy to maintain consistency.
//
// Philosophy:
// - Each block type has both serialize() and deserialize() methods together
// - No raw pointers - only std::span for type safety
// - No temporary buffers - operate directly on memory via spans/casts
// - Compile-time size validation where possible
//
// NOTE: Block type definitions (FixedBlock, DynamicBlock, etc.) are now in
// block_types.hpp to avoid duplication with hybrid_memory_map.hpp
// ============================================================================

// ============================================================================
// FixedBlockExecutor - Execution logic for FixedBlock
// ============================================================================

struct FixedBlockExecutor {
    
    // ------------------------------------------------------------------------
    // Serialization
    // ------------------------------------------------------------------------
    
    /// @brief Serialize fixed block - direct memcpy from struct to buffer
    /// @param src_struct Span viewing the source struct memory
    /// @param dest_buffer Span viewing the destination buffer
    /// @return Number of bytes written
    static std::size_t serialize(std::span<const std::byte> src_struct,
                                 std::span<std::byte> dest_buffer,
                                 const FixedBlock& block) {
        // Direct memcpy - no intermediate buffers
        std::memcpy(dest_buffer.data() + block.dst_offset,
                   src_struct.data() + block.src_offset,
                   block.size);
        return block.size;
    }
    
    // ------------------------------------------------------------------------
    // Deserialization
    // ------------------------------------------------------------------------
    
    /// @brief Deserialize fixed block - direct memcpy from buffer to struct
    /// @param src_buffer Span viewing the source serialized data
    /// @param dest_struct Span viewing the destination struct memory
    static void deserialize(std::span<const std::byte> src_buffer,
                           std::span<std::byte> dest_struct,
                           const FixedBlock& block) {
        // Direct memcpy - no intermediate buffers
        std::memcpy(dest_struct.data() + block.src_offset,
                   src_buffer.data() + block.dst_offset,
                   block.size);
    }
};

// ============================================================================
// PaddingBlockExecutor - Execution logic for PaddingBlock (no-op)
// ============================================================================

struct PaddingBlockExecutor {
    // Padding blocks are not serialized - these methods exist for completeness
    
    static std::size_t serialize(std::span<const std::byte>, std::span<std::byte>,
                                const PaddingBlock&) {
        return 0;  // Skip padding
    }
    
    static void deserialize(std::span<const std::byte>, std::span<std::byte>,
                           const PaddingBlock&) {
        // Skip padding
    }
};

// ============================================================================
// DynamicBlockExecutor - Execution logic for DynamicBlock
// ============================================================================

struct DynamicBlockExecutor {
    
    // ------------------------------------------------------------------------
    // Serialization
    // ------------------------------------------------------------------------
    
    /// @brief Serialize dynamic block - length prefix + container data
    /// @tparam ContainerT Container type (fixed_vector, fixed_string, RingBuffer, etc.)
    /// @param container The container to serialize
    /// @param dest_buffer Destination buffer span
    /// @param current_offset Current write position (updated)
    /// @return Number of bytes written
    template<typename ContainerT>
        requires SerializableContainer<ContainerT>
    static std::size_t serialize(const ContainerT& container,
                                std::span<std::byte> dest_buffer,
                                std::size_t& current_offset,
                                const DynamicBlock& block) {
        std::size_t start_offset = current_offset;
        
        // Write length prefix
        uint32_t length = static_cast<uint32_t>(container.size());
        std::memcpy(dest_buffer.data() + current_offset, &length, sizeof(uint32_t));
        current_offset += sizeof(uint32_t);
        
        // Write container data (may be 1-2 spans for circular buffers)
        if (length > 0) {
            auto spans = get_serialization_spans(container);
            for (const auto& span : spans) {
                if (span.empty()) continue;
                std::size_t span_bytes = span.size() * block.element_size;
                std::memcpy(dest_buffer.data() + current_offset,
                           reinterpret_cast<const std::byte*>(span.data()),
                           span_bytes);
                current_offset += span_bytes;
            }
        }
        
        return current_offset - start_offset;
    }
    
    // ------------------------------------------------------------------------
    // Deserialization
    // ------------------------------------------------------------------------
    
    /// @brief Deserialize dynamic block - read length prefix + data
    /// @tparam ContainerT Container type (fixed_vector, fixed_string, RingBuffer, etc.)
    /// @param src_buffer Source buffer span
    /// @param container The container to fill
    /// @param current_offset Current read position (updated)
    template<typename ContainerT>
        requires SerializableContainer<ContainerT>
    static void deserialize(std::span<const std::byte> src_buffer,
                           ContainerT& container,
                           std::size_t& current_offset,
                           const DynamicBlock& block) {
        // Read length prefix
        uint32_t length;
        std::memcpy(&length, src_buffer.data() + current_offset, sizeof(uint32_t));
        current_offset += sizeof(uint32_t);
        
        // Validate length against capacity
        if (length > block.capacity) {
            // Handle error - length exceeds container capacity
            return;
        }
        
        // Set container size and copy data directly
        container.set_size_unsafe(length);
        if (length > 0) {
            std::size_t data_bytes = length * block.element_size;
            std::memcpy(container.data_unsafe(),
                       src_buffer.data() + current_offset,
                       data_bytes);
            current_offset += data_bytes;
        }
    }
};

// ============================================================================
// RuntimeOffsetBlockExecutor - Execution logic for RuntimeOffsetBlock
// ============================================================================

struct RuntimeOffsetBlockExecutor {
    
    // ------------------------------------------------------------------------
    // Serialization
    // ------------------------------------------------------------------------
    
    /// @brief Serialize runtime-offset block
    /// @param src_struct Source struct memory span
    /// @param dest_buffer Destination buffer span
    /// @param current_offset Current write position (runtime-computed)
    /// @return Number of bytes written
    static std::size_t serialize(std::span<const std::byte> src_struct,
                                std::span<std::byte> dest_buffer,
                                std::size_t current_offset,
                                const RuntimeOffsetBlock& block) {
        // Offset is computed at runtime based on preceding dynamic blocks
        std::memcpy(dest_buffer.data() + current_offset,
                   src_struct.data() + block.src_offset,
                   block.size);
        return block.size;
    }
    
    // ------------------------------------------------------------------------
    // Deserialization
    // ------------------------------------------------------------------------
    
    /// @brief Deserialize runtime-offset block
    /// @param src_buffer Source buffer span
    /// @param dest_struct Destination struct memory span
    /// @param current_offset Current read position (runtime-computed)
    static void deserialize(std::span<const std::byte> src_buffer,
                           std::span<std::byte> dest_struct,
                           std::size_t current_offset,
                           const RuntimeOffsetBlock& block) {
        // Offset is computed at runtime based on preceding dynamic blocks
        std::memcpy(dest_struct.data() + block.src_offset,
                   src_buffer.data() + current_offset,
                   block.size);
    }
};

} // namespace detail
} // namespace sertial

