#pragma once

#include <cstddef>
#include <cstdint>

namespace sertial {
namespace detail {

// ============================================================================
// Block Type Definitions - Single Source of Truth
// ============================================================================
// These types are used throughout the serialization system:
// - StructLayout uses them for compile-time metadata
// - BlockExecutor uses them for runtime execution
// - HybridMemoryMap uses them for schema generation
//
// Previously duplicated across hybrid_memory_map.hpp and block_executor.hpp
// Now consolidated here as the single authoritative definition.
// ============================================================================

/// @brief Block type enumeration for execution order
enum class BlockType : uint8_t {
    Fixed,          ///< Fixed-size memcpy with compile-time offset
    Padding,        ///< Padding in struct (skipped in packed format)
    Dynamic,        ///< Variable-size field (runtime size)
    RuntimeOffset   ///< Fixed-size memcpy with runtime offset
};

// ============================================================================
// FixedBlock - Contiguous fixed-size fields before any dynamic content
// ============================================================================

/// @brief Describes a consecutive region of fixed-size fields
/// 
/// Fixed blocks are serialized with a single memcpy operation at a
/// compile-time-known offset. These occur before any dynamic fields.
struct FixedBlock {
    std::size_t src_offset;      ///< Offset in struct
    std::size_t dst_offset;      ///< Offset in packed buffer (compile-time known)
    std::size_t size;            ///< Total bytes to memcpy
    std::size_t field_start;     ///< First field index in this block
    std::size_t field_count;     ///< Number of consecutive fields
    
    constexpr FixedBlock() = default;
    
    constexpr FixedBlock(std::size_t src, std::size_t dst, std::size_t sz, 
                         std::size_t fs = 0, std::size_t fc = 0)
        : src_offset(src), dst_offset(dst), size(sz), 
          field_start(fs), field_count(fc) {}
};

// ============================================================================
// PaddingBlock - Alignment gaps (not serialized)
// ============================================================================

/// @brief Describes padding bytes in the struct (removed in packed format)
/// 
/// Padding blocks are skipped during serialization, allowing us to pack
/// structs tighter in the wire format than they are in memory.
struct PaddingBlock {
    std::size_t src_offset;      ///< Where padding starts in struct
    std::size_t size;            ///< Padding bytes to skip
    
    constexpr PaddingBlock() = default;
    
    constexpr PaddingBlock(std::size_t src, std::size_t sz)
        : src_offset(src), size(sz) {}
};

// ============================================================================
// DynamicBlock - Variable-size containers (fixed_vector, fixed_string, RingBuffer)
// ============================================================================

/// @brief Describes a variable-size field that needs runtime evaluation
/// 
/// Dynamic blocks serialize as: [4-byte length prefix][actual data]
/// The actual size is determined at runtime from container.size().
struct DynamicBlock {
    std::size_t field_index;           ///< Which field in the struct
    std::size_t src_offset;            ///< Offset of container in struct
    std::size_t base_dst_offset;       ///< Where to start in packed buffer
    std::size_t element_size;          ///< sizeof(T) for container<T>
    std::size_t capacity;              ///< Max elements (for fixed containers)
    bool needs_length_prefix;          ///< Serialize size() first? (usually true)
    
    constexpr DynamicBlock() = default;
    
    constexpr DynamicBlock(std::size_t fidx, std::size_t src, std::size_t dst,
                          std::size_t elem_sz, std::size_t cap, bool prefix = true)
        : field_index(fidx), src_offset(src), base_dst_offset(dst),
          element_size(elem_sz), capacity(cap), needs_length_prefix(prefix) {}
};

// ============================================================================
// RuntimeOffsetBlock - Fixed fields after dynamic content
// ============================================================================

/// @brief Describes fixed fields after dynamic fields (runtime offset needed)
/// 
/// These fields have fixed size but their offset in the packed buffer depends
/// on the runtime size of preceding dynamic fields. They require a two-pass
/// approach: calculate offset, then memcpy.
struct RuntimeOffsetBlock {
    std::size_t src_offset;      ///< Offset in struct
    std::size_t size;            ///< Bytes to memcpy
    std::size_t field_start;     ///< First field index
    std::size_t field_count;     ///< Number of fields
    
    constexpr RuntimeOffsetBlock() = default;
    
    constexpr RuntimeOffsetBlock(std::size_t src, std::size_t sz,
                                 std::size_t fs = 0, std::size_t fc = 0)
        : src_offset(src), size(sz), field_start(fs), field_count(fc) {}
};

// ============================================================================
// BlockDescriptor - Execution Order
// ============================================================================

/// @brief Descriptor for execution order (which block to execute next)
/// 
/// The execution_order array in StructLayout contains these descriptors,
/// telling the block executor which block to process next and where to
/// find it in the respective block array.
struct BlockDescriptor {
    BlockType type;              ///< Which type of block
    uint16_t index;              ///< Index into respective block array
    
    constexpr BlockDescriptor() : type(BlockType::Fixed), index(0) {}
    
    constexpr BlockDescriptor(BlockType t, uint16_t idx) 
        : type(t), index(idx) {}
};

} // namespace detail
} // namespace sertial
