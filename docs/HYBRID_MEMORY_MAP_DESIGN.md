# Hybrid Memory Map Design Plan

## Goal
Create a unified memory map that handles both fixed and variable-size fields with optimal memcpy regions and runtime offset calculation.

## Current State Analysis

### What We Have (MemoryMap<T>)
- ✅ Compile-time field offsets and sizes
- ✅ Packed offsets (removes padding)
- ✅ MemcpyRegion detection for consecutive fixed fields
- ✅ Single memcpy optimization when no padding

### What's Missing
- ❌ Variable-field aware region splitting
- ❌ Runtime offset calculation for post-variable regions
- ❌ Dynamic size calculation per variable field
- ❌ Proper block sequencing (Fixed → Dynamic → Fixed with runtime offset)

## Memory Layout Example

```cpp
struct Example {
    int a;              // offset 0, size 4
    float b;            // offset 4, size 4  
    fixed_vector<int, 10> vec;  // offset 8, size 40 (capacity-based in struct)
    int c;              // offset 48, size 4
    float d;            // offset 52, size 4
    int e;              // offset 56, size 4
};
```

### Packed Serialization Layout (no padding assumed)
```
[a:4][b:4][vec.size():4][vec.data:size*4][c:4][d:4][e:4]
 <-- 8 -->              <--- runtime --->  <---  12  --->
  Block0                  DynamicBlock        Block1 (runtime offset)
```

## Block Types Needed

### 1. FixedBlock
```cpp
struct FixedBlock {
    std::size_t src_offset;      // Offset in struct
    std::size_t dst_offset;      // Offset in packed buffer (compile-time)
    std::size_t size;            // Bytes to memcpy
    std::size_t field_start;     // First field index
    std::size_t field_count;     // Number of consecutive fields
};
```

### 2. PaddingBlock
```cpp
struct PaddingBlock {
    std::size_t size;            // Padding bytes to skip (in struct)
    std::size_t src_offset;      // Where padding starts in struct
    // No dst_offset - padding is removed in packed format
};
```

### 3. DynamicBlock  
```cpp
struct DynamicBlock {
    std::size_t field_index;           // Which field
    std::size_t src_offset;            // Offset of container in struct
    std::size_t base_dst_offset;       // Where to start (after previous blocks)
    std::size_t element_size;          // sizeof(T) for vector<T>
    std::size_t capacity;              // Max elements (for fixed containers)
    bool needs_length_prefix;          // Serialize size() first?
};
```

### 4. RuntimeOffsetBlock (FixedBlock after DynamicBlock)
```cpp
struct RuntimeOffsetBlock {
    std::size_t src_offset;      // Offset in struct  
    std::size_t size;            // Bytes to memcpy
    std::size_t field_start;     // First field index
    std::size_t field_count;     // Number of fields
    // dst_offset calculated at runtime based on previous dynamic blocks
};
```

## HybridMemoryMap Structure

```cpp
template<typename T>
struct HybridMemoryMap {
    // Counts
    static constexpr std::size_t fixed_block_count;
    static constexpr std::size_t padding_block_count;
    static constexpr std::size_t dynamic_block_count;
    static constexpr std::size_t runtime_offset_block_count;
    
    // Block arrays (compile-time)
    static constexpr std::array<FixedBlock, N> fixed_blocks;
    static constexpr std::array<PaddingBlock, P> padding_blocks;
    static constexpr std::array<DynamicBlock, M> dynamic_blocks;
    static constexpr std::array<RuntimeOffsetBlock, K> runtime_offset_blocks;
    
    // Execution order (interleaved blocks)
    static constexpr std::array<BlockDescriptor, fixed_block_count + padding_block_count + dynamic_block_count + runtime_offset_block_count> execution_order;
    
    // Sizes
    static constexpr std::size_t base_packed_size;  // Without dynamic content
    static std::size_t calculate_packed_size(const T& value);  // With dynamic content
};
```

## Algorithm: Build Block Sequence

```cpp
constexpr auto build_blocks() {
    Result result{};
    
    std::size_t current_dst_offset = 0;
    std::size_t fixed_start = 0;
    std::size_t fixed_size = 0;
    bool past_dynamic_field = false;
    
    for (std::size_t i = 0; i < num_fields; ++i) {
        if (is_variable_field[i]) {
            // Close any ongoing fixed block
            if (fixed_size > 0) {
                if (!past_dynamic_field) {
                    result.fixed_blocks[result.fixed_count++] = 
                        FixedBlock{struct_offsets[i-1], current_dst_offset - fixed_size, fixed_size, ...};
                } else {
                    result.runtime_blocks[result.runtime_count++] = 
                        RuntimeOffsetBlock{struct_offsets[i-1], fixed_size, ...};
                }
                fixed_size = 0;
            }
            
            // Add dynamic block
            result.dynamic_blocks[result.dynamic_count++] = 
                DynamicBlock{i, struct_offsets[i], current_dst_offset, elem_size[i], capacity[i], true};
            
            past_dynamic_field = true;
            // Don't advance dst_offset - it's runtime
            
        } else {
            // Accumulate fixed field
            if (fixed_size == 0) {
                fixed_start = struct_offsets[i];
            }
            
            // Check if consecutive (no padding gap)
            if (fixed_size > 0 && struct_offsets[i] != fixed_start + fixed_size) {
                // Padding gap - close and restart
                if (!past_dynamic_field) {
                    result.fixed_blocks[result.fixed_count++] = 
                        FixedBlock{fixed_start, current_dst_offset - fixed_size, fixed_size, ...};
                } else {
                    result.runtime_blocks[result.runtime_count++] = 
                        RuntimeOffsetBlock{fixed_start, fixed_size, ...};
                }
                current_dst_offset += field_sizes[i];
                fixed_start = struct_offsets[i];
                fixed_size = field_sizes[i];
            } else {
                fixed_size += field_sizes[i];
                if (!past_dynamic_field) {
                    current_dst_offset += field_sizes[i];
                }
            }
        }
    }
    
    // Close final block
    if (fixed_size > 0) {
        if (!past_dynamic_field) {
            result.fixed_blocks[result.fixed_count++] = FixedBlock{...};
        } else {
            result.runtime_blocks[result.runtime_count++] = RuntimeOffsetBlock{...};
        }
    }
    
    result.base_packed_size = current_dst_offset;
    return result;
}
```

## Serialization Algorithm

```cpp
std::size_t serialize_hybrid(const T& value, std::byte* dest) {
    std::size_t offset = 0;
    const std::byte* src = reinterpret_cast<const std::byte*>(&value);
    
    // Process in execution order
    for (const auto& block_desc : execution_order) {
        switch (block_desc.type) {
            case BlockType::Fixed: {
                const auto& block = fixed_blocks[block_desc.index];
                std::memcpy(dest + block.dst_offset, src + block.src_offset, block.size);
                offset = block.dst_offset + block.size;
                break;
            }
            
            case BlockType::Dynamic: {
                const auto& block = dynamic_blocks[block_desc.index];
                // Get field value and serialize runtime content
                auto nt = rfl::to_named_tuple(value);
                visit_field<block.field_index>(nt, [&](const auto& field) {
                    std::size_t count = field.size();
                    std::size_t bytes = count * block.element_size;
                    
                    if (block.needs_length_prefix) {
                        // Write size prefix
                        std::memcpy(dest + offset, &count, sizeof(count));
                        offset += sizeof(count);
                    }
                    
                    if (count > 0) {
                        std::memcpy(dest + offset, field.data(), bytes);
                        offset += bytes;
                    }
                });
                break;
            }
            
            case BlockType::RuntimeOffset: {
                const auto& block = runtime_offset_blocks[block_desc.index];
                // offset already advanced by previous blocks
                std::memcpy(dest + offset, src + block.src_offset, block.size);
                offset += block.size;
                break;
            }
        }
    }
    
    return offset;
}
```

## Implementation Steps

### Phase 1: Core Infrastructure (Current Task)
1. ✅ Create block type definitions (FixedBlock, DynamicBlock, RuntimeOffsetBlock)
2. ✅ Create BlockDescriptor with execution order
3. ✅ Implement build_blocks() algorithm
4. ✅ Store blocks in HybridMemoryMap

### Phase 2: Field Iteration (Next)
5. ⏳ Create visit_field<Index> helper for runtime field access
6. ⏳ Implement dynamic size calculation per field
7. ⏳ Create total packed size calculator

### Phase 3: Serialization (After Phase 2)
8. ⏳ Implement hybrid serialize with execution order
9. ⏳ Implement hybrid deserialize with length prefix reading
10. ⏳ Add endianness support for dynamic blocks

### Phase 4: Integration & Testing
11. ⏳ Update existing APIs to use HybridMemoryMap
12. ⏳ Test with various struct combinations
13. ⏳ Benchmark against current implementation

## Key Design Decisions

1. **Length Prefix**: Always serialize `size()` before variable content for deserialization
2. **Execution Order Array**: Single array describing block sequence for cache efficiency
3. **Zero Malloc**: Use compile-time sized arrays, runtime only reads size()
4. **Reuse MemoryMap**: Build on existing field offset/size infrastructure
5. **Single Source of Truth**: All serialization paths use HybridMemoryMap

## Testing Strategy

```cpp
// Test cases needed:
struct PureFixed { int a; float b; };                    // All fixed, 1 block
struct OneDynamic { int a; fixed_vector<int,10> v; };   // Fixed + Dynamic
struct Sandwich { int a; fixed_vector<int,10> v; int b; }; // Fixed + Dynamic + RuntimeOffset
struct MultiDynamic { fixed_vector<int,10> v1; fixed_vector<float,5> v2; }; // Multiple dynamics
struct Padded { char a; int b; fixed_vector<int,10> v; short c; }; // With padding
```

This plan provides a complete roadmap. Should I proceed with Phase 1 implementation?
