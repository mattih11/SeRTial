# Container Implementation Internals

**Deep technical reference on how SeRTial's container system works**

## Documentation

**User Guides**: [USER_GUIDE.md](USER_GUIDE.md) | [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md) | [EXAMPLES.md](EXAMPLES.md) | [SCHEMA_VIEWER.md](SCHEMA_VIEWER.md)  
**Developer Guides**: [ADDING_CONTAINERS.md](ADDING_CONTAINERS.md) | [CONTAINER_INTERNALS.md](CONTAINER_INTERNALS.md)  
**Technical References**: [SERIALIZATION_MECHANISM.md](SERIALIZATION_MECHANISM.md) | [REFLECTOR_BASED_SCHEMA.md](REFLECTOR_BASED_SCHEMA.md) | [SIZE_CALCULATIONS.md](SIZE_CALCULATIONS.md) | [TEMPLATE_PATTERNS.md](TEMPLATE_PATTERNS.md)

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [SerializableContainer Concept](#serializablecontainer-concept)
3. [Span-Based Serialization](#span-based-serialization)
4. [Element Padding](#element-padding)
5. [Multi-Span Containers](#multi-span-containers)
6. [Automatic Metadata Extraction](#automatic-metadata-extraction)
7. [Schema Generation](#schema-generation)
8. [Performance Characteristics](#performance-characteristics)

---

## Architecture Overview

### Design Philosophy

SeRTial's container system follows these principles:

1. **Compile-time everything**: Container properties analyzed at compile time
2. **Zero runtime overhead**: No virtual dispatch, no type erasure
3. **Generic serialization**: Single code path for all containers via spans
4. **Type safety**: Concept-based validation prevents misuse
5. **No manual registration**: Automatic trait extraction

### System Components

```
┌────────────────────────────────────────────────┐
│ SerializableContainer Concept (Validation)     │
└───────────────┬────────────────────────────────┘
                │
                ├──> container_metadata<T>        (Automatic extraction)
                ├──> serialization_view_provider<T> (Memory views)
                └──> container_type_name<T>      (Schema metadata)
                
┌────────────────────────────────────────────────┐
│ StructLayout<T> (Type Analysis)                │
│ - Detects containers in fields                 │
│ - Computes max_packed_size                     │
│ - Generates block execution plan               │
└────────────────────────────────────────────────┘
```

---

## SerializableContainer Concept

### Full Definition

From `include/sertial/containers/container_registration.hpp`:

```cpp
template<typename T>
concept SerializableContainer = requires(const T& c, T& mut_c) {
    // 1. Must have value_type
    typename T::value_type;
    
    // 2. Compile-time maximum capacity
    { T::max_size_v } -> std::convertible_to<std::size_t>;
    
    // 3. Runtime size query
    { c.size() } -> std::same_as<std::size_t>;
    
    // 4. Contiguous data access (const)
    { c.data() } -> std::convertible_to<const typename T::value_type*>;
    
    // 5. Mutable data access (for deserialization)
    { mut_c.data_unsafe() } -> std::convertible_to<typename T::value_type*>;
    
    // 6. Unsafe size setter (for deserialization)
    { mut_c.set_size_unsafe(std::size_t{}) } -> std::same_as<void>;
} && 
// 7. Prevent nested containers
!requires { typename T::value_type::max_size_v; };
```

### Requirement Rationale

**1. `value_type`**: Element type for trait extraction and span construction

**2. `max_size_v`**: Compile-time capacity for buffer sizing
- **Naming**: `max_size_v` (not `max_size`) to distinguish from `max_size()` method
- **Type**: Must be `static constexpr std::size_t`
- **Purpose**: Enables compile-time `max_packed_size` calculation

**3. `size()`**: Runtime element count
- **Return type**: Must be exactly `std::size_t` (not convertible)
- **Purpose**: Determines actual serialized data length

**4. `data()`**: Const pointer to element storage
- **Contiguous**: Elements must be contiguous in memory (memcpy requirement)
- **Return type**: `const T*` (convertible, allows custom pointer types)

**5. `data_unsafe()`**: Mutable pointer for deserialization
- **Purpose**: Write deserialized data directly to storage
- **No validation**: Deserialization knows data is valid
- **Performance**: Avoids unnecessary bounds checking

**6. `set_size_unsafe()`**: Direct size modification
- **Purpose**: Update size after deserialization write
- **No validation**: Size already validated during deserialization
- **Efficiency**: Avoids resize logic (reallocation, initialization)

**7. Nested container prevention**: `value_type` cannot itself be a container
- **Limitation**: Simplifies serialization, avoids complexity
- **Detection**: Checks if `value_type` has `max_size_v` member
- **Alternatives**: Use POD structs or flatten structure

---

## Span-Based Serialization

### Core Abstraction

All containers serialize via **memory spans** (std::span):

```cpp
template<SerializableContainer T>
struct serialization_view_provider {
    static constexpr auto get_spans(const T& container) {
        using element_type = typename T::value_type;
        return std::array<std::span<const element_type>, 2>{
            std::span{container.data(), container.size()},  // Primary span
            std::span{}                                      // Secondary (empty)
        };
    }
    
    static constexpr std::size_t span_count = 1;
};
```

### Why Spans?

**Benefits**:
1. **Zero-copy**: Non-owning reference to memory
2. **Type-safe**: Carries element type and count
3. **Generic**: Works for all contiguous containers
4. **Bounds-checked**: Size included (unlike raw pointers)
5. **Standard**: C++20 `std::span` is widely supported

**Serialization Loop** (from `io/unified_binary.hpp`):
```cpp
auto spans = sertial::get_serialization_spans(container);

// Write length prefix
uint32_t length = container.size();
std::memcpy(dest, &length, sizeof(uint32_t));
dest += sizeof(uint32_t);

// Write data (1-2 spans)
for (const auto& span : spans) {
    if (span.empty()) continue;  // Skip empty spans
    std::memcpy(dest, span.data(), span.size_bytes());
    dest += span.size_bytes();
}
```

### Array Format

Fixed-size array: **2 spans maximum**
- `span[0]`: Primary data region (always present)
- `span[1]`: Secondary region (empty for contiguous containers)

**Advantages**:
- Stack-allocated (no heap allocation)
- Compile-time size known
- Empty spans have zero cost (if-check skips)

---

## Element Padding

### Key Principle

Container elements are **serialized as-is** from C arrays using memcpy:

```cpp
std::size_t data_size = field.size() * sizeof(T);
std::memcpy(dest, field.data(), data_size);  // Direct copy from C array
```

### Implications

**Internal padding IS serialized**:
```cpp
struct PaddedElement {
    uint8_t a;   // 1 byte
    // 3 bytes padding (compiler alignment)
    uint32_t b;  // 4 bytes
};  // sizeof = 8 bytes (includes padding)

fixed_vector<PaddedElement, 10> vec = {{1, 100}, {2, 200}};

// Serialized: [length:4][elem0:8][elem1:8] = 20 bytes
// Padding is included in each 8-byte element
```

**No inter-element padding**:
C++ guarantees array elements are contiguous (no gaps between elements).

**Trade-offs**:
- **Fast**: Single memcpy per container
- **Simple**: No element-by-element recursion
- **Wasteful**: Padding bytes transmitted (unavoidable with memcpy)

### Size Calculations

```cpp
// Container maximum size (compile-time)
constexpr std::size_t max_size = sizeof(uint32_t) +              // Length prefix
                                  N * sizeof(T);                  // N elements (with padding)

// Container actual size (runtime)
std::size_t actual_size = sizeof(uint32_t) +                     // Length prefix
                          container.size() * sizeof(T);           // size() elements
```

### Optimization: Reorder Struct Fields

**Bad layout** (8 bytes wasted):
```cpp
struct BadElement {
    uint8_t  a;  // 1 byte + 3 padding
    uint32_t b;  // 4 bytes
    uint8_t  c;  // 1 byte + 3 padding
    uint32_t d;  // 4 bytes
};  // sizeof = 16 bytes

fixed_vector<BadElement, 100> vec;
// Max size: 4 + 100*16 = 1604 bytes (800 bytes padding!)
```

**Good layout** (no padding):
```cpp
struct GoodElement {
    uint32_t b;  // 4 bytes
    uint32_t d;  // 4 bytes
    uint8_t  a;  // 1 byte
    uint8_t  c;  // 1 byte
};  // sizeof = 10 bytes

fixed_vector<GoodElement, 100> vec;
// Max size: 4 + 100*10 = 1004 bytes (600 bytes saved!)
```

---

## Multi-Span Containers

### When Multiple Spans are Needed

Containers with **non-contiguous memory** require multiple spans:

**Example: RingBuffer with wrap-around**
```
Capacity: 8
Logical order:  [5, 6, 7, 8, 9]  (5 elements)
Physical memory: [8, 9, _, _, _, 5, 6, 7]
                  ^head         ^tail

span[0]: data[tail:capacity) = [5, 6, 7]  (tail to end)
span[1]: data[0:head)        = [8, 9]     (start to head)
```

### RingBuffer Specialization

From `container_registration.hpp`:

```cpp
template<typename T, std::size_t N>
struct serialization_view_provider<RingBuffer<T, N>> {
    static constexpr auto get_spans(const RingBuffer<T, N>& rb) {
        using SpanType = std::span<const T>;
        
        if (rb.is_wrapped()) {
            // Two spans: tail→end, start→head
            std::size_t tail = rb.tail_index();
            std::size_t head = rb.head_index();
            
            return std::array<SpanType, 2>{
                SpanType{rb.data_unsafe() + tail, N - tail},  // Tail to end
                SpanType{rb.data_unsafe(), head}              // Start to head
            };
        } else {
            // Single span: tail→head (contiguous)
            std::size_t tail = rb.tail_index();
            
            return std::array<SpanType, 2>{
                SpanType{rb.data_unsafe() + tail, rb.size()},
                SpanType{}  // Empty
            };
        }
    }
    
    static constexpr std::size_t span_count = 2;  // May use both
};
```

### Generic Serialization Handles Both Cases

```cpp
// Same code works for 1-span (fixed_vector) and 2-span (RingBuffer)
auto spans = get_serialization_spans(container);
for (const auto& span : spans) {
    if (span.empty()) continue;  // Skip empty spans
    std::memcpy(dest, span.data(), span.size_bytes());
    dest += span.size_bytes();
}
```

**Key insight**: Empty spans have zero cost - if-check skips, no memcpy called.

---

## Automatic Metadata Extraction

### container_metadata Template

Once a type satisfies `SerializableContainer`, all metadata is automatically available:

```cpp
template<SerializableContainer T>
struct container_metadata {
    using element_type = typename T::value_type;
    
    static constexpr std::size_t max_size = T::max_size_v;
    static constexpr std::size_t element_size = sizeof(element_type);
    
    static constexpr bool is_variable_length = true;
    static constexpr bool is_fixed_capacity = true;
    static constexpr bool is_serializable = true;
};
```

### Convenience Aliases

```cpp
// Check if type is a container
template<typename T>
inline constexpr bool is_serializable_container_v = SerializableContainer<T>;

// Get element type
template<SerializableContainer T>
using container_element_t = typename container_metadata<T>::element_type;

// Get capacity
template<SerializableContainer T>
inline constexpr std::size_t container_max_size_v = container_metadata<T>::max_size;

// Get element size
template<SerializableContainer T>
inline constexpr std::size_t container_element_size_v = container_metadata<T>::element_size;
```

### Usage in StructLayout

```cpp
template<typename T>
class StructLayout {
    // Analyze fields via reflect-cpp
    using NT = rfl::named_tuple_t<T>;
    
    // For each field, check if it's a container
    template<typename Field>
    static constexpr bool is_container_field() {
        using FieldType = /* extract type from Field */;
        return SerializableContainer<FieldType>;
    }
    
    // Extract capacity if container
    template<typename Field>
    static constexpr std::size_t field_max_size() {
        using FieldType = /* extract type */;
        if constexpr (SerializableContainer<FieldType>) {
            return container_max_size_v<FieldType>;
        } else {
            return 1;  // Non-container
        }
    }
};
```

---

## Schema Generation

### Container Metadata in JSON

Schema export includes container-specific fields:

```json
{
  "field_names": ["timestamp", "readings"],
  "field_sizes": [8, 412],
  "is_variable_length": [false, true],
  "max_elements": [1, 100],
  "element_sizes": [8, 4],
  "container_types": ["none", "fixed_vector"]
}
```

### Type Name Registration

Specify human-readable names for schema tools:

```cpp
template<typename T, std::size_t N>
struct container_type_name<fixed_vector<T, N>> {
    static constexpr const char* value = "fixed_vector";
};

template<std::size_t N>
struct container_type_name<fixed_string<N>> {
    static constexpr const char* value = "fixed_string";
};

template<typename T, std::size_t N>
struct container_type_name<RingBuffer<T, N>> {
    static constexpr const char* value = "ring_buffer";
};
```

### Reflector Integration

Container metadata is exposed via `rfl::Reflector`:

```cpp
// In container reflectors (containers/reflectors.hpp)
template<typename T, std::size_t N>
struct Reflector<fixed_vector<T, N>> {
    struct ReflType {
        rfl::Rename<"type", std::string> type = "fixed_vector";
        rfl::Rename<"element_type", std::string> element_type = type_name<T>();
        rfl::Rename<"max_size", std::size_t> max_size = N;
        rfl::Rename<"element_size", std::size_t> element_size = sizeof(T);
    };
    
    static auto to_meta(const fixed_vector<T, N>&) {
        return ReflType{};
    }
};
```

**See**: [REFLECTOR_BASED_SCHEMA.md](REFLECTOR_BASED_SCHEMA.md) for reflector architecture details.

---

## Performance Characteristics

### Compile-Time Operations

**Zero runtime cost**:
- Container detection (`SerializableContainer<T>`)
- Metadata extraction (`container_metadata<T>`)
- Max size calculation (`container_max_size_v<T>`)
- Span count determination (`span_count`)

**All resolved at compile time** via template metaprogramming.

### Serialization Performance

**fixed_vector<T, 100> with 50 elements**:

```
Operation              Time      Notes
-----------------------------------------------------------------
Get spans              0 ns      Compile-time dispatch
Write length prefix    ~1 ns     4-byte memcpy
Write elements         ~10 ns    Single 200-byte memcpy (50*4)
Total                  ~11 ns    O(size), independent of capacity
```

**RingBuffer<T, 100> with 50 elements (wrapped)**:

```
Operation              Time      Notes
-----------------------------------------------------------------
Get spans              ~2 ns     Check wrap + compute indices
Write length prefix    ~1 ns     4-byte memcpy
Write span[0]          ~5 ns     memcpy tail→end
Write span[1]          ~5 ns     memcpy start→head
Total                  ~13 ns    O(size), slight overhead vs contiguous
```

### Memory Access Pattern

**Cache-friendly**:
- Sequential memory access (contiguous arrays)
- Single memcpy per span (no element-by-element)
- Predictable access pattern (prefetcher-friendly)

**No allocations**:
- Spans are stack-allocated (std::array<std::span, 2>)
- No temporary buffers
- Direct memory-to-memory copy

---

## Summary

**Key architectural decisions**:

1. **Concept-based validation**: Compile-time container detection
2. **Span-based serialization**: Generic code for all containers
3. **Element padding included**: Fast memcpy at cost of wire efficiency
4. **Multi-span support**: Handles non-contiguous storage (RingBuffer)
5. **Automatic metadata**: No manual trait specializations needed
6. **Schema integration**: Reflector-based metadata export

**Trade-offs**:

| Decision | Benefit | Cost |
|----------|---------|------|
| Concept-based | Type safety, automatic detection | C++20 required |
| Span-based | Generic code, zero-copy | Requires contiguous storage |
| Include padding | Fast (single memcpy) | Larger wire format |
| Multi-span | Supports circular buffers | Slight complexity |

---

## Next Steps

- **Add custom containers**: [ADDING_CONTAINERS.md](ADDING_CONTAINERS.md)
- **Understand serialization**: [SERIALIZATION_MECHANISM.md](SERIALIZATION_MECHANISM.md)
- **Learn size computation**: [SIZE_CALCULATIONS.md](SIZE_CALCULATIONS.md)
- **Study reflector system**: [REFLECTOR_BASED_SCHEMA.md](REFLECTOR_BASED_SCHEMA.md)

---

**Questions?** Open an issue: [https://github.com/mattih11/SeRTial](https://github.com/mattih11/SeRTial)
