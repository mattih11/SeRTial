# SeRTial Container Guide

**Complete reference for SeRTial's bounded containers**

## Documentation

**User Guides**: [USER_GUIDE.md](USER_GUIDE.md) | [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md) | [EXAMPLES.md](EXAMPLES.md) | [SCHEMA_VIEWER.md](SCHEMA_VIEWER.md)  
**Developer Guides**: [ADDING_CONTAINERS.md](ADDING_CONTAINERS.md) | [CONTAINER_INTERNALS.md](CONTAINER_INTERNALS.md)  
**Technical References**: [SERIALIZATION_MECHANISM.md](SERIALIZATION_MECHANISM.md) | [REFLECTOR_BASED_SCHEMA.md](REFLECTOR_BASED_SCHEMA.md) | [SIZE_CALCULATIONS.md](SIZE_CALCULATIONS.md) | [TEMPLATE_PATTERNS.md](TEMPLATE_PATTERNS.md)

---

## Table of Contents

1. [Overview](#overview)
2. [fixed_vector](#fixed_vector)
3. [fixed_string](#fixed_string)
4. [RingBuffer](#ringbuffer)
5. [static_buffer](#static_buffer)
6. [std::array](#stdarray)
7. [Choosing a Container](#choosing-a-container)
8. [Container Limitations](#container-limitations)

---

## Overview

SeRTial provides **bounded containers** for real-time safe serialization with zero heap allocation. All containers have **compile-time capacity** but **runtime size**.

### Key Characteristics

| Container | Purpose | Capacity | Size | Use Case |
|-----------|---------|----------|------|----------|
| `fixed_vector<T, N>` | Variable-size sequence | Fixed | Variable | Dynamic lists with known max |
| `fixed_string<N>` | Variable-length text | Fixed | Variable | Text/names with max length |
| `RingBuffer<T, N>` | Circular history | Fixed | Variable | Time-series/FIFO with overflow |
| `static_buffer<N>` | Raw bytes | Fixed | Variable | Binary data/low-level buffers |
| `std::array<T, N>` | Fixed-size sequence | Fixed | **Fixed** | Always N elements |

### Serialization Behavior

**Variable-size containers** (fixed_vector, fixed_string, RingBuffer, static_buffer):
- Serialize **only actual elements** (not capacity)
- Wire format: `[length:4 bytes][data:size*elem_size]`
- Deserialization fills container with actual size

**Fixed-size containers** (std::array):
- Serialize **all N elements** (always)
- Wire format: `[data:N*elem_size]` (no length prefix)
- Deserialization expects exactly N elements

---

## fixed_vector

**Variable-size vector with compile-time capacity**

```cpp
#include <sertial/containers/fixed_vector.hpp>
```

### Declaration

```cpp
template<typename T, std::size_t MaxSize>
class fixed_vector;
```

### Constructor

```cpp
fixed_vector<float, 100> vec;           // Empty, capacity 100
fixed_vector<int, 10> vec2{1, 2, 3};   // Initialize with 3 elements
fixed_vector<char, 50> vec3(5, 'x');   // 5 copies of 'x'
```

### Capacity Methods

```cpp
size_t size() const;          // Current number of elements
size_t capacity() const;      // Maximum capacity (always MaxSize)
size_t max_size() const;      // Same as capacity()
bool empty() const;           // true if size() == 0
bool full() const;            // true if size() == capacity()
```

### Element Access

```cpp
T& operator[](size_t index);          // Unchecked access (assert in debug)
const T& operator[](size_t index) const;

T& at(size_t index);                  // Checked access (throws)
const T& at(size_t index) const;

T& front();                           // First element
T& back();                            // Last element
T* data();                            // Pointer to underlying array
```

### Modifiers

```cpp
void push_back(const T& value);       // Add element (throws if full)
void push_back(T&& value);            // Add element (move)
void pop_back();                      // Remove last element
void clear();                         // Remove all elements (size = 0)
void resize(size_t new_size);         // Change size (throws if > capacity)
```

### Iterators

```cpp
iterator begin();
iterator end();
const_iterator begin() const;
const_iterator end() const;
const_iterator cbegin() const;
const_iterator cend() const;
```

### Serialization

```cpp
struct SensorData {
    uint32_t sensor_id;
    fixed_vector<float, 100> readings;  // Max 100 readings
};

SensorData data;
data.sensor_id = 42;
data.readings.push_back(1.5f);
data.readings.push_back(2.5f);
data.readings.push_back(3.5f);

auto buffer = sertial::serialize(data);
// Wire format: [sensor_id:4][length:4][reading0:4][reading1:4][reading2:4]
// Total size: 4 + 4 + 3*4 = 20 bytes (not 4 + 4 + 100*4 = 408!)
```

### Example: Timestamped Measurements

```cpp
struct Measurement {
    uint64_t timestamp;
    fixed_vector<float, 1000> samples;
};

Measurement m;
m.timestamp = get_time_ns();
for (int i = 0; i < 100; ++i) {
    m.samples.push_back(read_adc());
}

auto buffer = sertial::serialize(m);
// Size: 8 + 4 + 100*4 = 412 bytes (not 8 + 4 + 1000*4 = 4012!)
```

---

## fixed_string

**Variable-length string with compile-time capacity**

```cpp
#include <sertial/containers/fixed_string.hpp>
```

### Declaration

```cpp
template<std::size_t MaxSize>
class fixed_string;
```

### Constructor

```cpp
fixed_string<256> str;                  // Empty string
fixed_string<64> str2("Hello");         // From C-string
fixed_string<128> str3("World"sv);      // From string_view
fixed_string<100> str4(std::string{"Test"});  // From std::string
fixed_string<50> str5(5, 'x');          // 5 copies of 'x': "xxxxx"
```

### Capacity Methods

```cpp
size_t size() const;          // Current string length (no null terminator)
size_t length() const;        // Same as size()
size_t capacity() const;      // Maximum capacity (MaxSize - 1, reserves 1 for '\0')
size_t max_size() const;      // Same as capacity()
bool empty() const;           // true if size() == 0
bool full() const;            // true if size() == capacity()
```

### Element Access

```cpp
char& operator[](size_t index);          // Unchecked access
const char& operator[](size_t index) const;

char& at(size_t index);                  // Checked access (throws)
const char& at(size_t index) const;

char& front();                           // First character
char& back();                            // Last character
const char* c_str() const;               // Null-terminated C-string
const char* data() const;                // Same as c_str()
```

### Modifiers

```cpp
void push_back(char ch);                 // Append character (throws if full)
void pop_back();                         // Remove last character
void clear();                            // Empty the string
void resize(size_t new_size);            // Change length (throws if > capacity)
void append(const char* str);            // Append C-string
void append(std::string_view sv);        // Append string_view
```

### String Operations

```cpp
std::string_view view() const;           // Get string_view (zero-copy)
std::string to_string() const;           // Convert to std::string (copy)

// Assignment
fixed_string<N>& operator=(const char* str);
fixed_string<N>& operator=(std::string_view sv);
fixed_string<N>& operator=(const std::string& str);

// Comparison
bool operator==(const fixed_string<N>& other) const;
bool operator==(const char* str) const;
bool operator==(std::string_view sv) const;
```

### Serialization

```cpp
struct LogMessage {
    uint64_t timestamp;
    fixed_string<256> message;
};

LogMessage log;
log.timestamp = 1234567890;
log.message = "System started successfully";

auto buffer = sertial::serialize(log);
// Wire format: [timestamp:8][length:4][chars:27]
// Total size: 8 + 4 + 27 = 39 bytes (not 8 + 4 + 256!)
```

### Example: Named Data

```cpp
struct NamedValue {
    fixed_string<64> name;
    float value;
};

NamedValue nv;
nv.name = "temperature";  // 11 chars
nv.value = 25.5f;

auto buffer = sertial::serialize(nv);
// Size: 4 + 11 + 4 = 19 bytes
```

---

## RingBuffer

**Circular buffer with FIFO overflow behavior**

```cpp
#include <sertial/containers/ring_buffer.hpp>
```

### Declaration

```cpp
template<typename T, std::size_t MaxSize>
class RingBuffer;
```

### Constructor

```cpp
RingBuffer<float, 100> ring;            // Empty, capacity 100
RingBuffer<int, 50> ring2{1, 2, 3};    // Initialize with 3 elements
```

### Capacity Methods

```cpp
size_t size() const;          // Current number of elements
size_t capacity() const;      // Maximum capacity (always MaxSize)
size_t max_size() const;      // Same as capacity()
bool empty() const;           // true if size() == 0
bool full() const;            // true if size() == capacity()
```

### Element Access

```cpp
T& operator[](size_t index);          // Logical index (0 = oldest)
const T& operator[](size_t index) const;

T& at(size_t index);                  // Checked access (throws)
const T& at(size_t index) const;

T& front();                           // Oldest element
T& back();                            // Newest element
```

### Modifiers

```cpp
void push_back(const T& value);       // Add element (overwrites oldest if full)
void push_back(T&& value);            // Add element (move)
void pop_front();                     // Remove oldest element
void clear();                         // Remove all elements
```

### Overflow Behavior

**Key difference from fixed_vector**: When full, `push_back()` **overwrites oldest element** instead of throwing.

```cpp
RingBuffer<int, 3> ring;
ring.push_back(1);  // [1]
ring.push_back(2);  // [1, 2]
ring.push_back(3);  // [1, 2, 3] (full)
ring.push_back(4);  // [2, 3, 4] (1 overwritten)
ring.push_back(5);  // [3, 4, 5] (2 overwritten)

assert(ring.size() == 3);
assert(ring.front() == 3);  // Oldest remaining
assert(ring.back() == 5);   // Newest
```

### Iterators

RingBuffer iterators traverse from **oldest to newest** (logical order):

```cpp
RingBuffer<int, 5> ring{3, 4, 5};
for (int val : ring) {
    std::cout << val << " ";  // Output: 3 4 5
}
```

### Serialization

```cpp
struct History {
    uint32_t buffer_id;
    RingBuffer<float, 100> values;
};

History hist;
hist.buffer_id = 1;
for (int i = 0; i < 150; ++i) {
    hist.values.push_back(i * 0.1f);  // Only last 100 kept
}

auto buffer = sertial::serialize(hist);
// Wire format: [buffer_id:4][size:4][values:100*4]
// Total size: 4 + 4 + 100*4 = 408 bytes

// Deserialization restores current contents (not capacity)
auto restored = sertial::deserialize<History>(buffer.view());
assert(restored->values.size() == 100);
assert(restored->values.front() == 50 * 0.1f);  // Oldest kept value
```

### Example: Time-Series Buffer

```cpp
struct SensorHistory {
    fixed_string<32> sensor_name;
    RingBuffer<float, 1000> last_readings;  // Keep last 1000
};

SensorHistory sensor;
sensor.sensor_name = "temp_01";

while (running) {
    float reading = read_temperature();
    sensor.last_readings.push_back(reading);  // Auto-overflow
    
    if (need_snapshot) {
        auto buffer = sertial::serialize(sensor);
        save_to_disk(buffer.view());
    }
}
```

---

## static_buffer

**Raw byte buffer with compile-time capacity**

```cpp
#include <sertial/containers/static_buffer.hpp>
```

### Declaration

```cpp
template<std::size_t Capacity>
class static_buffer;
```

### Constructor

```cpp
static_buffer<1024> buf;                        // Empty, capacity 1024
static_buffer<512> buf2(100);                   // Size 100 (uninitialized)
static_buffer<256> buf3(std::span{data, 50});  // Copy 50 bytes from data
```

### Capacity Methods

```cpp
size_t size() const;          // Current number of bytes
size_t capacity() const;      // Maximum capacity (always Capacity)
size_t remaining() const;     // capacity() - size()
bool empty() const;           // true if size() == 0
bool full() const;            // true if size() == capacity()
```

### Element Access

```cpp
std::byte* data();                    // Raw pointer to buffer
const std::byte* data() const;
std::span<std::byte> span();          // Writable span of used bytes
std::span<const std::byte> view() const;  // Read-only span
```

### Modifiers

```cpp
void resize(size_t new_size);         // Change size (throws if > capacity)
void clear();                         // Set size to 0
void assign(std::span<const std::byte> data);  // Copy bytes
```

### Serialization

`static_buffer` is typically the **result** of serialization:

```cpp
Point3D point{1.0f, 2.0f, 3.0f};
auto buffer = sertial::serialize(point);  // Returns static_buffer<12>

// buffer is static_buffer<sizeof(Point3D)>
std::span<const std::byte> view = buffer.view();
socket.send(view.data(), view.size());
```

### Example: Pre-Allocated Buffer

```cpp
// Prepare buffer for multiple serializations
static_buffer<1024> reusable_buffer;

for (const auto& msg : messages) {
    size_t size = sertial::serialize_to(msg, reusable_buffer.data());
    reusable_buffer.resize(size);
    
    transmit(reusable_buffer.view());
}
```

---

## std::array

**Fixed-size standard array (always serializes all elements)**

```cpp
#include <array>
```

### Declaration

```cpp
template<typename T, std::size_t N>
struct std::array;
```

### Key Difference

Unlike variable-size containers, `std::array` **always serializes all N elements**:

```cpp
struct Matrix3x3 {
    std::array<float, 9> elements;  // Always 9 floats
};

Matrix3x3 mat;
mat.elements = {1, 0, 0,
                0, 1, 0,
                0, 0, 1};

auto buffer = sertial::serialize(mat);
// Wire format: [elements:9*4] (no length prefix)
// Size: 36 bytes (always, even if some zeros)
```

### When to Use

Use `std::array` for:
- Fixed dimensions (3D coordinates, rotation matrices)
- Compile-time known sizes
- When all elements are always meaningful

Use `fixed_vector` for:
- Variable-length data
- When actual count varies
- To save wire bandwidth

### Example: Fixed Structures

```cpp
struct Quaternion {
    std::array<float, 4> components;  // [x, y, z, w] always 4
};

struct Transform {
    std::array<float, 3> position;     // [x, y, z]
    Quaternion orientation;             // 4 components
};

Transform tf;
tf.position = {1.0f, 2.0f, 3.0f};
tf.orientation.components = {0.0f, 0.0f, 0.0f, 1.0f};

auto buffer = sertial::serialize(tf);
// Size: 3*4 + 4*4 = 28 bytes (fixed, no length prefixes)
```

---

## Choosing a Container

### Decision Tree

**Do you need variable-length data?**
- **No** → Use `std::array<T, N>` (simplest, no length prefix)
- **Yes** → Continue...

**What kind of data?**
- **Text/names** → Use `fixed_string<N>`
- **Binary data** → Use `static_buffer<N>`
- **Typed elements** → Continue...

**Do you need FIFO overflow (keep newest)?**
- **Yes** → Use `RingBuffer<T, N>` (oldest auto-discarded)
- **No** → Use `fixed_vector<T, N>` (throws when full)

### Use Cases

| Scenario | Container | Reason |
|----------|-----------|--------|
| 3D position (x,y,z) | `std::array<float, 3>` | Always 3 elements |
| Sensor readings | `fixed_vector<float, N>` | Variable count, max N |
| User name | `fixed_string<N>` | Variable length text |
| Recent history | `RingBuffer<T, N>` | Keep last N, FIFO |
| Raw packet data | `static_buffer<N>` | Binary blob |
| Rotation matrix | `std::array<float, 9>` | Always 3x3 |
| Log entries | `fixed_vector<Entry, N>` | Variable entries |

### Performance Comparison

All containers have:
- **O(1) access**: `operator[]`, `front()`, `back()`
- **O(1) insertion**: `push_back()` (when not resizing)
- **O(1) serialization**: Direct memcpy of used elements

Differences:
- `std::array`: No size tracking, fixed N elements always serialized
- `fixed_vector`/`fixed_string`: Size tracking, throws when full
- `RingBuffer`: Size tracking + head/tail pointers, overwrites when full

---

## Container Limitations

### No Nested Containers

**Not supported** (compile error):
```cpp
// ERROR: Nested containers not serializable
fixed_vector<fixed_vector<float, 10>, 100> matrix;
```

**Alternatives**:
```cpp
// Option 1: Flatten
fixed_vector<float, 1000> flattened;  // 100 rows × 10 columns

// Option 2: Use std::array inner
struct Row {
    std::array<float, 10> columns;
};
fixed_vector<Row, 100> rows;

// Option 3: Use POD structs
struct Point { float x, y, z; };
fixed_vector<Point, 1000> points;
```

### Capacity Not Serialized

Container capacity is compile-time only - **not transmitted**:

```cpp
fixed_vector<int, 100> vec1;
vec1.push_back(1);
vec1.push_back(2);

auto buffer = sertial::serialize(vec1);
auto vec2 = sertial::deserialize<fixed_vector<int, 100>>(buffer.view());

assert(vec2->capacity() == 100);  // Compile-time capacity
assert(vec2->size() == 2);        // Runtime size restored
// Wire format: [length:4][elem0:4][elem1:4] = 12 bytes (capacity not sent)
```

### Element Type Constraints

Container element types must be:
- **Trivially copyable** (for memcpy serialization)
- **No pointers** (addresses not valid after deserialization)
- **No dynamic containers** (std::vector, std::string forbidden)

**Supported element types**:
```cpp
// Primitives
fixed_vector<uint32_t, N> ints;
fixed_vector<float, N> floats;

// POD structs
struct Point { float x, y, z; };
fixed_vector<Point, N> points;

// std::array
fixed_vector<std::array<float, 3>, N> coords;

// Dynamic containers (FORBIDDEN)
fixed_vector<std::vector<int>, N> bad;     // ERROR
fixed_vector<std::string, N> bad2;          // ERROR
```

---

## Next Steps

- **See examples**: [EXAMPLES.md](EXAMPLES.md)
- **Add custom containers**: [ADDING_CONTAINERS.md](ADDING_CONTAINERS.md)
- **Understand internals**: [CONTAINER_INTERNALS.md](CONTAINER_INTERNALS.md)
- **Visualize schemas**: [SCHEMA_VIEWER.md](SCHEMA_VIEWER.md)

---

**Questions?** Open an issue: [https://github.com/mattih11/SeRTial](https://github.com/mattih11/SeRTial)
