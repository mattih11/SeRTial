# GitHub Copilot Instructions for SeRTial

## Project Overview

**SeRTial** (Serialization for Real-Time) is a C++20 binary serialization library built on compile-time reflection (reflect-cpp). It provides zero-allocation, high-performance message serialization for real-time and embedded systems.

**Current Status**: Production-ready core with fixed containers (fixed_vector, fixed_string, static_buffer)  
**Development**: Exploring RingBuffer integration for circular buffer serialization

### Core Philosophy
- **Compile-time everything**: Type analysis, size computation, and layout mapping happen at compile time
- **Zero allocation mandate**: Stack-only buffers with compile-time max size - NO heap allocation in serialization paths
- **Real-time safe**: Deterministic execution, no exceptions in hot paths, bounded execution time
- **Template metaprogramming**: Heavy use of C++20 concepts, SFINAE, and constexpr for compile-time dispatch
- **Simple user API**: Complex type trait machinery hidden - users see only `serialize(obj)` and `deserialize<T>(data)`

## Architecture

### Unified Block-Based Serialization
SeRTial uses **HybridMemoryMap** to analyze struct layout at compile time and generate an execution plan:

```cpp
struct Message {
    uint32_t id;                    // Fixed block
    fixed_vector<float, 100> data;  // Dynamic block (runtime size)
    uint64_t timestamp;             // RuntimeOffset block (position varies)
};

// Compile-time analysis:
// - Block 0: Fixed[offset=0, size=4]  (id)
// - Block 1: Dynamic[field=1, capacity=100, elem_size=4]  (data)
// - Block 2: RuntimeOffset[offset=?, size=8]  (timestamp)
```

**Block Types:**
1. **Fixed**: Contiguous fixed-size fields before any dynamic field → single memcpy
2. **Padding**: Alignment gaps in struct (skipped during serialization)
3. **Dynamic**: Variable-size containers (4-byte length prefix + runtime data)
4. **RuntimeOffset**: Fixed-size fields after dynamic content (offset computed at runtime)

### Type Trait System
Three layers of type introspection:

1. **Container Detection** (`traits/container_detection.hpp`):
   - `is_fixed_container_v<T>`: Is T a fixed_vector/fixed_string?
   - `fixed_container_capacity_v<T>`: Compile-time capacity
   - `fixed_container_element_size_v<T>`: Element byte size

2. **Container Traits** (`containers/container_traits.hpp`):
   - `is_fixed_capacity<T>`: Has fixed max capacity?
   - `container_category<T>`: FixedCapacity/DynamicCapacity/NotAContainer
   - `fixed_capacity_traits<T>`: Extract element_type and max_size

3. **Memory Map Traits** (`core/traits/memory_map.hpp`):
   - `is_variable_length_field<T>`: Needs runtime length tracking?
   - `variable_length_element_size<T>`: Element size for dynamic sizing
   - `variable_length_max_elements<T>`: Maximum element count

**Current Issue**: Container registration is spread across 3 files. Goal: consolidate into single registration point.

### Serialization Flow

```cpp
// User code (simple API)
auto buffer = serialize(msg);  // Zero heap allocation
auto restored = deserialize<Message>(buffer.view());

// Behind the scenes:
// 1. HybridMemoryMap<Message> analyzes layout at compile time
// 2. Generates block execution plan (fixed → dynamic → runtime_offset)
// 3. Computes max_packed_size for stack buffer allocation
// 4. serialize_to_unified() executes block plan with memcpy operations
// 5. Returns static_buffer<max_packed_size> with actual size
```

## Code Style & Conventions

### Real-Time Constraints

**NEVER use in serialization paths:**
```cpp
// ❌ FORBIDDEN in real-time code:
new / delete
malloc / free / realloc
std::vector::push_back()  // May allocate
std::string operations    // May allocate
std::cout in loops        // Blocking I/O
throw exceptions          // Use std::optional/expected
Virtual functions         // Prefer compile-time dispatch
std::mutex                // Use lock-free atomics if needed
```

**DO use:**
```cpp
// ✅ REAL-TIME SAFE:
std::array<T, N>              // Fixed-size, stack
static_buffer<N>              // SeRTial's bounded buffer
fixed_vector<T, N>            // Capacity-bounded, no allocation
fixed_string<N>               // Bounded string, no allocation
std::atomic<T>                // Lock-free operations
constexpr / consteval         // Compile-time computation
std::optional<T>              // Error handling without exceptions
std::span<T>                  // Non-owning view
```

### Template Metaprogramming

SeRTial relies heavily on **compile-time type analysis**:

```cpp
// Use concepts for constraints
template<typename T>
concept FixedCapacityContainer = requires {
    typename T::value_type;
    { T::max_size } -> std::convertible_to<std::size_t>;
    requires std::is_same_v<decltype(std::declval<T>().capacity()), std::size_t>;
};

// SFINAE for conditional behavior
template<typename T>
static constexpr bool has_variable_fields = 
    detail::struct_has_fixed_containers<T>();

if constexpr (has_variable_fields) {
    // Compile-time branch - zero runtime cost
    return calculate_runtime_size(obj);
} else {
    return base_packed_size;  // Compile-time constant
}

// Variadic templates for field analysis
template<typename... Fields>
constexpr auto analyze_fields(rfl::NamedTuple<Fields...>*) {
    return std::array<FieldInfo, sizeof...(Fields)>{
        analyze_field<Fields>()...
    };
}
```

### Type Traits Pattern

All type analysis follows a consistent pattern:

```cpp
// 1. Primary template (default case)
template<typename T>
struct trait_name : std::false_type {};

// 2. Specializations for known types
template<typename T, std::size_t N>
struct trait_name<fixed_vector<T, N>> : std::true_type {};

template<std::size_t N>
struct trait_name<fixed_string<N>> : std::true_type {};

// 3. Convenience variable template
template<typename T>
inline constexpr bool trait_name_v = trait_name<T>::value;

// 4. Usage in compile-time dispatch
if constexpr (trait_name_v<FieldType>) {
    // Handle special case
}
```

## Directory Structure

```
SeRTial/
├── include/sertial/
│   ├── sertial.hpp              # Main public header
│   ├── message.hpp              # Message<T> wrapper (legacy, prefer direct serialize)
│   ├── containers/              # Bounded containers
│   │   ├── fixed_vector.hpp     # Stack-allocated vector (capacity-bounded)
│   │   ├── fixed_string.hpp     # Stack-allocated string
│   │   ├── static_buffer.hpp    # Fixed-capacity byte buffer
│   │   ├── ring_buffer.hpp      # Circular buffer [NEW - integration pending]
│   │   └── container_traits.hpp # Container categorization traits
│   ├── core/                    # Type analysis engine
│   │   ├── concepts.hpp         # C++20 concepts
│   │   ├── endian.hpp           # Endianness handling
│   │   ├── traits.hpp           # Main traits include
│   │   └── traits/              # Trait implementation
│   │       ├── bounded.hpp      # Compile-time max size computation
│   │       ├── hybrid_memory_map.hpp  # Block-based layout analysis
│   │       ├── memory_map.hpp   # Field offset/size analysis
│   │       ├── padding.hpp      # Padding detection
│   │       ├── size_category.hpp # Static/Dynamic/Trailing categorization
│   │       └── type_info.hpp    # Comprehensive type traits
│   ├── io/                      # Serialization implementation
│   │   └── unified_binary.hpp   # Block-based serialize/deserialize
│   ├── integration/             # External integrations
│   │   ├── message_collection.hpp # Multi-message schemas
│   │   ├── runtime_test.hpp     # Serialize/deserialize validation
│   │   └── schema_generator.hpp # JSON schema export
│   ├── traits/                  # Detection utilities
│   │   └── container_detection.hpp # Fixed container detection
│   └── debug/                   # Development tools
│       └── print_utils.hpp      # Debug output utilities
├── examples/                    # Usage demonstrations
│   ├── serialization_example.cpp # Comprehensive API showcase
│   ├── schema_example.cpp       # Schema generation
│   └── defines/                 # Example message types
├── test/                        # Unit tests
│   ├── test_foundation.cpp      # Basic type traits
│   ├── test_serialization.cpp   # Serialization correctness
│   ├── test_hybrid_binary.cpp   # HybridMemoryMap validation
│   └── test_ring_buffer.cpp     # RingBuffer unit tests
├── scripts/                     # Utilities
│   ├── visualize_schema.py      # CLI schema viewer
│   └── visualize_schema_gui.py  # GUI schema viewer
└── CMakeLists.txt              # Build configuration
```

## Documentation Strategy

### Active Documentation
- **README.md**: User-facing guide with API examples, quick start, and design rationale
- **Code comments**: Doxygen-style for public APIs, implementation notes for complex metaprogramming

### When to Document
- **New containers**: Document integration points (3 trait files currently, target: 1 file)
- **Type trait changes**: Explain compile-time implications and usage patterns
- **Serialization format changes**: Update wire format specifications
- **Performance implications**: Note any changes to allocation/timing guarantees

### Documentation Style
```cpp
/**
 * @brief Stack-allocated vector with fixed capacity
 * 
 * Unlike std::vector, fixed_vector never allocates heap memory. When full,
 * push_back() triggers an assertion in debug builds. Use for real-time
 * contexts where allocations are forbidden.
 * 
 * @tparam T Element type (must be copyable or movable)
 * @tparam N Maximum capacity (compile-time constant)
 * 
 * @note Capacity is NOT serialized - only size() elements are written
 * @note Serialization format: [length:4][elements:size()*sizeof(T)]
 * 
 * @example
 * fixed_vector<float, 100> data;
 * data.push_back(1.5f);
 * auto buf = serialize(data);  // Writes 4 + 1*4 = 8 bytes
 */
template<typename T, std::size_t N>
class fixed_vector { ... };
```

## Common Patterns

### Element Padding in Containers

**Key principle**: Container elements are serialized **as-is** from C arrays using memcpy.

```cpp
// Serialization copies elements with their natural C++ layout
std::size_t data_size = field.size() * sizeof(T);
std::memcpy(dest, field.data(), data_size);  // Direct copy from C array
```

**Implications:**
- **Internal padding IS serialized**: Element `sizeof(T)` includes padding
- **No inter-element padding**: C++ guarantees array elements are contiguous
- **Fast**: Single memcpy per container, no element-by-element recursion
- **Slightly wasteful**: Padding bytes transmitted (unavoidable with memcpy)

**Example:**
```cpp
struct PaddedElement {
    uint8_t a;   // 1 byte
    // 3 bytes padding
    uint32_t b;  // 4 bytes
};  // sizeof = 8 bytes (includes padding)

fixed_vector<PaddedElement, 10> vec = {{1, 100}, {2, 200}};
// Serialized: [length:4][elem0:8][elem1:8] = 20 bytes
//              padding is included in each 8-byte element

// Size calculations:
max_size = 4 + 10 * sizeof(PaddedElement) = 4 + 10*8 = 84 bytes
actual_size = 4 + 2 * sizeof(PaddedElement) = 4 + 2*8 = 20 bytes
```

**Current Limitation**: **Nested containers NOT supported**
```cpp
// ❌ NOT SUPPORTED:
fixed_vector<fixed_vector<float, 5>, 10> matrix;
// Problem: Inner fixed_vector has runtime state (size_) that would be serialized

// ✅ ALTERNATIVES:
// 1. Flatten: fixed_vector<float, 50> data  (10*5 elements)
// 2. Use std::array: std::array<fixed_vector<float, 5>, 10>  (always 10 rows)
// 3. Use POD elements: fixed_vector<Point, 1000> points  (Point is simple struct)
```

### Compile-Time Size Computation
```cpp
// Container must expose max_size at compile time
template<typename T>
constexpr std::size_t compute_max_size() {
    if constexpr (is_fixed_container_v<T>) {
        return sizeof(uint32_t) +  // Length prefix
               fixed_container_capacity_v<T> * 
               fixed_container_element_size_v<T>;
    } else {
        return sizeof(T);
    }
}
```

### Block Execution Pattern
```cpp
// HybridMemoryMap generates block execution plan
for (const auto& descriptor : execution_order) {
    switch (descriptor.type) {
        case BlockType::Fixed:
            // Direct memcpy of contiguous fields
            std::memcpy(dest + dst_offset, src + src_offset, size);
            break;
            
        case BlockType::Dynamic:
            // Length prefix + variable data
            uint32_t len = container.size();
            std::memcpy(dest, &len, 4);
            std::memcpy(dest + 4, container.data(), len * elem_size);
            break;
            
        case BlockType::RuntimeOffset:
            // Fixed field after dynamic content (offset varies)
            std::memcpy(dest + runtime_offset, src + src_offset, size);
            break;
            
        case BlockType::Padding:
            // Skip - not serialized
            break;
    }
}
```

### Error Handling Pattern
```cpp
// Prefer std::optional over exceptions
std::optional<T> deserialize(std::span<const std::byte> data) {
    if (data.size() < sizeof(uint32_t)) {
        return std::nullopt;  // Not: throw std::runtime_error
    }
    
    T result;
    // ... deserialization ...
    return result;
}

// Or use explicit error types
enum class SerializationError {
    BufferTooSmall,
    InvalidLength,
    CorruptedData
};

std::expected<T, SerializationError> deserialize_safe(std::span<const std::byte> data);
```

## Testing Guidelines

### Unit Test Focus
- **Compile-time correctness**: `static_assert` for type traits
- **Serialization round-trip**: Verify serialize→deserialize preserves data
- **Size calculations**: Actual serialized size matches predicted size
- **Boundary conditions**: Empty containers, max capacity, wrap-around (RingBuffer)

### Test Structure
```cpp
TEST_CASE("fixed_vector serialization") {
    // Arrange
    fixed_vector<uint32_t, 10> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    
    // Compile-time checks
    static_assert(is_fixed_container_v<decltype(vec)>);
    static_assert(fixed_container_capacity_v<decltype(vec)> == 10);
    
    // Act
    auto buffer = serialize(vec);
    auto restored = deserialize<decltype(vec)>(buffer.view());
    
    // Assert
    REQUIRE(restored.has_value());
    REQUIRE(restored->size() == 3);
    REQUIRE((*restored)[0] == 1);
    REQUIRE((*restored)[1] == 2);
    REQUIRE((*restored)[2] == 3);
    
    // Size validation
    size_t expected_size = sizeof(uint32_t) + 3 * sizeof(uint32_t);
    REQUIRE(buffer.size() == expected_size);
}
```

### Performance Tests
```cpp
// Validate zero-allocation guarantee
TEST_CASE("serialization is allocation-free") {
    // Set up allocation guard (platform-specific)
    AllocationGuard guard;
    
    Player player{42, 100.0f, 1.0f, 2.0f, 3.0f};
    auto buffer = serialize(player);  // Must not allocate
    
    REQUIRE(guard.allocation_count() == 0);
}

// Measure serialization performance
BENCHMARK("serialize Player (20 bytes)") {
    Player p{42, 100.0f, 1.0f, 2.0f, 3.0f};
    return serialize(p);
};
```

## Integration Patterns

### Adding a New Container Type

**Current State**: Registration spread across 3 files (container_detection.hpp, container_traits.hpp, memory_map.hpp)  
**Target State**: Single registration point (TBD - pending simplification)

**Current Process (to be simplified):**

1. **Implement container** (`containers/my_container.hpp`):
```cpp
template<typename T, std::size_t N>
class MyContainer {
public:
    using value_type = T;
    static constexpr std::size_t max_size = N;
    
    constexpr std::size_t size() const { return size_; }
    constexpr std::size_t capacity() const { return N; }
    const T* data() const { return data_.data(); }
    
    // For serialization support:
    T* data_unsafe() { return data_.data(); }  // Direct access
    void set_size_unsafe(std::size_t n) { size_ = n; }  // Skip validation
    
private:
    std::array<T, N> data_;
    std::size_t size_{0};
};
```

2. **Register in container_detection.hpp**:
```cpp
#include "../containers/my_container.hpp"

template<typename T, std::size_t N>
struct is_fixed_container_impl<MyContainer<T, N>> : std::true_type {};

template<typename T, std::size_t N>
struct fixed_container_capacity<MyContainer<T, N>> {
    static constexpr std::size_t value = N;
};

template<typename T, std::size_t N>
struct fixed_container_element_size<MyContainer<T, N>> {
    static constexpr std::size_t value = sizeof(T);
};
```

3. **Register in container_traits.hpp**:
```cpp
#include "my_container.hpp"

template<typename T, std::size_t N>
struct is_fixed_capacity<MyContainer<T, N>> : std::true_type {};

template<typename T, std::size_t N>
struct fixed_capacity_traits<MyContainer<T, N>> {
    using element_type = T;
    static constexpr std::size_t max_size = N;
};

template<typename T, std::size_t N>
struct container_category<MyContainer<T, N>> {
    static constexpr ContainerCategory value = ContainerCategory::FixedCapacity;
};
```

4. **Register in memory_map.hpp**:
```cpp
template<typename T, std::size_t N>
struct is_variable_length_field<MyContainer<T, N>> : std::true_type {};

template<typename T, std::size_t N>
struct variable_length_element_size<MyContainer<T, N>> {
    static constexpr std::size_t value = sizeof(T);
};

template<typename T, std::size_t N>
struct variable_length_max_elements<MyContainer<T, N>> {
    static constexpr std::size_t value = N;
};
```

### Using Bounded Containers in Structs

```cpp
// Define message with bounded containers
struct SensorData {
    uint64_t timestamp;
    uint32_t sensor_id;
    fixed_vector<float, 100> readings;  // Max 100 readings
    fixed_string<64> location;          // Max 64 chars
};

// Automatic serialization
SensorData data{
    .timestamp = 1234567890,
    .sensor_id = 42,
    .readings = {1.0f, 2.0f, 3.0f},
    .location = "Lab-A"
};

// Zero-allocation serialize
auto buffer = serialize(data);  // Stack-allocated buffer

// Compile-time size analysis
using HMM = HybridMemoryMap<SensorData>;
static_assert(HMM::has_variable_fields);
static_assert(HMM::base_packed_size == 16);  // timestamp + sensor_id
static_assert(HMM::max_packed_size == 16 + 4 + 400 + 4 + 64);  // Worst case
```

### Buffer Management

**When to use each buffer type:**

- **`std::array<T, N>`**: Fixed-size, simple data
  ```cpp
  std::array<uint32_t, 10> ids;  // Always 10 elements
  ```

- **`static_buffer<N>`**: Binary data with variable size
  ```cpp
  static_buffer<1024> buf;
  buf.resize(actual_size);  // Size varies, capacity fixed
  ```

- **`fixed_vector<T, N>`**: Dynamic-size collection with bound
  ```cpp
  fixed_vector<float, 100> samples;
  samples.push_back(1.5f);  // Size grows, max 100
  ```

- **`fixed_string<N>`**: Text with max length
  ```cpp
  fixed_string<256> name = "Player1";
  ```

- **`RingBuffer<T, N>`**: Circular history (in development)
  ```cpp
  RingBuffer<Message, 100> history;
  history.push_back(msg);  // Overwrites oldest when full
  ```

**Avoid in real-time code:**
```cpp
// ❌ DON'T USE:
std::vector<T>      // Heap allocation
std::string         // Heap allocation
std::deque<T>       // Heap allocation
std::list<T>        // Heap allocation per element
```

## When Suggesting Code

**DO:**
- Use compile-time type analysis (concepts, SFINAE, if constexpr)
- Prefer `constexpr` over runtime computation
- Use stack-allocated buffers (std::array, static_buffer, fixed_vector)
- Document real-time safety in comments
- Leverage type traits for generic code
- Use `std::optional` for error handling
- Make the simple case trivial - hide complexity behind clean APIs
- Validate at compile time with `static_assert`

**DON'T:**
- Suggest heap allocation (new/malloc/vector::push_back) in hot paths
- Use exceptions in real-time code
- Add runtime overhead for type checking (use compile-time dispatch)
- Ignore const-correctness
- Break zero-allocation guarantees
- Use blocking I/O in serialization paths
- Expose metaprogramming complexity to users

## Design Principles

### Compile-Time Guarantee
If it can be validated at compile time, it MUST be validated at compile time:

```cpp
// ✅ GOOD: Compile-time check
template<typename T>
auto serialize(const T& obj) {
    static_assert(is_reflectable_v<T>, "Type must be reflectable");
    static_assert(max_serialized_size_v<T> < 10'000'000,
                  "Suspiciously large max size - check for unbounded types");
    // ...
}

// ❌ BAD: Runtime check for compile-time property
if (!is_reflectable(obj)) {
    throw std::runtime_error("Type not reflectable");
}
```

### Zero-Allocation Mandate
NO heap allocations in serialization/deserialization:

```cpp
// ✅ GOOD: Stack buffer with compile-time size
template<typename T>
auto serialize(const T& obj) {
    constexpr std::size_t max_size = max_serialized_size_v<T>;
    static_buffer<max_size> buffer;
    // Serialize into buffer...
    return buffer;
}

// ❌ BAD: Dynamic allocation
std::vector<std::byte> buffer;
buffer.resize(calculate_size(obj));  // ALLOCATION!
```

### Type Safety
Leverage C++20 type system to catch errors early:

```cpp
// ✅ GOOD: Concept-based constraints
template<BoundedSerializable T>
auto serialize(const T& obj) { /* ... */ }

// Attempting to serialize unbounded type:
std::vector<int> vec;
serialize(vec);  // Compile error: vector not BoundedSerializable

// ❌ BAD: Runtime check
if (!is_bounded(obj)) {
    throw std::runtime_error("Type not bounded");
}
```

## Performance Considerations

### Hot Path Optimization
Serialization must be deterministic and fast:

1. **Avoid branching**: Use compile-time dispatch (`if constexpr`)
2. **Minimize copies**: Use `std::span` for views, not copies
3. **Batch memcpy**: HybridMemoryMap coalesces consecutive fields
4. **Cache-friendly**: Sequential memory access patterns

```cpp
// ✅ GOOD: Compile-time dispatch
if constexpr (is_fixed_container_v<T>) {
    return serialize_dynamic(obj);
} else {
    return serialize_static(obj);
}

// ❌ BAD: Runtime polymorphism
if (obj.is_dynamic()) {  // Virtual call
    return serialize_dynamic(obj);
}
```

### Memory Layout
Pay attention to struct padding for optimal packing:

```cpp
// ❌ BAD: 8 bytes of padding
struct BadLayout {
    uint8_t  a;  // 1 byte + 3 padding
    uint32_t b;  // 4 bytes
    uint8_t  c;  // 1 byte + 3 padding
    uint32_t d;  // 4 bytes
};  // Total: 16 bytes (8 wasted)

// ✅ GOOD: No padding
struct GoodLayout {
    uint32_t b;  // 4 bytes
    uint32_t d;  // 4 bytes
    uint8_t  a;  // 1 byte
    uint8_t  c;  // 1 byte
};  // Total: 10 bytes (packed to 10)
```

SeRTial automatically eliminates padding in serialized format, but awareness helps.

## Zero-Copy with std::span

SeRTial extensively uses `std::span<T>` for **zero-copy, non-owning views**:

### Why std::span?

```cpp
// ❌ BAD: Copy-heavy, container-specific
std::vector<std::byte> get_buffer();           // Returns copy
void send(const std::vector<std::byte>& v);    // Tied to vector

// ✅ GOOD: Zero-copy, container-agnostic
static_buffer<N> get_buffer();                 // Stack-allocated
    └─> .view() returns std::span              // Zero-copy view
void send(std::span<const std::byte> data);    // Accepts any contiguous buffer
```

### Common Patterns

```cpp
// Serialization output
auto buffer = serialize(msg);
std::span<const std::byte> view = buffer.view();  // Zero-copy
socket.send(view.data(), view.size());            // Pass to I/O

// Deserialization input
std::optional<T> deserialize(std::span<const std::byte> data);

// Works with all container types:
static_buffer<1024> buf;
deserialize<T>(buf.view());                    // ✓

std::array<std::byte, 100> arr;
deserialize<T>(std::span{arr});                // ✓

std::vector<std::byte> vec;  // If you must use heap
deserialize<T>(std::span{vec});                // ✓
```

### Benefits
- **Zero-copy**: Non-owning reference, no data duplication
- **Type-safe**: Carries size information (unlike raw pointers)
- **Flexible**: Works with array, vector, static_buffer, raw pointers
- **Bounds-checked**: Size available at runtime
- **Modern C++20**: Standard library, no external dependencies

## Schema Viewer Compatibility

SeRTial maintains strict compatibility with visualization tools:

### Design Principles

1. **Stay close to compile-time data**: Minimal abstraction layers
2. **Use rfl::json::write**: Leverage reflect-cpp's JSON serialization directly
3. **Exact data format**: Export compile-time structures as-is
4. **Shared Python logic**: Common rendering mechanism for CLI and GUI viewers

### Schema Export

```cpp
// Export uses actual compile-time data structures
auto schema = HybridMemoryMap<T>::get_schema();
std::string json = rfl::json::write(schema);  // Direct rfl serialization

// Schema contains exact compile-time information:
{
    "field_name": "data",
    "type": "fixed_vector<float, 100>",
    "is_variable_length": true,
    "element_size": 4,
    "max_elements": 100,
    "base_packed_size": 12,      // From HybridMemoryMap
    "max_packed_size": 412       // From HybridMemoryMap
}
```

### Implementation Rule

**All schema-relevant changes must update:**
1. `schema_generator.hpp` - Add new fields to TypeSchema struct
2. `visualize_schema.py` - Update CLI rendering
3. `visualize_schema_gui.py` - Update GUI rendering
4. **Shared Python module** (future) - Common logic for both viewers

**Never**: Create custom JSON manually - always use `rfl::json::write(schema_struct)`

## Questions to Ask Yourself

Before suggesting code:
1. Is this allocation-free? (No new/malloc/vector in hot paths)
2. Can this be computed at compile time? (Use constexpr/consteval)
3. Does this respect type safety? (Concepts/static_assert)
4. Is error handling deterministic? (std::optional, no exceptions)
5. Would this cause unnecessary copies? (Use std::span for views)
6. Is the complexity hidden from users? (Simple API, complex internals)
7. Does this maintain real-time guarantees? (Bounded execution time)
8. Does schema export stay close to compile-time structures? (Use rfl::json::write)

## Current Development Focus

### Active Work: Trait Simplification
**Problem**: Container registration spread across 3 files makes adding new types tedious  
**Goal**: Consolidate to single registration point

**Affected files:**
- `traits/container_detection.hpp` (is_fixed_container, capacity, element_size)
- `containers/container_traits.hpp` (is_fixed_capacity, category, traits)
- `core/traits/memory_map.hpp` (is_variable_length_field, max_elements)

**Target**: Define traits once, derive rest automatically

### Pending: RingBuffer Integration
**Challenge**: RingBuffer data may wrap around (non-contiguous)  
**Constraint**: Must maintain zero-allocation during serialization  
**Approach**: Needs special-case handling or linearization strategy

## Summary

SeRTial is a **compile-time**, **zero-allocation**, **type-safe** binary serialization library. When writing code:
- Think templates, not runtime dispatch
- Think stack, not heap
- Think compile-time validation, not runtime checks
- Think memcpy blocks, not field-by-field iteration
- Think constexpr, not dynamic

**Mantra**: If it can be computed at compile time, it SHALL be computed at compile time.

**Goal**: Make serialization trivial for users (`serialize(obj)`) while maintaining strict real-time guarantees through sophisticated compile-time machinery.
