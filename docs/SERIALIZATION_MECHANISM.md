# SeRTial Serialization Mechanism

## Overview

SeRTial uses a **unified block-based serialization system** that handles both fixed-size and variable-size types through a single code path. All analysis happens at compile time, generating an optimal execution plan with zero runtime overhead.

## Core Components

### 1. StructLayout<T>
**Purpose**: Single source of truth for serialization layout  
**Analysis**: Compile-time struct introspection  
**Output**: Block execution plan

```cpp
// Example struct:
struct Message {
    uint32_t id;                    // Field 0: Fixed (4 bytes)
    fixed_vector<float, 100> data;  // Field 1: Dynamic (variable size)
    uint64_t timestamp;             // Field 2: Fixed (8 bytes)
};

// Compile-time analysis generates:
// Block 0: Fixed[src=0, dst=0, size=4]           → memcpy id
// Block 1: Dynamic[field=1, elem=4, cap=100]     → serialize data with prefix
// Block 2: RuntimeOffset[src=?, size=8]          → memcpy timestamp
```

### 2. Block Types

#### **FixedBlock**
- **What**: Contiguous fixed-size fields before any dynamic field
- **Serialization**: Single `memcpy` operation
- **Size**: Known at compile time
- **Offset**: Fixed at compile time

```cpp
struct FixedBlock {
    std::size_t src_offset;   // Offset in struct memory
    std::size_t dst_offset;   // Offset in serialized output
    std::size_t size;         // Total bytes to copy
    std::size_t field_start;  // First field index
    std::size_t field_count;  // Number of fields in block
};
```

#### **DynamicBlock**
- **What**: Variable-size containers (fixed_vector, fixed_string, RingBuffer)
- **Serialization**: Length prefix (4 bytes) + actual data
- **Size**: Known only at runtime via `.size()`
- **Offset**: Computed at runtime (depends on previous dynamic blocks)

```cpp
struct DynamicBlock {
    std::size_t field_index;       // Which struct field
    std::size_t src_offset;        // Offset in struct memory
    std::size_t dst_offset;        // Offset before data (for length prefix)
    std::size_t element_size;      // sizeof(T) - from compile-time traits
    std::size_t capacity;          // Max elements (N) - from compile-time traits
    bool has_length_prefix;        // Always true for containers
};
```

#### **RuntimeOffsetBlock**
- **What**: Fixed-size fields AFTER dynamic content
- **Serialization**: Single `memcpy` operation
- **Size**: Known at compile time
- **Offset**: Computed at runtime (depends on dynamic block sizes)

```cpp
struct RuntimeOffsetBlock {
    std::size_t src_offset;   // Offset in struct memory
    std::size_t size;         // Total bytes to copy
    std::size_t field_start;  // First field index
    std::size_t field_count;  // Number of fields in block
};
```

#### **PaddingBlock**
- **What**: Alignment gaps in struct memory layout
- **Serialization**: **Skipped** - not included in packed format
- **Purpose**: Track where compiler inserted padding for alignment

```cpp
struct PaddingBlock {
    std::size_t offset;  // Where padding starts in struct
    std::size_t size;    // Padding bytes
};
```

### 3. Execution Order

Blocks are executed in **field order** to maintain data consistency:

```cpp
// Execution plan stored as:
std::array<BlockDescriptor, max_blocks> execution_order;

struct BlockDescriptor {
    BlockType type;       // Fixed/Padding/Dynamic/RuntimeOffset
    std::size_t index;    // Index into type-specific array
};

// Example execution:
// 1. Fixed[0] → Copy id
// 2. Dynamic[0] → Serialize data with length prefix
// 3. RuntimeOffset[0] → Copy timestamp
```

## Serialization Flow

### Step 1: Compile-Time Analysis

```cpp
template<typename T>
struct HybridLayoutBuilder {
    // Extract field information via reflect-cpp
    static constexpr auto field_is_variable = [...]();  // Is field a container?
    static constexpr auto elem_sizes = [...]();         // sizeof(T) for containers
    static constexpr auto capacities = [...]();         // Max size (N) for containers
    
    // Build block structure
    static constexpr BlockLayout build_blocks() {
        // Analyze each field:
        for (field : struct_fields) {
            if (is_variable_length_field) {
                // Create DynamicBlock
                blocks.add(DynamicBlock{
                    field_index, 
                    struct_offset,
                    current_output_offset,  // Before data
                    elem_sizes[field],      // From compile-time traits
                    capacities[field]       // From compile-time traits
                });
            } else {
                // Add to FixedBlock or RuntimeOffsetBlock
                // depending on whether past dynamic field
            }
        }
        return blocks;
    }
};
```

### Step 2: Runtime Serialization

```cpp
template<typename T>
std::size_t serialize_to_unified(const T& value, std::byte* dest) {
    using HMM = StructLayout<T>;
    
    const auto* src = reinterpret_cast<const std::byte*>(&value);
    std::size_t current_offset = 0;
    
    // Execute blocks in order
    for (const auto& descriptor : HMM::execution_order) {
        switch (descriptor.type) {
            case BlockType::Fixed: {
                const auto& block = HMM::fixed_blocks[descriptor.index];
                std::memcpy(dest + block.dst_offset,    // Fixed destination
                           src + block.src_offset,       // Fixed source
                           block.size);                  // Fixed size
                current_offset = block.dst_offset + block.size;
                break;
            }
            
            case BlockType::Dynamic: {
                const auto& block = HMM::dynamic_blocks[descriptor.index];
                
                // Access container field at runtime
                auto nt = rfl::to_named_tuple(value);
                visit_field_by_index(nt, block.field_index, [&](const auto& field) {
                    // Write length prefix (4 bytes)
                    uint32_t length = static_cast<uint32_t>(field.size());  // RUNTIME
                    std::memcpy(dest + current_offset, &length, 4);
                    current_offset += 4;
                    
                    // Write data
                    std::size_t data_size = length * block.element_size;  // Compile-time elem size
                    if (data_size > 0) {
                        std::memcpy(dest + current_offset, field.data(), data_size);
                        current_offset += data_size;
                    }
                });
                break;
            }
            
            case BlockType::RuntimeOffset: {
                const auto& block = HMM::runtime_offset_blocks[descriptor.index];
                std::memcpy(dest + current_offset,      // RUNTIME offset
                           src + block.src_offset,       // Fixed source
                           block.size);                  // Fixed size
                current_offset += block.size;
                break;
            }
            
            case BlockType::Padding:
                // Skip - not serialized
                break;
        }
    }
    
    return current_offset;  // Total bytes written
}
```

## Size Calculations

### Compile-Time: max_packed_size

Maximum possible serialized size (all containers at capacity):

```cpp
template<typename T>
struct StructLayout {
    static constexpr std::size_t max_packed_size = []() constexpr {
        std::size_t size = base_packed_size;  // Fixed + RuntimeOffset blocks
        
        // Add worst-case size for each dynamic block
        for (const auto& block : dynamic_blocks) {
            size += sizeof(uint32_t);                      // Length prefix
            size += block.capacity * block.element_size;   // Max data
        }
        
        return size;
    }();
    
    // base_packed_size = sum of all FixedBlock + RuntimeOffsetBlock sizes
    static constexpr std::size_t base_packed_size = [...]();
};
```

### Runtime: calculate_packed_size()

Actual serialized size based on current container sizes:

```cpp
template<typename T>
static std::size_t calculate_packed_size(const T& value) {
    if constexpr (!has_variable_fields) {
        return base_packed_size;  // Compile-time constant
    } else {
        std::size_t total = base_packed_size;  // Fixed + RuntimeOffset
        
        auto nt = rfl::to_named_tuple(value);
        
        // Add actual size for each dynamic block
        for (const auto& block : dynamic_blocks) {
            total += sizeof(uint32_t);  // Length prefix
            
            total += visit_field_by_index(nt, block.field_index, [](const auto& field) {
                return field.size() * sizeof(typename decltype(field)::value_type);
            });
        }
        
        return total;
    }
}
```

### Size Guarantees

```cpp
// Compile-time guarantees:
static_assert(HMM::max_packed_size >= HMM::base_packed_size);
static_assert(HMM::base_packed_size == sum_of_fixed_and_runtime_offset_blocks);

// Runtime guarantees:
std::size_t actual = calculate_packed_size(obj);
assert(actual >= HMM::base_packed_size);        // At least base size
assert(actual <= HMM::max_packed_size);         // Never exceeds max
assert(actual == serialize(obj).size());        // Matches actual serialization
```

## Zero-Copy Views: std::span

SeRTial extensively uses `std::span<T>` for **zero-copy, non-owning views** of data:

### Buffer Output

```cpp
// static_buffer returns std::span for zero-copy access
auto buffer = serialize(message);
std::span<const std::byte> view = buffer.view();  // No copy

// Pass to I/O without copying
socket.send(view.data(), view.size());

// Or deserialize directly from span
auto msg = deserialize<Message>(view);
```

### Deserialization Input

```cpp
// Accept any contiguous byte buffer
std::optional<T> deserialize(std::span<const std::byte> data);

// Works with:
static_buffer<1024> buf;
deserialize<T>(buf.view());                    // Works

std::array<std::byte, 100> arr;
deserialize<T>(std::span{arr});                // Works

std::vector<std::byte> vec;
deserialize<T>(std::span{vec});                // Works

uint8_t* raw_ptr;
deserialize<T>(std::span{raw_ptr, size});      // Works
```

### Why std::span?

```cpp
// BAD: Copy-heavy API
std::vector<std::byte> serialize(const T& obj);     // Copies data
void deserialize(const std::vector<std::byte>& v);  // Requires specific container

// GOOD: Zero-copy API
static_buffer<N> serialize(const T& obj);           // Stack-allocated
    └─> .view() returns std::span                   // Zero-copy view

std::optional<T> deserialize(std::span<const std::byte>);  // Accepts any buffer
```

### std::span Benefits

1. **Zero-copy**: Non-owning view, no data duplication
2. **Type-safe**: Carries size information (unlike raw pointers)
3. **Flexible**: Works with array, vector, static_buffer, raw pointers
4. **Bounds-checked**: Size available at runtime (.size())
5. **Modern C++20**: Standard library type, no external dependencies

## Wire Format

### Fixed-Only Types
```
struct Player {
    uint32_t id;      // 4 bytes
    float health;     // 4 bytes
    float x, y, z;    // 12 bytes
};
// Total: 20 bytes

Wire format (20 bytes):
[id:4][health:4][x:4][y:4][z:4]
```

### Variable-Size Types
```
struct Message {
    uint32_t id;                    // 4 bytes
    fixed_vector<uint16_t, 100> v;  // Variable (0-200 bytes data)
    uint64_t timestamp;             // 8 bytes
};
// Total: 4 + (4 + N*2) + 8 = 16 + N*2 bytes

Wire format (N=3, total 22 bytes):
[id:4][length:4][v[0]:2][v[1]:2][v[2]:2][timestamp:8]
       \_____________________/
         Dynamic block (10 bytes)
```

### Padding Elimination Example
```
// In-memory layout (with padding):
struct Header {
    uint32_t type;     // 4 bytes
    uint8_t flags;     // 1 byte
    // 3 bytes padding (alignment)
    uint64_t timestamp;// 8 bytes
};
// sizeof(Header) = 16 bytes

// Serialized format (no padding):
[type:4][flags:1][timestamp:8]
// Packed size = 13 bytes (saves 3 bytes per message)
```

## Performance Characteristics

### Compile-Time Work
- Field layout analysis (offsets, sizes, alignments)
- Container detection (which fields are variable-length)
- Block generation (FixedBlock, DynamicBlock, RuntimeOffsetBlock)
- Max size computation (worst-case buffer allocation)
- Padding detection (which bytes to skip)

### Runtime Work
- Block execution (memcpy + length prefix writes)
- Container size queries (`.size()` calls)
- Offset calculation (for RuntimeOffset blocks)

### Zero Runtime Overhead
- No type checking (all compile-time)
- No virtual calls (template-based dispatch)
- No heap allocation (stack buffers with compile-time sizing)
- No dynamic size computation (pre-computed block structure)

### Typical Performance
```
Fixed-only types:  ~10-15 ns   (pure memcpy)
With containers:   ~30-50 ns   (memcpy + length prefix writes)
Deserialization:   Same or slightly faster (direct memory write)
```

## Future: RingBuffer Integration

RingBuffer requires special handling due to potential wrap-around:

```cpp
// Scenario 1: No wrap-around
Memory: [A][B][C][D][_][_][_][_]
        ^head        ^tail
Serialization: 1 memcpy (A-D contiguous)

// Scenario 2: Wrap-around
Memory: [C][D][_][_][_][_][A][B]
        ^tail        ^head
Serialization: 2 memcpy (A-B, then C-D)
                      ↓
               [A][B][C][D]  (linearized output)

// Always serialize in logical order (oldest→newest)
```

**Required compile-time information:**
- Element size: `sizeof(T)` (same as other containers)
- Capacity: `N` (same as other containers)
- **New**: Wrap-detection flag? Or handle at runtime via `.data()` check

**Design decision needed**: Single vs. dual memcpy path in serialization.
