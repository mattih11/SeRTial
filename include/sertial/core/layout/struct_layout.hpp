#pragma once

#include "../concepts.hpp"
#include "../traits/padding.hpp"
#include "block_types.hpp"  // Block type definitions
#include "../../containers/container_registration.hpp"
#include <cstddef>
#include <array>
#include <span>
#include <cstring>
#include <rfl.hpp>

namespace sertial {

// ============================================================================
// StructLayout<T> - Single Source of Truth
// ============================================================================
// Unified compile-time struct analysis and runtime serialization/deserialization.
// Replaces the old 3-file split (memory_map/hybrid_memory_map/unified_binary).
//
// Key Features:
// - Constexpr metadata (std::array, not std::vector - zero allocation)
// - std::span-based APIs with compile-time size validation
// - Serialize/deserialize co-located with block metadata
// - Schema derives automatically from same constexpr data
// ============================================================================

namespace detail {

// Block types are now in block_types.hpp (single source of truth)
using ::sertial::detail::BlockType;
using ::sertial::detail::FixedBlock;
using ::sertial::detail::PaddingBlock;
using ::sertial::detail::DynamicBlock;
using ::sertial::detail::RuntimeOffsetBlock;
using ::sertial::detail::BlockDescriptor;

// ========================================================================
// Field Visitor - Runtime field access by index
// ========================================================================

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

} // namespace detail

// ============================================================================
// StructLayout<T> - Primary Template (Reflectable Types Only)
// ============================================================================

template<typename T>
struct StructLayout {
    // Type must be reflectable
    static_assert(detail::has_named_tuple_t_v<T>, 
                  "Type must be reflectable (use rfl::Reflector or aggregate)");
    
    // ========================================================================
    // Compile-Time Field Analysis (using MemoryMap approach)
    // ========================================================================
    
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
    
    // Build field metadata arrays using the same approach as MemoryMap
    template<typename... Fields>
    static constexpr auto build_field_sizes_from_nt(rfl::NamedTuple<Fields...>*) {
        return std::array<std::size_t, sizeof...(Fields)>{
            sizeof(detail::field_type_t<Fields>)...
        };
    }
    
    template<typename... Fields>
    static constexpr auto build_field_alignments_from_nt(rfl::NamedTuple<Fields...>*) {
        return std::array<std::size_t, sizeof...(Fields)>{
            alignof(detail::field_type_t<Fields>)...
        };
    }
    
    template<typename... Fields>
    static constexpr auto build_field_is_variable_from_nt(rfl::NamedTuple<Fields...>*) {
        return std::array<bool, sizeof...(Fields)>{
            SerializableContainer<detail::field_type_t<Fields>>...
        };
    }
    
    template<typename... Fields>
    static constexpr auto build_field_element_sizes_from_nt(rfl::NamedTuple<Fields...>*) {
        return std::array<std::size_t, sizeof...(Fields)>{
            []() constexpr {
                using FieldType = detail::field_type_t<Fields>;
                if constexpr (SerializableContainer<FieldType>) {
                    return sizeof(typename FieldType::value_type);
                } else {
                    return std::size_t{0};
                }
            }()...
        };
    }
    
    template<typename... Fields>
    static constexpr auto build_field_capacities_from_nt(rfl::NamedTuple<Fields...>*) {
        return std::array<std::size_t, sizeof...(Fields)>{
            []() constexpr {
                using FieldType = detail::field_type_t<Fields>;
                if constexpr (SerializableContainer<FieldType>) {
                    return FieldType::max_size_v;
                } else {
                    return std::size_t{0};
                }
            }()...
        };
    }
    
    static constexpr auto field_sizes = []() {
        if constexpr (num_fields == 0) {
            return std::array<std::size_t, 1>{};
        } else {
            using NT = rfl::named_tuple_t<T>;
            return build_field_sizes_from_nt(static_cast<NT*>(nullptr));
        }
    }();
    
    static constexpr auto field_alignments = []() {
        if constexpr (num_fields == 0) {
            return std::array<std::size_t, 1>{};
        } else {
            using NT = rfl::named_tuple_t<T>;
            return build_field_alignments_from_nt(static_cast<NT*>(nullptr));
        }
    }();
    
    static constexpr auto field_is_variable = []() {
        if constexpr (num_fields == 0) {
            return std::array<bool, 1>{};
        } else {
            using NT = rfl::named_tuple_t<T>;
            return build_field_is_variable_from_nt(static_cast<NT*>(nullptr));
        }
    }();
    
    static constexpr auto element_sizes = []() {
        if constexpr (num_fields == 0) {
            return std::array<std::size_t, 1>{};
        } else {
            using NT = rfl::named_tuple_t<T>;
            return build_field_element_sizes_from_nt(static_cast<NT*>(nullptr));
        }
    }();
    
    static constexpr auto capacities = []() {
        if constexpr (num_fields == 0) {
            return std::array<std::size_t, 1>{};
        } else {
            using NT = rfl::named_tuple_t<T>;
            return build_field_capacities_from_nt(static_cast<NT*>(nullptr));
        }
    }();
    
    // Compute field offsets from sizes and alignments (constexpr-friendly!)
    static constexpr auto field_offsets = []() {
        std::array<std::size_t, (num_fields > 0 ? num_fields : 1)> offsets{};
        if constexpr (num_fields > 0) {
            std::size_t current = 0;
            for (std::size_t i = 0; i < num_fields; ++i) {
                // Align to field's alignment requirement
                std::size_t align = field_alignments[i];
                std::size_t aligned = (current + align - 1) & ~(align - 1);
                offsets[i] = aligned;
                current = aligned + field_sizes[i];
            }
        }
        return offsets;
    }();
    
    // ========================================================================
    // Block Building (Compile-Time)
    // ========================================================================
    
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
            const std::size_t struct_offset = field_offsets[i];
            const std::size_t field_size = field_sizes[i];
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
                
                // Add dynamic block
                layout.dynamic_blocks[layout.dynamic_count] = 
                    detail::DynamicBlock(i, struct_offset, current_dst_offset,
                                       element_sizes[i], capacities[i], true);
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
                        // Gap detected - close region, add padding, start new
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
                        
                        // Add padding
                        std::size_t padding_size = struct_offset - expected_offset;
                        layout.padding_blocks[layout.padding_count] = 
                            detail::PaddingBlock(expected_offset, padding_size);
                        layout.execution_order[layout.execution_count++] = 
                            detail::BlockDescriptor(detail::BlockType::Padding, layout.padding_count);
                        layout.padding_count++;
                        
                        // Start new region
                        region_start_src = struct_offset;
                        region_start_dst = current_dst_offset;
                        region_size = field_size;
                        region_field_start = i;
                        region_field_count = 1;
                    }
                }
            }
        }
        
        // Close final fixed region
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
    
    // Expose block data
    static constexpr auto fixed_blocks = layout.fixed_blocks;
    static constexpr auto padding_blocks = layout.padding_blocks;
    static constexpr auto dynamic_blocks = layout.dynamic_blocks;
    static constexpr auto runtime_offset_blocks = layout.runtime_offset_blocks;
    static constexpr auto execution_order = layout.execution_order;
    
    static constexpr std::size_t fixed_block_count = layout.fixed_count;
    static constexpr std::size_t padding_block_count = layout.padding_count;
    static constexpr std::size_t dynamic_block_count = layout.dynamic_count;
    static constexpr std::size_t runtime_offset_block_count = layout.runtime_offset_count;
    static constexpr std::size_t total_blocks = layout.execution_count;
    
    static constexpr bool has_variable_fields = []() {
        for (std::size_t i = 0; i < num_fields; ++i) {
            if (field_is_variable[i]) return true;
        }
        return false;
    }();
    
    // ========================================================================
    // Size Calculations (Compile-Time)
    // ========================================================================
    
    static constexpr std::size_t base_packed_size = layout.base_packed_size;
    
    static constexpr std::size_t max_packed_size = []() {
        std::size_t size = base_packed_size;
        for (std::size_t i = 0; i < layout.dynamic_count; ++i) {
            const auto& block = layout.dynamic_blocks[i];
            size += sizeof(uint32_t) + (block.capacity * block.element_size);
        }
        for (std::size_t i = 0; i < layout.runtime_offset_count; ++i) {
            size += layout.runtime_offset_blocks[i].size;
        }
        return size;
    }();
    
    // ========================================================================
    // Buffer Type Alias (Clean User-Facing API)
    // ========================================================================
    
    /// @brief Stack-allocated buffer type with exact compile-time size
    /// Use this instead of verbose std::array<std::byte, max_packed_size>
    using buffer_type = std::array<std::byte, max_packed_size>;
    
    // ========================================================================
    // Serialization (std::span-based API)
    // ========================================================================
    
    /// @brief Serialize to compile-time sized span
    static std::size_t serialize(const T& obj, std::span<std::byte, max_packed_size> dest) {
        if constexpr (!has_variable_fields) {
            // Pure fixed
            const auto* src = reinterpret_cast<const std::byte*>(&obj);
            for (std::size_t i = 0; i < fixed_block_count; ++i) {
                const auto& block = fixed_blocks[i];
                std::memcpy(dest.data() + block.dst_offset, src + block.src_offset, block.size);
            }
            return base_packed_size;
        } else {
            // Variable fields
            const auto* src = reinterpret_cast<const std::byte*>(&obj);
            auto nt = rfl::to_named_tuple(obj);
            std::size_t current_offset = 0;
            
            for (std::size_t i = 0; i < total_blocks; ++i) {
                const auto& descriptor = execution_order[i];
                
                switch (descriptor.type) {
                    case detail::BlockType::Fixed: {
                        const auto& block = fixed_blocks[descriptor.index];
                        std::memcpy(dest.data() + block.dst_offset, src + block.src_offset, block.size);
                        current_offset = block.dst_offset + block.size;
                        break;
                    }
                    
                    case detail::BlockType::Padding:
                        break;
                    
                    case detail::BlockType::Dynamic: {
                        const auto& block = dynamic_blocks[descriptor.index];
                        detail::visit_field_by_index(nt, block.field_index,
                            [&](const auto& field) -> int {
                                using FieldType = std::decay_t<decltype(field)>;
                                if constexpr (SerializableContainer<FieldType>) {
                                    uint32_t length = static_cast<uint32_t>(field.size());
                                    std::memcpy(dest.data() + current_offset, &length, sizeof(uint32_t));
                                    current_offset += sizeof(uint32_t);
                                    
                                    if (length > 0) {
                                        auto spans = get_serialization_spans(field);
                                        for (const auto& span : spans) {
                                            if (span.empty()) continue;
                                            std::size_t span_bytes = span.size() * block.element_size;
                                            std::memcpy(dest.data() + current_offset,
                                                       reinterpret_cast<const std::byte*>(span.data()),
                                                       span_bytes);
                                            current_offset += span_bytes;
                                        }
                                    }
                                }
                                return 0;  // Dummy return for visitor
                            });
                        break;
                    }
                    
                    case detail::BlockType::RuntimeOffset: {
                        const auto& block = runtime_offset_blocks[descriptor.index];
                        std::memcpy(dest.data() + current_offset, src + block.src_offset, block.size);
                        current_offset += block.size;
                        break;
                    }
                }
            }
            return current_offset;
        }
    }
    
    /// @brief Serialize to dynamic span (runtime check)
    static std::optional<std::size_t> serialize(const T& obj, std::span<std::byte> dest) {
        if (dest.size() < max_packed_size) {
            return std::nullopt;
        }
        std::array<std::byte, max_packed_size> temp;
        std::size_t size = serialize(obj, std::span<std::byte, max_packed_size>{temp});
        std::memcpy(dest.data(), temp.data(), size);
        return size;
    }
    
    // ========================================================================
    // Deserialization (std::span-based API)
    // ========================================================================
    
    /// @brief Deserialize from span
    static bool deserialize(T& obj, std::span<const std::byte> src) {
        if constexpr (!has_variable_fields) {
            if (src.size() < base_packed_size) return false;
            auto* dest = reinterpret_cast<std::byte*>(&obj);
            for (std::size_t i = 0; i < fixed_block_count; ++i) {
                const auto& block = fixed_blocks[i];
                std::memcpy(dest + block.src_offset, src.data() + block.dst_offset, block.size);
            }
            return true;
        } else {
            auto* dest = reinterpret_cast<std::byte*>(&obj);
            auto nt = rfl::to_named_tuple(obj);
            std::size_t current_offset = 0;
            
            for (std::size_t i = 0; i < total_blocks; ++i) {
                const auto& descriptor = execution_order[i];
                
                switch (descriptor.type) {
                    case detail::BlockType::Fixed: {
                        const auto& block = fixed_blocks[descriptor.index];
                        if (src.size() < block.dst_offset + block.size) return false;
                        std::memcpy(dest + block.src_offset, src.data() + block.dst_offset, block.size);
                        current_offset = block.dst_offset + block.size;
                        break;
                    }
                    
                    case detail::BlockType::Padding:
                        break;
                    
                    case detail::BlockType::Dynamic: {
                        const auto& block = dynamic_blocks[descriptor.index];
                        if (current_offset + sizeof(uint32_t) > src.size()) return false;
                        
                        uint32_t length;
                        std::memcpy(&length, src.data() + current_offset, sizeof(uint32_t));
                        current_offset += sizeof(uint32_t);
                        
                        if (length > block.capacity) return false;
                        
                        std::size_t data_size = length * block.element_size;
                        if (current_offset + data_size > src.size()) return false;
                        
                        // Direct pointer access to container in struct (bypasses named_tuple copy issue)
                        bool success = false;
                        detail::visit_field_by_index(nt, block.field_index,
                            [&](const auto& field_ref) -> int {
                                using FieldType = std::decay_t<decltype(field_ref)>;
                                if constexpr (SerializableContainer<FieldType>) {
                                    // Get direct pointer to container in struct
                                    auto* container = reinterpret_cast<FieldType*>(dest + block.src_offset);
                                    
                                    if (data_size > 0) {
                                        std::memcpy(container->data_unsafe(), src.data() + current_offset, data_size);
                                    }
                                    container->set_size_unsafe(length);
                                    current_offset += data_size;
                                    success = true;
                                }
                                return 0;  // Dummy return for visitor
                            });
                        if (!success) return false;
                        break;
                    }
                    
                    case detail::BlockType::RuntimeOffset: {
                        const auto& block = runtime_offset_blocks[descriptor.index];
                        if (current_offset + block.size > src.size()) return false;
                        std::memcpy(dest + block.src_offset, src.data() + current_offset, block.size);
                        current_offset += block.size;
                        break;
                    }
                }
            }
            return true;
        }
    }
    
    /// @brief Deserialize and return optional
    static std::optional<T> deserialize_opt(std::span<const std::byte> src) {
        T result{};
        if (deserialize(result, src)) {
            return result;
        }
        return std::nullopt;
    }
};

} // namespace sertial
