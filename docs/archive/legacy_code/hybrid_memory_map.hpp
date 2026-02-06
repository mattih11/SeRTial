#pragma once

/// @file hybrid_memory_map.hpp
/// @brief Schema generation support (LEGACY - used for JSON export only)
///
/// This file provides schema generation functions for JSON export to Python
/// visualization tools. It is NOT used in runtime serialization hot paths.
///
/// Runtime serialization uses StructLayout<T> directly. This file exists to
/// support get_hybrid_schema<T>() which generates TypeSchema objects for
/// JSON export.
///
/// Future: Could be replaced by reading from StructLayout constexpr metadata
/// and generating TypeSchema at runtime for JSON export.
///
/// Architecture:
///   SchemaGenerator → hybrid_memory_map.hpp → memory_map.hpp → JSON export
///   (Parallel to runtime serialization path)

#include "memory_map.hpp"
#include "../layout/block_types.hpp"
#include "../../traits/container_detection.hpp"
#include "../../containers/container_registration.hpp"
#include <cstddef>
#include <array>

namespace sertial {

// ============================================================================
// Hybrid Memory Map - Schema Generation Support (Legacy)
// ============================================================================
// Analyzes struct layout to create optimal serialization plan:
// - FixedBlock: Consecutive fixed fields → single memcpy
// - PaddingBlock: Struct padding to skip (not in packed format)
// - DynamicBlock: Variable-size fields (runtime size evaluation)
// - RuntimeOffsetBlock: Fixed fields after dynamic (runtime offset)
//
// Block type definitions are now in layout/block_types.hpp (single source of truth).
//
// TODO: Currently HybridMemoryMap preserves alignment padding in nested structs,
//       resulting in slightly larger output than MemoryMap's aggressive packing.
//       Example: Header<Timestamp> produces 28 bytes instead of 24 bytes.
//       This is a known limitation of the block-based approach.
//       Future optimization: Implement padding removal similar to MemoryMap.
// ============================================================================

namespace detail {

// ============================================================================
// Runtime Field Access Helpers
// ============================================================================

/// @brief Get size of a variable-length container in bytes
template<typename T>
std::size_t get_container_byte_size(const T& container) {
    if constexpr (is_fixed_container_impl<T>::value) {
        return container.size() * sizeof(typename T::value_type);
    } else {
        return 0;
    }
}

/// @brief Visit a field by index at runtime (using fold expression dispatch)
template<typename Visitor, typename... Fields>
constexpr auto visit_field_by_index(
    const rfl::NamedTuple<Fields...>& nt,
    std::size_t index,
    Visitor&& visitor) -> decltype(visitor(rfl::get<0>(nt)))
{
    using ReturnType = decltype(visitor(rfl::get<0>(nt)));
    ReturnType result{};
    
    // Compile-time fold over all indices
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((index == Is ? (result = visitor(rfl::get<Is>(nt)), true) : false) || ...);
    }(std::make_index_sequence<sizeof...(Fields)>{});
    
    return result;
}

/// @brief Non-const version for mutable access
template<typename Visitor, typename... Fields>
constexpr auto visit_field_by_index(
    rfl::NamedTuple<Fields...>& nt,
    std::size_t index,
    Visitor&& visitor) -> decltype(visitor(rfl::get<0>(nt)))
{
    using ReturnType = decltype(visitor(rfl::get<0>(nt)));
    ReturnType result{};
    
    // Compile-time fold over all indices
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((index == Is ? (result = visitor(rfl::get<Is>(nt)), true) : false) || ...);
    }(std::make_index_sequence<sizeof...(Fields)>{});
    
    return result;
}

// Block type definitions (FixedBlock, PaddingBlock, DynamicBlock, etc.) are
// now in layout/block_types.hpp to eliminate duplication with block_executor.hpp

// Block type definitions (FixedBlock, PaddingBlock, DynamicBlock, etc.) are
// now in layout/block_types.hpp to eliminate duplication with block_executor.hpp

} // namespace detail

/// @brief Build blocks at compile-time by analyzing field layout
template<typename T>
struct HybridLayoutBuilder {
    using MM = MemoryMap<T>;
    
    // Helper to get field count
    static constexpr std::size_t get_field_count() {
        if constexpr (detail::has_named_tuple_t_v<T>) {
            using NT = rfl::named_tuple_t<T>;
            return []<typename... Fields>(rfl::NamedTuple<Fields...>*) {
                return sizeof...(Fields);
            }(static_cast<NT*>(nullptr));
        }
        return 0;
    }
    
    static constexpr std::size_t num_fields = get_field_count();
    
    // Check if field is variable-size
    template<typename... Fields>
    static constexpr auto get_field_variable_flags(rfl::NamedTuple<Fields...>*) {
        std::array<bool, sizeof...(Fields)> flags{};
        std::size_t idx = 0;
        ([&] {
            flags[idx++] = detail::is_fixed_container_v<detail::field_type_t<Fields>>;
        }(), ...);
        return flags;
    }
    
    // Get element sizes for variable fields
    template<typename... Fields>
    static constexpr auto get_field_element_sizes(rfl::NamedTuple<Fields...>*) {
        std::array<std::size_t, sizeof...(Fields)> sizes{};
        std::size_t idx = 0;
        ([&] {
            using FieldType = detail::field_type_t<Fields>;
            sizes[idx++] = detail::is_fixed_container_v<FieldType> 
                ? detail::fixed_container_element_size_v<FieldType> 
                : 0;
        }(), ...);
        return sizes;
    }
    
    // Get capacities for variable fields
    template<typename... Fields>
    static constexpr auto get_field_capacities(rfl::NamedTuple<Fields...>*) {
        std::array<std::size_t, sizeof...(Fields)> caps{};
        std::size_t idx = 0;
        ([&] {
            using FieldType = detail::field_type_t<Fields>;
            caps[idx++] = detail::is_fixed_container_v<FieldType> 
                ? detail::fixed_container_capacity_v<FieldType> 
                : 0;
        }(), ...);
        return caps;
    }
    
    static constexpr auto field_is_variable = []() {
        if constexpr (num_fields == 0) {
            return std::array<bool, 1>{};
        } else {
            using NT = rfl::named_tuple_t<T>;
            return get_field_variable_flags(static_cast<NT*>(nullptr));
        }
    }();
    
    static constexpr auto elem_sizes = []() {
        if constexpr (num_fields == 0) {
            return std::array<std::size_t, 1>{};
        } else {
            using NT = rfl::named_tuple_t<T>;
            return get_field_element_sizes(static_cast<NT*>(nullptr));
        }
    }();
    
    static constexpr auto capacities = []() {
        if constexpr (num_fields == 0) {
            return std::array<std::size_t, 1>{};
        } else {
            using NT = rfl::named_tuple_t<T>;
            return get_field_capacities(static_cast<NT*>(nullptr));
        }
    }();
    
    // Build block structure at compile-time
    struct BlockLayout {
        std::array<detail::FixedBlock, num_fields + 1> fixed_blocks{};
        std::array<detail::PaddingBlock, num_fields + 1> padding_blocks{};
        std::array<detail::DynamicBlock, num_fields + 1> dynamic_blocks{};
        std::array<detail::RuntimeOffsetBlock, num_fields + 1> runtime_offset_blocks{};
        std::array<detail::BlockDescriptor, (num_fields + 1) * 3> execution_order{};
        
        std::size_t fixed_count = 0;
        std::size_t padding_count = 0;
        std::size_t dynamic_count = 0;
        std::size_t runtime_offset_count = 0;
        std::size_t execution_count = 0;
        std::size_t base_packed_size = 0;
    };
    
    static constexpr BlockLayout build_blocks() {
        BlockLayout layout{};
        
        if constexpr (num_fields == 0) {
            return layout;
        }
        
        std::size_t current_dst_offset = 0;
        std::size_t region_start_src = 0;
        std::size_t region_start_dst = 0;
        std::size_t region_size = 0;
        std::size_t region_field_start = 0;
        std::size_t region_field_count = 0;
        bool in_fixed_region = false;
        bool past_dynamic_field = false;
        
        for (std::size_t i = 0; i < num_fields; ++i) {
            const std::size_t struct_offset = MM::field_offsets[i];
            const std::size_t field_size = MM::field_sizes[i];
            const bool is_variable = field_is_variable[i];
            
            if (is_variable) {
                // Close any ongoing fixed region
                if (in_fixed_region && region_size > 0) {
                    if (!past_dynamic_field) {
                        layout.fixed_blocks[layout.fixed_count] = 
                            detail::FixedBlock(region_start_src, region_start_dst, region_size,
                                             region_field_start, region_field_count);
                        layout.execution_order[layout.execution_count++] = 
                            detail::BlockDescriptor(detail::BlockType::Fixed, layout.fixed_count);
                        layout.fixed_count++;
                        current_dst_offset += region_size;
                    } else {
                        layout.runtime_offset_blocks[layout.runtime_offset_count] = 
                            detail::RuntimeOffsetBlock(region_start_src, region_size,
                                                      region_field_start, region_field_count);
                        layout.execution_order[layout.execution_count++] = 
                            detail::BlockDescriptor(detail::BlockType::RuntimeOffset, 
                                                  layout.runtime_offset_count);
                        layout.runtime_offset_count++;
                    }
                }
                
                // Add dynamic block - extract capacity and element size from field
                // We use field metadata arrays which are compile-time computed
                std::size_t elem_size = elem_sizes[i];
                std::size_t capacity = capacities[i];
                
                layout.dynamic_blocks[layout.dynamic_count] = 
                    detail::DynamicBlock(i, struct_offset, current_dst_offset,
                                       elem_size, capacity, true);
                layout.execution_order[layout.execution_count++] = 
                    detail::BlockDescriptor(detail::BlockType::Dynamic, layout.dynamic_count);
                layout.dynamic_count++;
                
                past_dynamic_field = true;
                in_fixed_region = false;
                region_size = 0;
                region_field_count = 0;
                
            } else {
                // Fixed-size field
                if (!in_fixed_region) {
                    // Start new fixed region
                    region_start_src = struct_offset;
                    region_start_dst = current_dst_offset;
                    region_size = field_size;
                    region_field_start = i;
                    region_field_count = 1;
                    in_fixed_region = true;
                } else {
                    // Check if consecutive (detect padding)
                    std::size_t expected_offset = region_start_src + region_size;
                    if (struct_offset == expected_offset) {
                        // Consecutive - extend region
                        region_size += field_size;
                        region_field_count++;
                    } else {
                        // Gap detected - close current region, add padding, start new region
                        
                        // Close current fixed region
                        if (!past_dynamic_field) {
                            layout.fixed_blocks[layout.fixed_count] = 
                                detail::FixedBlock(region_start_src, region_start_dst, region_size,
                                                 region_field_start, region_field_count);
                            layout.execution_order[layout.execution_count++] = 
                                detail::BlockDescriptor(detail::BlockType::Fixed, layout.fixed_count);
                            layout.fixed_count++;
                            current_dst_offset += region_size;
                        } else {
                            layout.runtime_offset_blocks[layout.runtime_offset_count] = 
                                detail::RuntimeOffsetBlock(region_start_src, region_size,
                                                          region_field_start, region_field_count);
                            layout.execution_order[layout.execution_count++] = 
                                detail::BlockDescriptor(detail::BlockType::RuntimeOffset,
                                                      layout.runtime_offset_count);
                            layout.runtime_offset_count++;
                        }
                        
                        // Add padding block
                        std::size_t padding_size = struct_offset - expected_offset;
                        layout.padding_blocks[layout.padding_count] = 
                            detail::PaddingBlock(expected_offset, padding_size);
                        layout.execution_order[layout.execution_count++] = 
                            detail::BlockDescriptor(detail::BlockType::Padding, layout.padding_count);
                        layout.padding_count++;
                        
                        // Start new fixed region
                        region_start_src = struct_offset;
                        region_start_dst = current_dst_offset;
                        region_size = field_size;
                        region_field_start = i;
                        region_field_count = 1;
                    }
                }
            }
        }
        
        // Close final fixed region if any
        if (in_fixed_region && region_size > 0) {
            if (!past_dynamic_field) {
                layout.fixed_blocks[layout.fixed_count] = 
                    detail::FixedBlock(region_start_src, region_start_dst, region_size,
                                     region_field_start, region_field_count);
                layout.execution_order[layout.execution_count++] = 
                    detail::BlockDescriptor(detail::BlockType::Fixed, layout.fixed_count);
                layout.fixed_count++;
                current_dst_offset += region_size;
            } else {
                layout.runtime_offset_blocks[layout.runtime_offset_count] = 
                    detail::RuntimeOffsetBlock(region_start_src, region_size,
                                              region_field_start, region_field_count);
                layout.execution_order[layout.execution_count++] = 
                    detail::BlockDescriptor(detail::BlockType::RuntimeOffset,
                                          layout.runtime_offset_count);
                layout.runtime_offset_count++;
            }
        }
        
        layout.base_packed_size = current_dst_offset;
        return layout;
    }
    
    static constexpr BlockLayout layout = build_blocks();
};

/// @brief Hybrid Memory Map - combines compile-time fixed regions with runtime variable fields
template<typename T>
struct HybridMemoryMap {
    using MM = MemoryMap<T>;
    using Builder = HybridLayoutBuilder<T>;
    
    // Reuse MemoryMap's compile-time analysis
    static constexpr bool can_single_memcpy = MM::can_single_memcpy;
    
    // Check if struct has variable fields
    static constexpr bool has_variable_fields = detail::struct_has_fixed_containers<T>();
    
    // Block counts (computed at compile-time)
    static constexpr std::size_t fixed_block_count = Builder::layout.fixed_count;
    static constexpr std::size_t padding_block_count = Builder::layout.padding_count;
    static constexpr std::size_t dynamic_block_count = Builder::layout.dynamic_count;
    static constexpr std::size_t runtime_offset_block_count = Builder::layout.runtime_offset_count;
    
    // Aliases for compatibility with tests
    static constexpr std::size_t variable_field_count = dynamic_block_count;
    static constexpr std::size_t copy_region_count = fixed_block_count + runtime_offset_block_count;
    
    // Total execution steps
    static constexpr std::size_t total_blocks = Builder::layout.execution_count;
    
    // Base packed size (without dynamic content)
    static constexpr std::size_t base_packed_size = Builder::layout.base_packed_size;
    
    // Maximum packed size (with all dynamic fields at max capacity)
    static constexpr std::size_t max_packed_size = []() constexpr {
        std::size_t size = base_packed_size;
        // Add maximum size for each dynamic block
        for (std::size_t i = 0; i < dynamic_block_count; ++i) {
            const auto& block = Builder::layout.dynamic_blocks[i];
            // Length prefix + max capacity
            size += sizeof(uint32_t) + (block.capacity * block.element_size);
        }
        // Add runtime offset blocks (fixed-size fields after dynamic content)
        for (std::size_t i = 0; i < runtime_offset_block_count; ++i) {
            const auto& block = Builder::layout.runtime_offset_blocks[i];
            size += block.size;
        }
        return size;
    }();
    
    // Block arrays (compile-time)
    static constexpr auto fixed_blocks = Builder::layout.fixed_blocks;
    static constexpr auto padding_blocks = Builder::layout.padding_blocks;
    static constexpr auto dynamic_blocks = Builder::layout.dynamic_blocks;
    static constexpr auto runtime_offset_blocks = Builder::layout.runtime_offset_blocks;
    
    // Execution order
    static constexpr auto execution_order = Builder::layout.execution_order;
    
    // Legacy compatibility
    static constexpr auto copy_regions = MM::memcpy_regions;
    
    /// @brief Get block information for schema export
    static std::vector<BlockInfo> get_block_info() {
        std::vector<BlockInfo> blocks;
        blocks.reserve(total_blocks);
        
        for (std::size_t i = 0; i < total_blocks; ++i) {
            const auto& descriptor = execution_order[i];
            BlockInfo info;
            
            switch (descriptor.type) {
                case detail::BlockType::Fixed: {
                    const auto& block = fixed_blocks[descriptor.index];
                    info.type = "Fixed";
                    info.src_offset = block.src_offset;
                    info.dst_offset = block.dst_offset;
                    info.size = block.size;
                    info.field_start = block.field_start;
                    info.field_count = block.field_count;
                    info.is_variable = false;
                    break;
                }
                case detail::BlockType::Padding: {
                    const auto& block = padding_blocks[descriptor.index];
                    info.type = "Padding";
                    info.src_offset = block.src_offset;
                    info.size = block.size;
                    info.is_variable = false;
                    break;
                }
                case detail::BlockType::Dynamic: {
                    const auto& block = dynamic_blocks[descriptor.index];
                    info.type = "Dynamic";
                    info.src_offset = block.src_offset;
                    info.field_index = block.field_index;
                    info.is_variable = true;
                    
                    // Add span-based serialization info
                    info.span_based_serialization = true;
                    // Get span count from the field type
                    if constexpr (detail::has_named_tuple_t_v<T>) {
                        using NT = rfl::named_tuple_t<T>;
                        const auto field_idx = block.field_index;
                        info.max_span_count = [field_idx]<typename... Fields>(rfl::NamedTuple<Fields...>*) {
                            std::size_t span_count = 0;
                            std::size_t current_idx = 0;
                            (void)((current_idx++ == field_idx ? 
                                   (span_count = detail::get_field_span_count<detail::field_type_t<Fields>>(), true) : 
                                   false) || ...);
                            return span_count;
                        }(static_cast<NT*>(nullptr));
                    }
                    break;
                }
                case detail::BlockType::RuntimeOffset: {
                    const auto& block = runtime_offset_blocks[descriptor.index];
                    info.type = "RuntimeOffset";
                    info.src_offset = block.src_offset;
                    info.size = block.size;
                    info.field_start = block.field_start;
                    info.field_count = block.field_count;
                    info.is_variable = false;
                    break;
                }
            }
            
            blocks.push_back(info);
        }
        
        return blocks;
    }
    
    /// @brief Calculate packed size at runtime (for variable-size fields)
    template<typename U = T>
    static std::size_t calculate_packed_size([[maybe_unused]] const U& value) {
        if constexpr (!has_variable_fields) {
            return base_packed_size;
        } else if constexpr (detail::has_named_tuple_t_v<T>) {
            std::size_t total = base_packed_size;
            
            // Convert to NamedTuple for field access
            auto nt = rfl::to_named_tuple(value);
            
            // Add size of each variable field's actual content
            for (std::size_t i = 0; i < dynamic_block_count; ++i) {
                const auto& block = dynamic_blocks[i];
                
                // Add length prefix size
                total += sizeof(uint32_t);
                
                // Visit the field at runtime and get its byte size
                total += detail::visit_field_by_index(nt, block.field_index, 
                    [](const auto& field) -> std::size_t {
                        return detail::get_container_byte_size(field);
                    });
            }
            
            // Add runtime offset blocks (fixed-size fields after dynamic content)
            for (std::size_t i = 0; i < runtime_offset_block_count; ++i) {
                total += runtime_offset_blocks[i].size;
            }
            
            return total;
        } else {
            // Fallback for non-reflectable types
            return sizeof(T);
        }
    }
    
    /// @brief Serialize using hybrid block-based approach
    static std::vector<std::byte> serialize(const T& value) {
        if constexpr (!has_variable_fields) {
            // Pure fixed - simple memcpy
            std::vector<std::byte> buffer(base_packed_size);
            const auto* src = reinterpret_cast<const std::byte*>(&value);
            
            for (std::size_t i = 0; i < fixed_block_count; ++i) {
                const auto& block = fixed_blocks[i];
                std::memcpy(buffer.data() + block.dst_offset, 
                           src + block.src_offset, 
                           block.size);
            }
            
            return buffer;
        } else if constexpr (detail::has_named_tuple_t_v<T>) {
            // Variable fields - execute blocks in order
            const std::size_t total_size = calculate_packed_size(value);
            std::vector<std::byte> buffer(total_size);
            
            const auto* src = reinterpret_cast<const std::byte*>(&value);
            auto nt = rfl::to_named_tuple(value);
            std::size_t current_offset = 0;
            
            // Execute blocks in optimal order
            for (std::size_t i = 0; i < total_blocks; ++i) {
                const auto& descriptor = execution_order[i];
                
                switch (descriptor.type) {
                    case detail::BlockType::Fixed: {
                        const auto& block = fixed_blocks[descriptor.index];
                        std::memcpy(buffer.data() + block.dst_offset,
                                   src + block.src_offset,
                                   block.size);
                        current_offset = block.dst_offset + block.size;
                        break;
                    }
                    
                    case detail::BlockType::Padding:
                        // Skip padding - not serialized
                        break;
                    
                    case detail::BlockType::Dynamic: {
                        const auto& block = dynamic_blocks[descriptor.index];
                        
                        // Serialize variable field with length prefix
                        detail::visit_field_by_index(nt, block.field_index,
                            [&](const auto& field) {
                                using FieldType = std::decay_t<decltype(field)>;
                                if constexpr (detail::is_fixed_container_impl<FieldType>::value) {
                                    // Write length prefix (uint32_t)
                                    uint32_t length = static_cast<uint32_t>(field.size());
                                    std::memcpy(buffer.data() + current_offset, &length, sizeof(uint32_t));
                                    current_offset += sizeof(uint32_t);
                                    
                                    // Write data
                                    std::size_t data_size = field.size() * sizeof(typename FieldType::value_type);
                                    if (data_size > 0) {
                                        std::memcpy(buffer.data() + current_offset, field.data(), data_size);
                                        current_offset += data_size;
                                    }
                                }
                                return 0; // dummy return for fold expression
                            });
                        break;
                    }
                    
                    case detail::BlockType::RuntimeOffset: {
                        const auto& block = runtime_offset_blocks[descriptor.index];
                        std::memcpy(buffer.data() + current_offset,
                                   src + block.src_offset,
                                   block.size);
                        current_offset += block.size;
                        break;
                    }
                }
            }
            
            return buffer;
        } else {
            // Fallback - shouldn't reach here
            return {};
        }
    }
    
    /// @brief Deserialize using hybrid block-based approach
    static T deserialize(const std::byte* data, std::size_t size) {
        // For fixed-size types, validate minimum size
        if constexpr (!has_variable_fields) {
            if (size < base_packed_size) {
                throw std::runtime_error("Buffer too small for deserialization");
            }
        }
        
        T result{};
        
        if constexpr (!has_variable_fields) {
            // Pure fixed - simple memcpy
            auto* dst = reinterpret_cast<std::byte*>(&result);
            
            for (std::size_t i = 0; i < fixed_block_count; ++i) {
                const auto& block = fixed_blocks[i];
                if (block.dst_offset + block.size <= size) {
                    std::memcpy(dst + block.src_offset,
                               data + block.dst_offset,
                               block.size);
                }
            }
        } else if constexpr (detail::has_named_tuple_t_v<T>) {
            // Variable fields - need direct struct access for modification
            auto* dst = reinterpret_cast<std::byte*>(&result);
            std::size_t current_offset = 0;
            
            for (std::size_t i = 0; i < total_blocks; ++i) {
                const auto& descriptor = execution_order[i];
                
                switch (descriptor.type) {
                    case detail::BlockType::Fixed: {
                        const auto& block = fixed_blocks[descriptor.index];
                        if (block.dst_offset + block.size <= size) {
                            std::memcpy(dst + block.src_offset,
                                       data + block.dst_offset,
                                       block.size);
                            current_offset = block.dst_offset + block.size;
                        }
                        break;
                    }
                    
                    case detail::BlockType::Padding:
                        // Skip padding
                        break;
                    
                    case detail::BlockType::Dynamic: {
                        const auto& block = dynamic_blocks[descriptor.index];
                        
                        // Read length prefix
                        if (current_offset + sizeof(uint32_t) <= size) {
                            uint32_t length = 0;
                            std::memcpy(&length, data + current_offset, sizeof(uint32_t));
                            current_offset += sizeof(uint32_t);
                            
                            // Directly access and modify the container via its offset
                            // Generic deserialization for all SerializableContainer types
                            auto nt = rfl::to_named_tuple(result);
                            detail::visit_field_by_index(nt, block.field_index,
                                [&](const auto& field_ref) -> int {
                                    using FieldType = std::decay_t<decltype(field_ref)>;
                                    
                                    // Unified deserialization for all containers
                                    if constexpr (SerializableContainer<FieldType>) {
                                        auto* container = reinterpret_cast<FieldType*>(dst + block.src_offset);
                                        std::size_t data_size = length * sizeof(typename FieldType::value_type);
                                        
                                        if (current_offset + data_size <= size && length <= container->capacity()) {
                                            // All containers use data_unsafe() + set_size_unsafe()
                                            if (data_size > 0) {
                                                std::memcpy(container->data_unsafe(), data + current_offset, data_size);
                                            }
                                            container->set_size_unsafe(length);
                                            current_offset += data_size;
                                        }
                                    }
                                    return 0; // dummy return
                                });
                        }
                        break;
                    }
                    
                    case detail::BlockType::RuntimeOffset: {
                        const auto& block = runtime_offset_blocks[descriptor.index];
                        if (current_offset + block.size <= size) {
                            std::memcpy(dst + block.src_offset,
                                       data + current_offset,
                                       block.size);
                            current_offset += block.size;
                        }
                        break;
                    }
                }
            }
        }
        
        return result;
    }
};

// ============================================================================
// Schema Generation Helpers (extends MemoryMap with HybridMemoryMap info)
// ============================================================================

/// @brief Get enhanced schema with hybrid memory map block information
template<typename T>
inline TypeSchema get_hybrid_schema() {
    auto schema = MemoryMap<T>::get_schema();
    
    // Add hybrid memory map information
    using HMM = HybridMemoryMap<T>;
    schema.has_variable_fields = HMM::has_variable_fields;
    schema.base_packed_size = HMM::base_packed_size;
    schema.fixed_block_count = HMM::fixed_block_count;
    schema.dynamic_block_count = HMM::dynamic_block_count;
    schema.runtime_offset_block_count = HMM::runtime_offset_block_count;
    schema.blocks = HMM::get_block_info();
    
    return schema;
}

/// @brief Get enhanced schema with custom name/category
template<typename T>
inline TypeSchema get_hybrid_schema(const std::string& name, const std::string& category = "") {
    auto schema = MemoryMap<T>::get_schema(name, category);
    
    // Add hybrid memory map information
    using HMM = HybridMemoryMap<T>;
    schema.has_variable_fields = HMM::has_variable_fields;
    schema.base_packed_size = HMM::base_packed_size;
    schema.fixed_block_count = HMM::fixed_block_count;
    schema.dynamic_block_count = HMM::dynamic_block_count;
    schema.runtime_offset_block_count = HMM::runtime_offset_block_count;
    schema.blocks = HMM::get_block_info();
    
    return schema;
}

} // namespace sertial
