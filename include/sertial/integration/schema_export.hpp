#pragma once

#include "../core/layout/struct_layout.hpp"
#include "../core/layout/block_types.hpp"
#include <rfl.hpp>
#include <rfl/json.hpp>
#include <string>
#include <vector>

namespace sertial {

// ============================================================================
// Schema Export - Direct from StructLayout
// ============================================================================
// Converts StructLayout's constexpr metadata to JSON-serializable format.
// Uses rfl::json::write() for automatic JSON generation.
// ============================================================================

/// @brief Block information for JSON export
struct BlockInfo {
    std::string type;
    std::size_t src_offset;
    std::size_t dst_offset;
    std::size_t size;
    std::size_t field_index;
    std::size_t capacity;
    std::size_t element_size;
};

/// @brief Type schema for JSON export
struct TypeSchema {
    std::string name;
    std::size_t sizeof_bytes;
    std::size_t base_packed_size;
    std::size_t max_packed_size;
    bool has_variable_fields;
    std::size_t field_count;
    std::size_t fixed_block_count;
    std::size_t dynamic_block_count;
    std::size_t runtime_offset_block_count;
    std::vector<BlockInfo> blocks;
};

/// @brief Export schema directly from StructLayout
template<typename T>
TypeSchema export_schema() {
    using Layout = StructLayout<T>;
    
    TypeSchema schema;
    schema.name = std::string(rfl::type_name_t<T>().str());
    schema.sizeof_bytes = sizeof(T);
    schema.base_packed_size = Layout::base_packed_size;
    schema.max_packed_size = Layout::max_packed_size;
    schema.has_variable_fields = Layout::has_variable_fields;
    schema.field_count = Layout::num_fields;
    schema.fixed_block_count = Layout::fixed_block_count;
    schema.dynamic_block_count = Layout::dynamic_block_count;
    schema.runtime_offset_block_count = Layout::runtime_offset_block_count;
    
    // Export blocks in execution order
    for (std::size_t i = 0; i < Layout::total_blocks; ++i) {
        const auto& descriptor = Layout::execution_order[i];
        BlockInfo block_info;
        
        switch (descriptor.type) {
            case detail::BlockType::Fixed: {
                const auto& block = Layout::fixed_blocks[descriptor.index];
                block_info.type = "Fixed";
                block_info.src_offset = block.src_offset;
                block_info.dst_offset = block.dst_offset;
                block_info.size = block.size;
                block_info.field_index = block.field_start;
                block_info.capacity = 0;
                block_info.element_size = 0;
                schema.blocks.push_back(block_info);
                break;
            }
            
            case detail::BlockType::Padding: {
                const auto& block = Layout::padding_blocks[descriptor.index];
                block_info.type = "Padding";
                block_info.src_offset = block.src_offset;
                block_info.dst_offset = 0;
                block_info.size = block.size;
                block_info.field_index = 0;
                block_info.capacity = 0;
                block_info.element_size = 0;
                schema.blocks.push_back(block_info);
                break;
            }
            
            case detail::BlockType::Dynamic: {
                const auto& block = Layout::dynamic_blocks[descriptor.index];
                block_info.type = "Dynamic";
                block_info.src_offset = block.src_offset;
                block_info.dst_offset = block.base_dst_offset;
                block_info.size = 0;  // Variable size
                block_info.field_index = block.field_index;
                block_info.capacity = block.capacity;
                block_info.element_size = block.element_size;
                schema.blocks.push_back(block_info);
                break;
            }
            
            case detail::BlockType::RuntimeOffset: {
                const auto& block = Layout::runtime_offset_blocks[descriptor.index];
                block_info.type = "RuntimeOffset";
                block_info.src_offset = block.src_offset;
                block_info.dst_offset = 0;  // Computed at runtime
                block_info.size = block.size;
                block_info.field_index = block.field_start;
                block_info.capacity = 0;
                block_info.element_size = 0;
                schema.blocks.push_back(block_info);
                break;
            }
        }
    }
    
    return schema;
}

/// @brief Export schema to JSON string
template<typename T>
std::string export_schema_json() {
    return rfl::json::write(export_schema<T>());
}

} // namespace sertial
