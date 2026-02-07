# Container Handling in SeRTial

## Overview

SeRTial supports both **bounded containers** (compile-time max size) and **unbounded containers** (heap-allocated) for serialization. The type system uses compile-time traits to distinguish and optimize for each category.

## Container Categories

### 1. Fixed-Capacity Containers (Bounded)
**Properties:**
- Compile-time maximum size known
- Zero heap allocation
- Real-time safe
- Stack-allocated storage

**Supported Types:**
```cpp
fixed_vector<T, N>     // SeRTial's bounded vector
fixed_string<N>        // SeRTial's bounded string
RingBuffer<T, N>       // Circular buffer (in development)
std::array<T, N>       // Standard fixed-size array
```

**Interface Requirements:**
```cpp
template<typename T, std::size_t N>
class FixedCapacityContainer {
    using value_type = T;
    static constexpr std::size_t max_size = N;
    
    std::size_t size() const;          // Runtime size
    std::size_t capacity() const;      // Returns N
    const T* data() const;             // Pointer to elements
};
```

### 2. Dynamic Heap Containers (Unbounded)
**Properties:**
- Runtime size allocation
- Heap memory usage
- **NOT real-time safe**
- May cause unbounded serialization

**Supported Types:**
```cpp
std::vector<T>         // Dynamic array (heap-allocated)
std::string            // Dynamic string (heap-allocated)
```

**Warning**: Using unbounded containers breaks real-time guarantees and prevents compile-time buffer sizing.

### 3. Static Arrays
**Properties:**
- Fixed size (always N elements)
- No runtime size tracking
- Stack or static storage

**Supported Types:**
```cpp
std::array<T, N>       // Standard fixed-size array
T[N]                   // C-style array
```

**Serialization**: Full array always serialized (no length prefix).

## Container Detection System

### Concept-Based Registration (Phase 2 Complete)

**Single Registration Point**: `containers/container_registration.hpp`

SeRTial now uses a **C++20 concept-based system** that automatically detects and validates serializable containers. This eliminates the need for manual trait specializations in multiple files.

```cpp
/// @brief Concept: Container with compile-time capacity suitable for serialization
template<typename T>
concept SerializableContainer = requires(const T& c, T& mut_c) {
    // Must have value_type
    typename T::value_type;
    
    // Must have compile-time max_size_v (NOT max_size - containers use _v suffix)
    { T::max_size_v } -> std::convertible_to<std::size_t>;
    
    // Must have runtime size query
    { c.size() } -> std::same_as<std::size_t>;
    
    // Must provide contiguous data access
    { c.data() } -> std::convertible_to<const typename T::value_type*>;
    
    // Mutable data access (for deserialization)
    { mut_c.data_unsafe() } -> std::convertible_to<typename T::value_type*>;
    
    // Unsafe size setter (for deserialization)
    { mut_c.set_size_unsafe(std::size_t{}) } -> std::same_as<void>;
} && 
// Prevent nested containers: value_type must NOT be a container itself
!requires { typename T::value_type::max_size_v; };
```

**Automatic Trait Extraction**: All metadata is derived from the concept check:

```cpp
/// @brief Automatic metadata extraction for SerializableContainer types
template<SerializableContainer T>
struct container_metadata {
    using element_type = typename T::value_type;
    static constexpr std::size_t max_size = T::max_size_v;
    static constexpr std::size_t element_size = sizeof(element_type);
    static constexpr bool is_variable_length = true;
    static constexpr bool is_fixed_capacity = true;
    static constexpr bool is_serializable = true;
};

// Convenience aliases (automatically work for all SerializableContainer types)
template<typename T>
inline constexpr std::size_t container_max_size_v = container_metadata<T>::max_size;

template<typename T>
inline constexpr std::size_t container_element_size_v = container_metadata<T>::element_size;
```

```cpp
// Immediately works in structs:
struct Message {
    uint32_t id;
    MyContainer<float, 100> data;  // Serializable
    uint64_t timestamp;
};

// Compile-time analysis works:
using HMM = StructLayout<Message>;
static_assert(HMM::has_variable_fields);
static_assert(HMM::max_packed_size == 4 + 4 + 100*4 + 8);  // id + length + data + timestamp

// Serialization works:
Message msg{42, {1.0f, 2.0f, 3.0f}, 1234567890};
auto buffer = serialize(msg);  // Just works
auto restored = deserialize<Message>(buffer.view());  // Just works
```

### Common Mistakes and Solutions

#### Mistake 1: Using `max_size` instead of `max_size_v`

```cpp
// WRONG - concept expects max_size_v
static constexpr std::size_t max_size = N;

// CORRECT - note the _v suffix
static constexpr std::size_t max_size_v = N;
```

**Reason**: Historical convention - containers use `max_size_v` to avoid confusion with the `max_size()` method.

#### Mistake 2: Missing `data_unsafe()` or `set_size_unsafe()`

```cpp
// WRONG - only const data()
const T* data() const;

// CORRECT - also provide mutable access for deserialization
const T* data() const;
T* data_unsafe();  // Mutable access
void set_size_unsafe(std::size_t n);  // Direct size setter
```

**Reason**: Deserialization needs to write directly to container storage without validation overhead.

#### Mistake 3: Concept Fails with Unclear Error

If concept check fails, compiler shows which requirement failed:

```
error: constraints not satisfied
  required expression 'T::max_size_v' is invalid
  required expression 'mut_c.data_unsafe()' is invalid
```

**Solution**: Check each requirement individually:
```cpp
// Debug: check requirements one by one
using T = MyContainer<int, 10>;
static_assert(requires { typename T::value_type; });  // Has value_type?
static_assert(requires { T::max_size_v; });  // Has max_size_v?
static_assert(requires(T c) { c.size(); });  // Has size()?
static_assert(requires(T c) { c.data(); });  // Has data()?
static_assert(requires(T c) { c.data_unsafe(); });  // Has data_unsafe()?
static_assert(requires(T c) { c.set_size_unsafe(0); });  // Has set_size_unsafe()?
```

### Nested Container Detection

The concept **automatically rejects nested containers**:

```cpp
// Compile-time rejection of nested containers
static_assert(SerializableContainer<fixed_vector<int, 10>>);  // PASS
static_assert(!SerializableContainer<fixed_vector<fixed_vector<int, 5>, 10>>);  // Should PASS (rejected)
```

**Current status**: Nested container rejection is implemented but may not work perfectly on all compilers. If you encounter nested containers at compile time, you'll get an error during serialization (safe - won't compile invalid code).

### Special Cases

#### RingBuffer (Wrap-Around Handling)

RingBuffer is **intentionally excluded** from the concept system:

```cpp
// RingBuffer does NOT satisfy SerializableContainer
static_assert(!SerializableContainer<RingBuffer<float, 100>>);
```

**Reason**: RingBuffer data may wrap around (non-contiguous), requiring special serialization logic. This is **by design** - RingBuffer gets custom serialization handling in a future phase.

#### std::vector and std::string

These satisfy the concept requirements but are **unbounded** (heap-allocated):

```cpp
// Satisfies concept (has value_type, size(), data(), etc.)
// But breaks real-time guarantees (heap allocation)
std::vector<int> vec;
std::string str;
```

**Recommendation**: Use `fixed_vector<T, N>` and `fixed_string<N>` for real-time code

## Adding a New Container Type

With the concept-based system, adding a new container is **simple and straightforward**:

### Step 1: Implement the Container Interface

Create your container in `include/sertial/containers/my_container.hpp`:

```cpp
template<typename T, std::size_t N>
class MyContainer {
public:
    // Required nested type
    using value_type = T;
    
    // Required compile-time constant (NOTE: use max_size_v, not max_size)
    static constexpr std::size_t max_size_v = N;
    
    // Required runtime methods
    constexpr std::size_t size() const noexcept { return size_; }
    constexpr std::size_t capacity() const noexcept { return N; }
    constexpr const T* data() const noexcept { return data_.data(); }
    
    // Required deserialization methods
    constexpr T* data_unsafe() noexcept { return data_.data(); }
    constexpr void set_size_unsafe(std::size_t n) noexcept { size_ = n; }
    
    // Your custom interface...
    void push_back(const T& value);
    void clear() noexcept;
    // etc.
    
private:
    std::array<T, N> data_;
    std::size_t size_{0};
};
```

### Step 2: Verify Concept Satisfaction

Add a static assertion in `containers/container_traits.hpp`:

```cpp
#include "my_container.hpp"

// At the end of the file, with other static assertions:
static_assert(SerializableContainer<MyContainer<int, 10>>, 
              "MyContainer must satisfy SerializableContainer");
```

### Step 3: Test

That's it! The concept system automatically:
- Detects your container as fixed-capacity
- Extracts element_type, max_size, element_size
- Enables serialization/deserialization
- Computes correct buffer sizes
- Generates schema information

```cpp
// Immediate

## Serialization Format

### Fixed-Capacity Containers

**Wire Format:**
```
[length:4 bytes][element_0][element_1]...[element_N-1]
```

**Example:**
```cpp
fixed_vector<float, 100> vec = {1.0f, 2.0f, 3.0f};

// Serialized (16 bytes):
[0x03000000][1.0f][2.0f][3.0f]
 \_ length _/\_____ data ____/
   4 bytes       12 bytes

// NOT serialized:
// - Capacity (100) - known at compile time
// - Unused storage (97 empty slots) - not written
```

**Key Points:**
- Length prefix always 4 bytes (`uint32_t`)
- Only actual elements serialized (`size()` elements, not `capacity()`)
- Element order preserved
- Capacity NOT stored (reconstructed from type information)

### Dynamic Containers (std::vector, std::string)

**Wire Format:** Same as fixed-capacity (length + data)

**Warning:**
```cpp
struct Message {
    std::vector<int> data;  // Unbounded!
};

// Cannot compute max_packed_size at compile time
// Cannot pre-allocate stack buffer
// Breaks real-time guarantees
```

**Alternative (Real-Time Safe):**
```cpp
struct Message {
    fixed_vector<int, 1000> data;  // Bounded
};

// max_packed_size = 4 + 1000*4 = 4004 bytes (compile-time known)
// Stack buffer: static_buffer<4004>
// Real-time safe: no heap allocation
```

### Static Arrays (std::array, C arrays)

**Wire Format:**
```
[element_0][element_1]...[element_N-1]
```

**No length prefix** - size always N (compile-time known)

**Example:**
```cpp
std::array<uint32_t, 3> arr = {10, 20, 30};

// Serialized (12 bytes):
[10][20][30]
// No length prefix - always 3 elements
```

## Container Interface Requirements

For a container to be serializable, it must provide:

### Compile-Time Interface

```cpp
template<typename T, std::size_t N>
class MyContainer {
public:
    // Required nested types
    using value_type = T;
    
    // Required compile-time constants
    static constexpr std::size_t max_size = N;
```

### Runtime Interface

```cpp
    // Required methods
    std::size_t size() const;              // Number of elements currently stored
    std::size_t capacity() const;          // Maximum elements (returns N)
    const T* data() const;                 // Pointer to first element
```

### Internal Serialization API (Optional but Recommended)

For deserialization efficiency:

```cpp
    // Direct access for deserialization (bypasses checks)
    T* data_unsafe();                      // Raw pointer to storage
    void set_size_unsafe(std::size_t n);   // Set size without validation
```

**Rationale**: Deserialization knows data is valid (already serialized correctly). Bypassing validation improves performance and simplifies code.

## Container Size Calculations

### Compile-Time: Maximum Size

```cpp
template<typename T>
constexpr std::size_t max_container_size() {
    if constexpr (is_fixed_container_v<T>) {
        return sizeof(uint32_t) +                          // Length prefix
               fixed_container_capacity_v<T> *             // Max elements
               fixed_container_element_size_v<T>;          // Element size
    } else {
        return sizeof(T);  // Not a container
    }
}

// Example:
using Vec = fixed_vector<float, 100>;
static_assert(max_container_size<Vec>() == 4 + 100*4);  // 404 bytes
```

### Runtime: Actual Size

```cpp
template<typename Container>
std::size_t actual_container_size(const Container& c) {
    return sizeof(uint32_t) +                    // Length prefix
           c.size() * sizeof(typename Container::value_type);
}

// Example:
fixed_vector<float, 100> vec = {1.0f, 2.0f, 3.0f};
assert(actual_container_size(vec) == 4 + 3*4);  // 16 bytes
```

### Size Bounds

```cpp
// Compile-time guarantees:
template<typename T>
struct container_size_bounds {
    // Minimum size (empty container)
    static constexpr std::size_t min = sizeof(uint32_t);
    
    // Maximum size (full capacity)
    static constexpr std::size_t max = sizeof(uint32_t) + 
                                       T::max_size * sizeof(typename T::value_type);
};

// Runtime validation:
auto actual = actual_container_size(container);
assert(actual >= container_size_bounds<decltype(container)>::min);
assert(actual <= container_size_bounds<decltype(container)>::max);
```

## Struct Size Calculations

### With Fixed-Capacity Containers

```cpp
struct SensorData {
    uint64_t timestamp;                // 8 bytes (fixed)
    uint32_t sensor_id;                // 4 bytes (fixed)
    fixed_vector<float, 100> readings; // Variable (4 + 0-400 bytes)
    uint64_t checksum;                 // 8 bytes (fixed, runtime offset)
};

// Size analysis:
using HMM = StructLayout<SensorData>;

// Base size (fixed fields):
static_assert(HMM::base_packed_size == 8 + 4 + 8);  // 20 bytes

// Maximum size (container at capacity):
static_assert(HMM::max_packed_size == 20 + 4 + 400);  // 424 bytes

// Runtime size (N readings):
std::size_t runtime_size(const SensorData& data) {
    return 20 + 4 + data.readings.size() * 4;  // 20 + 4 + N*4
}
```

### Minimum/Maximum Size Computation

```cpp
template<typename T>
struct message_size_bounds {
    using HMM = StructLayout<T>;
    
    // Minimum: all containers empty
    static constexpr std::size_t min_size = []() constexpr {
        std::size_t size = HMM::base_packed_size;  // Fixed fields
        
        // Add length prefixes for each dynamic block (empty containers)
        for (const auto& block : HMM::dynamic_blocks) {
            size += sizeof(uint32_t);  // Just the prefix, no data
        }
        
        return size;
    }();
    
    // Maximum: all containers at capacity
    static constexpr std::size_t max_size = HMM::max_packed_size;
};

// Example:
using Bounds = message_size_bounds<SensorData>;
static_assert(Bounds::min_size == 20 + 4);      // 24 bytes (empty vector)
static_assert(Bounds::max_size == 20 + 404);    // 424 bytes (full vector)
```

## Special Case: RingBuffer

RingBuffer poses unique challenges due to wrap-around:

### Memory Layout Scenarios

```cpp
RingBuffer<char, 8> rb;

// Scenario 1: No wrap-around (contiguous)
// head=0, tail=4, size=4
Memory: [A][B][C][D][_][_][_][_]
        ^head        ^tail

Serialization:
  memcpy(dest, rb.data() + head, size)  // 1 memcpy
Output: [A][B][C][D]

// Scenario 2: Wrapped (non-contiguous)
// head=6, tail=2, size=4
Memory: [C][D][_][_][_][_][A][B]
        ^tail        ^head

Serialization:
  memcpy(dest, rb.data() + head, capacity - head)  // A, B
  memcpy(dest + len1, rb.data(), tail)              // C, D
Output: [A][B][C][D]  // Logical order preserved

// Deserialization always creates non-wrapped buffer:
Memory: [A][B][C][D][_][_][_][_]
        ^head        ^tail
```

### Design Considerations

**Option 1: Runtime Wrap Detection**
```cpp
// Serialization checks at runtime
if (rb.head() < rb.tail()) {
    // Not wrapped: 1 memcpy
    memcpy(dest, rb.data() + rb.head(), rb.size() * elem_size);
} else {
    // Wrapped: 2 memcpy
    size_t first_chunk = capacity - rb.head();
    memcpy(dest, rb.data() + rb.head(), first_chunk * elem_size);
    memcpy(dest + first_chunk * elem_size, rb.data(), rb.tail() * elem_size);
}
```

**Option 2: Always Linearize**
```cpp
// RingBuffer provides get_ordered_data() method
auto ordered = rb.get_ordered_data();  // Returns vector/array with logical order
memcpy(dest, ordered.data(), rb.size() * elem_size);

// Pros: Simple, consistent
// Cons: Extra copy for non-wrapped case
```

**Recommendation**: Option 1 (runtime detection) for best performance.

### Required Information

**Compile-time (same as other containers):**
- `element_size = sizeof(T)`
- `capacity = N`

**Runtime (RingBuffer-specific):**
- `size()` - number of elements
- `head()` - write position (or first element index)
- `tail()` - read position (or past-last element index)
- `data()` - pointer to underlying storage

**No new compile-time traits needed** - handle wrap detection at runtime in serialization code.

## Element Padding and Memory Layout

### How Elements Are Serialized

Container elements are serialized using **direct memory copy** from the underlying C array:

```cpp
// Serialization uses:
std::size_t data_size = field.size() * sizeof(T);
std::memcpy(dest, field.data(), data_size);  // Copy as-is from C array
```

**Key principle**: Elements are copied with their **natural C++ layout**, including any internal padding.

### Example: Elements With Internal Padding

```cpp
struct PaddedElement {
    uint8_t a;   // 1 byte
    // 3 bytes padding (for alignment)
    uint32_t b;  // 4 bytes
};  // sizeof = 8 bytes

fixed_vector<PaddedElement, 10> vec;
vec.push_back({1, 100});
vec.push_back({2, 200});

// Serialized format:
// [length:4][elem0:8 bytes][elem1:8 bytes]
//           |  includes   | |  includes  |
//           |  padding    | |  padding   |

// Size calculation:
max_size = 4 + 10 * sizeof(PaddedElement)
         = 4 + 10 * 8  // sizeof includes padding!
         = 84 bytes
```

**Important**: The padding **IS serialized** as part of each element. This is:
- **Correct** - matches C++ memory layout
- **Fast** - simple memcpy, no element-by-element recursion
- ✅ **Safe** - padding bytes don't affect deserialization (just occupy space)
- ⚠️ **Slightly wasteful** - padding bytes transmitted (but unavoidable with memcpy approach)
- ⚠️ **Architecture-dependent** - padding varies by platform (see portability warning below)

### Elements Are Tightly Packed (No Inter-Element Padding)

C and C++ **guarantee** that array elements are contiguous:

```cpp
PaddedElement array[3];
// Memory layout:
// [elem0:8][elem1:8][elem2:8]  ← NO gaps between elements
// ^       ^       ^       ^
// &array[0]       &array[2]
//         &array[1]

// Guaranteed: &array[i+1] == &array[i] + sizeof(PaddedElement)
```

**Result**: When we `memcpy(field.data(), size * sizeof(T))`, we get tightly packed elements (each with its internal padding, but no gaps between).

### Cross-Architecture Portability Warning

**Current serialization is ARCHITECTURE-DEPENDENT:**

```cpp
struct Data {
    uint8_t a;
    uint32_t b;
};  // sizeof varies by architecture!

// x86-64:    sizeof = 8 bytes (3 bytes padding)
// Some ARM:  sizeof = 8 bytes (3 bytes padding)
// Some DSPs: sizeof = 5 bytes (0 bytes padding - byte aligned)
```

**Result**: Serialized data from one architecture **may not deserialize correctly** on another.

**Current scope**: SeRTial is designed for **same-architecture communication** (e.g., real-time IPC on same processor).

**For portable serialization**:
```cpp
// Option 1: Force packed layout
struct Data {
    uint8_t a;
    uint32_t b;
} __attribute__((packed));  // GCC/Clang

**Solution (if needed in future)**:
- Detect nested containers at compile time
- Use recursive serialization (iterate and serialize each inner container)
- Add explicit trait: `is_trivially_serializable_v<T>` to distinguish raw data from containers

**Current approach**: Static assertion to prevent nested containers (added below).

### Struct Composition with `rfl::Flatten`

**For struct "inheritance" or composition**, use `rfl::Flatten` to avoid nested struct overhead:

```cpp
// DON'T: Nested struct (extra padding possible)
struct Base {
    uint32_t id;
};

struct Derived {
    Base base;      // Nested struct - may add padding
    float value;    // Offset depends on Base alignment
};

// DO: Flatten fields into parent
struct Base {
    uint32_t id;
};

struct Derived {
    rfl::Flatten<Base> base;  // Fields flattened to Derived level
    float value;              // Directly follows id, no struct boundary
};

// Serialization sees:
// Derived { uint32_t id; float value; }
// No nesting overhead, optimal layout
```

**Benefits:**
- Eliminates one level of padding
- Fields at same struct level (better packing)
- Works with SeRTial's field-by-field analysis
- Compile-time error on duplicate field names

**Example: Employee is a Person**
```cpp
struct Person {
    std::string first_name;
    std::string last_name;
    int age;
};

struct Employee {
    rfl::Flatten<Person> person;  // All Person fields flattened here
    std::string employer;
    float salary;
};

// Serialization treats Employee as having 5 fields:
// { first_name, last_name, age, employer, salary }
```

**Reference**: [rfl::Flatten documentation](https://rfl.getml.com/flatten_structs/)
};  // Better, but still architecture-specific
```

**See**: `docs/work/PADDING_AND_PORTABILITY.md` for detailed analysis and solutions.

### Current Limitation: No Nested Containers

**Not currently supported:**

```cpp
// CURRENTLY NOT SUPPORTED
struct Matrix {
    fixed_vector<fixed_vector<float, 5>, 10> data;  // Nested containers
};

// Problem: Inner fixed_vector is not just data!
// fixed_vector<float, 5> has:
//   - float data_[5]   // 20 bytes
//   - size_t size_     // 8 bytes
//   Total: 28-32 bytes (with alignment)

// Using sizeof(fixed_vector<float, 5>) would:
// - Serialize size_ member (runtime state - WRONG!)
// - Include padding after size_ (wasted)
// - Break deserialization (size_ would be overwritten with garbage)
```

**Why it fails:**
- `sizeof(fixed_vector<T, N>)` includes the `size_` member
- Memcpy would serialize runtime state (not just data)
- Deserialization would corrupt the size_ member

**Solution (if needed in future)**:
- Detect nested containers at compile time
- Use recursive serialization (iterate and serialize each inner container)
- Add explicit trait: `is_trivially_serializable_v<T>` to distinguish raw data from containers

**Current approach**: Static assertion to prevent nested containers (added below).

## Best Practices

### When to Use Each Container

```cpp
// Real-time systems: Use fixed-capacity containers
struct RTMessage {
    fixed_vector<Sample, 1000> samples;  // Bounded, no allocation
    fixed_string<256> label;             // Bounded, no allocation
};

// Real-time systems: Avoid unbounded containers
struct BadRTMessage {
    std::vector<Sample> samples;  // Heap allocation, unbounded
    std::string label;            // Heap allocation, unbounded
};

// Non-real-time: Unbounded containers acceptable
struct OfflineMessage {
    std::vector<Record> records;  // OK for offline processing
    std::string description;      // OK for offline processing
};

// Fixed-size arrays: No length tracking needed
struct Config {
    std::array<float, 16> matrix;  // Always 16 elements
    uint8_t version;
};
```

### Memory Efficiency

```cpp
// Good: Capacity matches expected usage
fixed_vector<Point, 100> points;  // Expect ~50-100 points

// Bad: Over-allocated
fixed_vector<Point, 10000> points;  // Expect ~10 points
// Wastes: 10000*sizeof(Point) stack space

// Solution: Choose capacity based on actual needs
// Rule of thumb: capacity = max_expected * 1.2
```

### Type Safety

```cpp
// Good: Compile-time checks
template<typename T>
requires SerializableContainer<T>  // Concept enforces requirements
void send(const T& container);

// Bad: Runtime checks
template<typename T>
void send(const T& container) {
    if (!has_data_method(container)) {  // Runtime error
        throw std::runtime_error("Not a container");
    }
}
```

### Nested Containers (Current Limitation)

```cpp
// ❌ NOT SUPPORTED: Nested fixed containers
struct Matrix {
    fixed_vector<fixed_vector<float, 5>, 10> rows;  // COMPILE ERROR
};
// Reason: Inner containers have runtime state (size_) that would be serialized incorrectly

// ✅ ALTERNATIVE 1: Flatten the structure
struct Matrix {
    fixed_vector<float, 50> data;  // 10 rows * 5 cols = 50 elements
    uint32_t rows = 10;
    uint32_t cols = 5;
    // Access: data[row * cols + col]
};

// ✅ ALTERNATIVE 2: Use fixed-size array of containers
struct Matrix {
    std::array<fixed_vector<float, 5>, 10> rows;  // std::array is always full
};
// Note: All 10 rows exist (can't have variable number of rows)

// ✅ WORKS: Containers of simple structs (even with padding)
struct Point { float x, y, z; uint8_t padding[4]; };  // 16 bytes with padding
struct Cloud {
    fixed_vector<Point, 1000> points;  // OK - Point is POD
};
```

## Schema Viewer Compatibility

Container serialization format is designed for easy visualization:

### JSON Schema Generation

```cpp
// Container schema includes:
{
    "field_name": "readings",
    "type": "fixed_vector<float, 100>",
    "is_variable_length": true,
    "element_type": "float",
    "element_size": 4,
    "max_elements": 100,
    "serialized_format": "length_prefix + data",
    "length_prefix_size": 4,
    "min_serialized_size": 4,      // Empty container
    "max_serialized_size": 404     // Full capacity
}
```

### Viewer Features

Python viewers (CLI and GUI) should display:
- Current size vs. capacity
- Element-by-element breakdown
- Hex dump of length prefix + data
- Memory layout visualization

**Implementation**: Extract from `StructLayout<T>` compile-time information via `get_schema()` → JSON → Python visualization.
