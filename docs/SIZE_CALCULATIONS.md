# Size Calculations in SeRTial

## Overview

SeRTial needs to compute serialized buffer sizes at **two different times**:

1. **Compile-time**: Maximum possible size (for stack buffer allocation)
2. **Runtime**: Actual size based on current container contents

This document explains how these calculations work, the formulas involved, and provides examples.

## Table of Contents

- [Core Concepts](#core-concepts)
- [Compile-Time Size Calculation](#compile-time-size-calculation)
- [Runtime Size Calculation](#runtime-size-calculation)
- [Size Bounds and Guarantees](#size-bounds-and-guarantees)
- [Examples](#examples)
- [Implementation Details](#implementation-details)

---

## Core Concepts

### Three Key Sizes

```cpp
// 1. max_packed_size: Compile-time maximum
//    Used to allocate static_buffer<N>
constexpr size_t max_size = StructLayout<T>::max_packed_size;

// 2. base_packed_size: Compile-time fixed overhead
//    Size of all fixed-size fields + fixed overhead for dynamic fields
constexpr size_t base_size = StructLayout<T>::base_packed_size;

// 3. calculate_packed_size(obj): Runtime actual size
//    Computes actual serialized size based on container contents
size_t actual_size = calculate_packed_size(obj);
```

### Why We Need Both?

**Compile-time (`max_packed_size`)**:
- Allocate stack buffer: `static_buffer<max_packed_size>`
- Zero heap allocation guarantee
- Must be conservative (worst-case)

**Runtime (`calculate_packed_size`)**:
- Set actual buffer size after serialization
- Enable efficient transmission (don't send unused bytes)
- Must be exact (actual data written)

---

## Compile-Time Size Calculation

### Formula: max_packed_size

```cpp
max_packed_size = base_packed_size + Σ(dynamic_field_max_size)
```

Where:
- **base_packed_size**: Size of Fixed and RuntimeOffset blocks
- **dynamic_field_max_size**: For each dynamic field: `4 + (capacity * element_size)`

### Component Breakdown

#### 1. Fixed Blocks

Fixed blocks include all **contiguous fixed-size fields before any dynamic field**:

```cpp
struct Message {
    uint32_t id;          // Fixed block (offset 0, size 4)
    uint64_t timestamp;   // Fixed block (offset 4, size 8)
    // Total Fixed block: 12 bytes
};

base_packed_size = 12;
max_packed_size = 12;  // No dynamic fields
```

#### 2. Dynamic Blocks

Dynamic blocks are variable-size containers:

```cpp
struct Message {
    uint32_t id;                      // Fixed block: 4 bytes
    fixed_vector<float, 100> data;    // Dynamic block: 4 + 100*4 = 404 bytes (max)
    uint64_t timestamp;               // RuntimeOffset block: 8 bytes
};

// Calculation:
base_packed_size = 4 + 8 = 12;              // Fixed + RuntimeOffset
max_packed_size = 12 + (4 + 100*4) = 416;   // + length prefix + max data
```

**Dynamic block size formula**:
```
dynamic_max_size = sizeof(uint32_t) + (capacity * sizeof(element_type))
                 = 4 + (capacity * element_size)
```

The `4 bytes` are the **length prefix** (uint32_t) indicating how many elements are present.

#### 3. RuntimeOffset Blocks

RuntimeOffset blocks are fixed-size fields **after dynamic content**:

```cpp
struct Message {
    fixed_vector<float, 10> samples;  // Dynamic
    uint64_t checksum;                // RuntimeOffset (position varies)
};

// checksum offset depends on samples.size() at runtime
// But checksum SIZE is always 8 bytes (included in base_packed_size)
```

#### 4. Padding Blocks

Padding in the **struct layout** is **NOT serialized**:

```cpp
struct Padded {
    uint8_t a;   // 1 byte
    // 3 bytes padding (struct alignment)
    uint32_t b;  // 4 bytes
};

// Struct sizeof = 8 bytes (with padding)
// Serialized size = 5 bytes (no padding)
base_packed_size = 1 + 4 = 5;  // Padding skipped
```

### Implementation

From `struct_layout.hpp`:

```cpp
template<typename T>
struct StructLayout {
    // Compile-time field analysis
    static constexpr std::size_t num_fields = /* ... */;
    static constexpr auto field_is_variable = /* array of bools */;
    static constexpr auto element_sizes = /* array of element sizes */;
    static constexpr auto capacities = /* array of capacities */;
    
    // Block-based layout
    static constexpr BlockLayout layout = build_blocks();
    static constexpr std::size_t base_packed_size = layout.base_packed_size;
    
    // Max size includes worst-case for all dynamic fields
    static constexpr std::size_t max_packed_size = []() {
        std::size_t size = base_packed_size;
        for (std::size_t i = 0; i < layout.dynamic_count; ++i) {
            const auto& block = layout.dynamic_blocks[i];
            // 4 bytes length prefix + capacity * element_size
            size += sizeof(uint32_t) + (block.capacity * block.element_size);
        }
        for (std::size_t i = 0; i < layout.runtime_offset_count; ++i) {
            size += layout.runtime_offset_blocks[i].size;
        }
        return size;
    }();
};
```

---

## Runtime Size Calculation

### Formula: calculate_packed_size

```cpp
calculate_packed_size(obj) = base_packed_size + Σ(dynamic_field_actual_size)
```

Where:
- **dynamic_field_actual_size**: For each dynamic field: `4 + (size() * element_size)`

### Difference from Compile-Time

**Key difference**: Use **actual size()** instead of **capacity**:

```cpp
fixed_vector<float, 100> data;
data.push_back(1.0f);
data.push_back(2.0f);

// Compile-time calculation:
max_size = 4 + 100*4 = 404 bytes

// Runtime calculation:
actual_size = 4 + 2*4 = 12 bytes  // Only 2 elements present
```

### Implementation

From `unified_binary.hpp`:

```cpp
template<typename T>
std::size_t calculate_packed_size(const T& obj) {
    using HMM = StructLayout<T>;
    
    if constexpr (!HMM::has_variable_fields) {
        // No dynamic fields → size is constant
        return HMM::base_packed_size;
    } else {
        std::size_t size = HMM::base_packed_size;
        
        // Iterate through fields
        auto named_tuple = rfl::to_named_tuple(obj);
        rfl::visit_fields(named_tuple, [&]<typename Field>(const Field& field) {
            using FieldType = std::decay_t<decltype(field.value())>;
            
            if constexpr (is_fixed_container_v<FieldType>) {
                // Runtime size: 4 + size() * element_size
                size += sizeof(uint32_t) + 
                        field.value().size() * 
                        fixed_container_element_size_v<FieldType>;
            }
        });
        
        return size;
    }
}
```

---

## Size Bounds and Guarantees

### Invariants

1. **Minimum size**: `actual_size >= base_packed_size`
   - Always include fixed fields and RuntimeOffset fields
   - Empty containers still have 4-byte length prefix

2. **Maximum size**: `actual_size <= max_packed_size`
   - Enforced by fixed_vector/fixed_string capacity
   - Stack buffer allocation uses max_packed_size

3. **Relationship**: `base_packed_size <= actual_size <= max_packed_size`

### Formula Summary

```cpp
// For struct with N dynamic fields:
base_packed_size = Σ(fixed_field_sizes) + Σ(runtime_offset_field_sizes)

max_packed_size = base_packed_size + Σ(4 + capacity_i * element_size_i)
                                      i=1..N

actual_size = base_packed_size + Σ(4 + size_i * element_size_i)
                                  i=1..N

// Bounds:
base_packed_size <= actual_size <= max_packed_size
```

### Edge Cases

**Empty containers**:
```cpp
fixed_vector<uint32_t, 10> vec;  // size() == 0

// Still serializes length prefix
actual_size = 4 + 0*4 = 4 bytes
```

**Multiple dynamic fields**:
```cpp
struct Multi {
    fixed_vector<float, 10> a;     // Dynamic
    fixed_string<64> b;            // Dynamic
    uint32_t c;                    // RuntimeOffset
};

base_packed_size = 4;  // c (RuntimeOffset)

max_packed_size = 4 + (4 + 10*4) + (4 + 64*1)
                = 4 + 44 + 68 = 116 bytes

// Runtime with a.size()=2, b="Hi" (2 chars):
actual_size = 4 + (4 + 2*4) + (4 + 2*1)
            = 4 + 12 + 6 = 22 bytes
```

**No dynamic fields**:
```cpp
struct Static {
    uint32_t a;
    uint64_t b;
};

base_packed_size = 12;
max_packed_size = 12;  // Same as base
actual_size = 12;      // Always constant
```

---

## Examples

### Example 1: Simple Fixed-Size Struct

```cpp
struct Player {
    uint32_t id;
    float health;
    float x, y, z;
};

// All fields are fixed size
static_assert(sizeof(Player) == 20);  // In memory (with possible padding)

// Serialization:
base_packed_size = 4 + 4 + 4 + 4 + 4 = 20 bytes
max_packed_size = 20 bytes
actual_size = 20 bytes (always)

// Buffer allocation:
static_buffer<20> buffer;  // Exact size known at compile time
```

### Example 2: Single Dynamic Container

```cpp
struct SensorData {
    uint64_t timestamp;
    uint32_t sensor_id;
    fixed_vector<float, 100> readings;
};

// Size calculation:
base_packed_size = 8 + 4 = 12 bytes  // Fixed fields (readings not included)

max_packed_size = 12 + (4 + 100*4)
                = 12 + 404
                = 416 bytes

// Runtime examples:
SensorData data1{.timestamp = 123, .sensor_id = 1, .readings = {}};
actual_size = 12 + (4 + 0*4) = 16 bytes  // Empty readings

SensorData data2{.timestamp = 123, .sensor_id = 1, .readings = {1.0f, 2.0f, 3.0f}};
actual_size = 12 + (4 + 3*4) = 28 bytes  // 3 readings

SensorData data3{/* 100 readings */};
actual_size = 12 + (4 + 100*4) = 416 bytes  // Full capacity
```

### Example 3: Multiple Dynamic Fields with RuntimeOffset

```cpp
struct Message {
    uint32_t header_id;                // Fixed block (0-4)
    fixed_vector<uint32_t, 10> ids;    // Dynamic block
    fixed_string<64> name;             // Dynamic block
    uint64_t checksum;                 // RuntimeOffset block
};

// Compile-time calculation:
base_packed_size = 4 + 8 = 12 bytes  // header_id + checksum

max_packed_size = 12 + (4 + 10*4) + (4 + 64*1)
                = 12 + 44 + 68
                = 124 bytes

// Runtime example:
Message msg{
    .header_id = 42,
    .ids = {1, 2, 3},      // 3 elements
    .name = "Test",        // 4 characters
    .checksum = 0xDEADBEEF
};

actual_size = 12 + (4 + 3*4) + (4 + 4*1)
            = 12 + 16 + 8
            = 36 bytes

// Serialized layout:
// [header_id:4][ids_len:4][ids_data:12][name_len:4][name_data:4][checksum:8]
// Total: 4 + 4 + 12 + 4 + 4 + 8 = 36 bytes ✓
```

### Example 4: Nested Fixed Containers (Future)

```cpp
struct Nested {
    fixed_vector<fixed_vector<float, 5>, 10> matrix;  // 10 rows, 5 cols each
};

// Size calculation (conceptual - not yet implemented):
// Each inner vector: 4 + 5*4 = 24 bytes max
// Outer vector: 4 + 10*24 = 244 bytes max
max_packed_size = 4 + 244 = 248 bytes

// Runtime with 3 rows (2 elements, 1 element, 0 elements):
// Row 0: 4 + 2*4 = 12 bytes
// Row 1: 4 + 1*4 = 8 bytes
// Row 2: 4 + 0*4 = 4 bytes
// Total: 4 + 12 + 8 + 4 = 28 bytes
```

---

## Implementation Details

### Where Sizes Are Computed

1. **StructLayout compile-time constants**:
   ```cpp
   template<typename T>
   struct StructLayout {
       static constexpr size_t base_packed_size = /* ... */;
       static constexpr size_t max_packed_size = /* ... */;
       static constexpr bool has_variable_fields = /* ... */;
   };
   ```

2. **Runtime calculation function**:
   ```cpp
   template<typename T>
   size_t calculate_packed_size(const T& obj);
   ```

3. **Used in serialization**:
   ```cpp
   template<typename T>
   auto serialize(const T& obj) {
       constexpr size_t max_size = max_serialized_size_v<T>;
       static_buffer<max_size> buffer;
       
       size_t actual_size = serialize_to_unified(obj, buffer.data_unsafe(), max_size);
       buffer.resize(actual_size);  // Set actual size
       
       return buffer;
   }
   ```

### Type Trait Access

Convenient access via type traits:

```cpp
// Compile-time access
template<typename T>
inline constexpr std::size_t max_serialized_size_v = 
    StructLayout<T>::max_packed_size;

template<typename T>
inline constexpr std::size_t base_serialized_size_v = 
    StructLayout<T>::base_packed_size;

// Usage:
static_assert(max_serialized_size_v<Player> == 20);
static_buffer<max_serialized_size_v<SensorData>> buffer;
```

### Optimization: Early Return for Static Types

```cpp
template<typename T>
std::size_t calculate_packed_size(const T& obj) {
    using HMM = StructLayout<T>;
    
    // Compile-time check: if no dynamic fields, return constant
    if constexpr (!HMM::has_variable_fields) {
        return HMM::base_packed_size;
    } else {
        // Dynamic calculation needed
        // ...
    }
}
```

This generates **zero runtime overhead** for fixed-size types.

---

## Size Calculation Cheat Sheet

| Scenario | base_packed_size | max_packed_size | actual_size |
|----------|------------------|-----------------|-------------|
| **All fixed fields** | Σ(field sizes) | Same as base | Same as base (constant) |
| **One dynamic field** | Fixed fields only | base + 4 + capacity*elem_size | base + 4 + size()*elem_size |
| **Multiple dynamic fields** | Fixed + RuntimeOffset | base + Σ(4 + cap_i*elem_i) | base + Σ(4 + size_i*elem_i) |
| **Empty containers** | Fixed + RuntimeOffset | base + Σ(4 + cap_i*elem_i) | base + Σ(4) (just prefixes) |

**Key formulas**:
```
base_packed_size = sizeof(all_fixed_fields) + sizeof(all_runtime_offset_fields)
                   [excludes dynamic field data, but includes their fixed metadata]

max_packed_size = base_packed_size + Σ(length_prefix + capacity * element_size)

actual_size = base_packed_size + Σ(length_prefix + size() * element_size)
```

---

## Validation and Testing

### Compile-Time Checks

```cpp
// Ensure max size is reasonable
static_assert(max_serialized_size_v<T> < 10'000'000, 
              "Suspiciously large max size");

// Ensure base <= max
static_assert(StructLayout<T>::base_packed_size <= 
              StructLayout<T>::max_packed_size);
```

### Runtime Validation

```cpp
// After serialization, verify size bounds
size_t actual = calculate_packed_size(obj);
assert(actual >= StructLayout<T>::base_packed_size);
assert(actual <= StructLayout<T>::max_packed_size);
```

### Test Cases

From `test_serialization.cpp`:

```cpp
TEST_CASE("Size calculations") {
    SECTION("Fixed-size type") {
        Player p{42, 100.0f, 1.0f, 2.0f, 3.0f};
        
        constexpr size_t max_size = max_serialized_size_v<Player>;
        static_assert(max_size == 20);
        
        size_t actual = calculate_packed_size(p);
        REQUIRE(actual == 20);
    }
    
    SECTION("Variable-size type") {
        SensorData data{.timestamp = 123, .sensor_id = 1, .readings = {1.0f, 2.0f}};
        
        constexpr size_t max_size = max_serialized_size_v<SensorData>;
        static_assert(max_size == 416);  // 12 + 4 + 100*4
        
        size_t actual = calculate_packed_size(data);
        REQUIRE(actual == 28);  // 12 + 4 + 2*4
        REQUIRE(actual < max_size);
    }
}
```

---

## Related Documentation

- **Serialization Mechanism**: See `docs/SERIALIZATION_MECHANISM.md` for how sizes are used during serialization
- **Container Handling**: See `docs/CONTAINER_HANDLING.md` for container-specific size calculations
- **StructLayout**: See `include/sertial/core/layout/struct_layout.hpp` for implementation

---

## Summary

**Key Takeaways**:

1. **Two size calculations**: Compile-time max (for buffer allocation) and runtime actual (for transmission)
2. **base_packed_size**: Fixed overhead, always serialized regardless of container contents
3. **max_packed_size**: Worst-case size, used for stack buffer allocation (zero heap guarantee)
4. **actual_size**: Runtime size based on container.size(), always between base and max
5. **Length prefixes**: Each dynamic field adds 4-byte prefix + variable data
6. **No padding**: Struct padding is eliminated in serialized format
7. **Zero overhead**: Fixed-size types have no runtime calculation (compile-time constant)

**Mental Model**:
- Fixed fields → Always serialized at fixed positions
- Dynamic fields → 4-byte length + variable data (0 to capacity elements)
- RuntimeOffset fields → Fixed size but variable position (after dynamic content)
- Padding → Ignored (not serialized)
