# SeRTial User Guide

**Complete guide to using SeRTial for binary serialization**

## Documentation

**User Guides**: [USER_GUIDE.md](USER_GUIDE.md) | [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md) | [EXAMPLES.md](EXAMPLES.md) | [SCHEMA_VIEWER.md](SCHEMA_VIEWER.md)  
**Developer Guides**: [ADDING_CONTAINERS.md](ADDING_CONTAINERS.md) | [CONTAINER_INTERNALS.md](CONTAINER_INTERNALS.md)  
**Technical References**: [SERIALIZATION_MECHANISM.md](SERIALIZATION_MECHANISM.md) | [REFLECTOR_BASED_SCHEMA.md](REFLECTOR_BASED_SCHEMA.md) | [SIZE_CALCULATIONS.md](SIZE_CALCULATIONS.md) | [TEMPLATE_PATTERNS.md](TEMPLATE_PATTERNS.md)

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Core Concepts](#core-concepts)
3. [Basic Serialization](#basic-serialization)
4. [Working with Containers](#working-with-containers)
5. [Schema Generation](#schema-generation)
6. [Common Patterns](#common-patterns)
7. [Error Handling](#error-handling)
8. [Performance Tips](#performance-tips)
9. [Troubleshooting](#troubleshooting)

---

## Quick Start

### Installation

```bash
git clone https://github.com/mattih11/SeRTial.git
cd SeRTial
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

### Basic Usage

```cpp
#include <sertial/sertial.hpp>

// Define a simple message
struct Position {
    float x, y, z;
};

// Serialize
Position pos{1.5f, 2.5f, 3.5f};
auto buffer = sertial::serialize(pos);  // Returns static_buffer<N>

// Deserialize
auto restored = sertial::deserialize<Position>(buffer.view());
if (restored) {
    std::cout << "Position: (" << restored->x << ", " 
              << restored->y << ", " << restored->z << ")\n";
}
```

### Key Features

- **Zero allocation**: All buffers are stack-allocated with compile-time size
- **Type-safe**: Compile-time type checking via C++20 concepts
- **Fast**: Block-based serialization with single memcpy per contiguous region
- **Simple API**: `serialize(obj)` and `deserialize<T>(data)` - that's it!

---

## Core Concepts

### Compile-Time Size Analysis

SeRTial analyzes your types at compile time using `StructLayout<T>`:

```cpp
struct Message {
    uint32_t id;                    // Fixed size: 4 bytes
    fixed_vector<float, 100> data;  // Variable size: 4 (length) + 0-400 bytes
    uint64_t timestamp;             // Fixed size: 8 bytes
};

// Compile-time analysis (happens automatically):
using Layout = StructLayout<Message>;
// Layout::base_packed_size = 16        (id + timestamp, no data)
// Layout::max_packed_size = 416        (id + 4 + 400 + timestamp)
// Layout::has_variable_fields = true   (contains fixed_vector)
```

### Zero-Allocation Guarantee

All serialization uses stack-allocated buffers:

```cpp
auto buffer = serialize(msg);  // Returns static_buffer<max_packed_size>
// buffer is std::array<std::byte, N> - NO heap allocation!

// Actual size determined at runtime based on container contents:
std::size_t actual_bytes = buffer.size();  // May be less than max_packed_size
```

### Supported Types

**Primitive Types**:
- Integers: `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, `int8_t`, `int16_t`, `int32_t`, `int64_t`
- Floating-point: `float`, `double`
- Boolean: `bool` (1 byte)

**Container Types** (see [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md)):
- `fixed_vector<T, N>` - Fixed-capacity vector
- `fixed_string<N>` - Fixed-capacity string
- `RingBuffer<T, N>` - Circular buffer
- `static_buffer<N>` - Raw byte buffer
- `std::array<T, N>` - Standard C++ array (always serializes all N elements)

**Struct Types**:
- Any struct with reflectable fields (plain POD structs work automatically with reflect-cpp)

---

## Basic Serialization

### Simple Serialize/Deserialize

```cpp
#include <sertial/sertial.hpp>

struct Point3D {
    float x, y, z;
};

int main() {
    // Serialize
    Point3D point{1.0f, 2.0f, 3.0f};
    auto buffer = sertial::serialize(point);
    
    // buffer.view() returns std::span<const std::byte>
    std::span<const std::byte> view = buffer.view();
    std::cout << "Serialized size: " << view.size() << " bytes\n";
    
    // Deserialize
    auto restored = sertial::deserialize<Point3D>(view);
    if (restored) {
        std::cout << "Restored: (" << restored->x << ", " 
                  << restored->y << ", " << restored->z << ")\n";
    }
}
```

### Serialize to Raw Pointer

For network I/O or when you have a pre-allocated buffer:

```cpp
// Prepare destination buffer
std::array<std::byte, 1024> dest_buffer;

// Serialize directly to raw pointer
std::size_t bytes_written = sertial::serialize_to(point, dest_buffer.data());

// Send over network
socket.send(dest_buffer.data(), bytes_written);
```

### Serialize to static_buffer

When you need to store or pass the buffer around:

```cpp
sertial::static_buffer<1024> buffer;
std::size_t size = sertial::serialize_to(point, buffer);
// buffer now contains serialized data, size() returns actual bytes used
```

### Deserialize into Existing Object

Avoid default construction when reusing objects in loops:

```cpp
Point3D reused_point;
for (const auto& packet : packets) {
    bool success = sertial::deserialize_into(packet.data, reused_point);
    if (success) {
        process(reused_point);
    }
}
```

---

## Working with Containers

SeRTial provides bounded containers for real-time safe serialization (no heap allocation).

### fixed_vector

Variable-size vector with compile-time capacity:

```cpp
#include <sertial/containers/fixed_vector.hpp>

struct SensorData {
    uint32_t sensor_id;
    sertial::fixed_vector<float, 100> readings;  // Max 100 elements
};

SensorData data;
data.sensor_id = 42;
data.readings.push_back(1.5f);
data.readings.push_back(2.5f);
data.readings.push_back(3.5f);

// Serialize only actual elements (not full capacity)
auto buffer = sertial::serialize(data);
// Size: 4 (sensor_id) + 4 (length) + 3*4 (readings) = 20 bytes
```

### fixed_string

Variable-length string with compile-time capacity:

```cpp
#include <sertial/containers/fixed_string.hpp>

struct LogMessage {
    uint64_t timestamp;
    sertial::fixed_string<256> message;  // Max 256 chars
};

LogMessage log;
log.timestamp = 1234567890;
log.message = "System started";  // Only 14 chars + null terminator

auto buffer = sertial::serialize(log);
// Size: 8 (timestamp) + 4 (length) + 15 (string data) = 27 bytes
```

### RingBuffer

Circular buffer with FIFO overflow behavior:

```cpp
#include <sertial/containers/ring_buffer.hpp>

struct HistoryBuffer {
    uint32_t buffer_id;
    sertial::RingBuffer<float, 100> history;  // Last 100 values
};

HistoryBuffer hist;
hist.buffer_id = 1;
for (int i = 0; i < 150; ++i) {
    hist.history.push_back(i * 0.1f);  // Older values overwritten
}

// Serialize only current elements (not capacity)
auto buffer = sertial::serialize(hist);
// Size: 4 (buffer_id) + 4 (size) + 100*4 (current elements) = 408 bytes
```

**See [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md) for complete container documentation.**

---

## Schema Generation

SeRTial can export JSON schemas for interactive visualization and documentation.

### Generate Schema

```cpp
#include <sertial/integration/schema_export.hpp>

struct MyMessage {
    uint32_t id;
    fixed_vector<float, 100> data;
    uint64_t timestamp;
};

int main() {
    // Export schema to JSON
    auto schema = sertial::get_struct_layout_schema<MyMessage>("MyMessage");
    std::string json = rfl::json::write(schema);
    
    // Save to file
    std::ofstream out("my_message.json");
    out << json;
}
```

### Visualize with Interactive Viewer

1. Generate schema JSON (as above)
2. Open viewer: `tools/sertial-inspect/viewer.html`
3. Drop JSON file or paste content
4. Explore structure interactively

**Live demo**: [https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html?schema=https://raw.githubusercontent.com/mattih11/SeRTial/main/examples/schemas/example_schemas.json](https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html?schema=https://raw.githubusercontent.com/mattih11/SeRTial/main/examples/schemas/example_schemas.json)

**See [SCHEMA_VIEWER.md](SCHEMA_VIEWER.md) for detailed viewer documentation.**

---

## Common Patterns

### Nested Structs

SeRTial automatically handles nested structures:

```cpp
struct Point3D {
    float x, y, z;
};

struct Pose3D {
    Point3D position;
    Point3D orientation;
};

struct RobotState {
    uint64_t timestamp;
    Pose3D pose;
    float velocity;
};

// Serialization handles nesting automatically
RobotState state{...};
auto buffer = sertial::serialize(state);
auto restored = sertial::deserialize<RobotState>(buffer.view());
```

### Message with Header

Common pattern for timestamped messages:

```cpp
struct Header {
    uint64_t timestamp;
    uint32_t sequence_number;
    uint32_t frame_id;
};

struct TemperatureReading {
    Header header;
    uint32_t sensor_id;
    float temperature_c;
    float confidence;
};

// Use like any other struct
TemperatureReading reading;
reading.header.timestamp = get_timestamp();
reading.header.sequence_number = seq++;
reading.header.frame_id = 1;
reading.sensor_id = 42;
reading.temperature_c = 25.5f;
reading.confidence = 0.95f;

auto buffer = sertial::serialize(reading);
```

### Real-Time Loop

Zero-allocation pattern for periodic publishing:

```cpp
void periodic_publisher() {
    SensorData data;
    data.sensor_id = 1;
    
    while (running) {
        // Clear and refill data (no allocation)
        data.readings.clear();
        data.readings.push_back(read_sensor());
        
        // Serialize (stack buffer)
        auto buffer = sertial::serialize(data);
        
        // Send (zero-copy view)
        socket.send(buffer.view().data(), buffer.size());
        
        sleep_for(period);
    }
}
```

---

## Error Handling

### Deserialization Returns Optional

```cpp
auto result = sertial::deserialize<MyType>(data);

if (result) {
    // Success - result contains MyType
    MyType& obj = *result;
    process(obj);
} else {
    // Failed - data too short, corrupted, or incompatible
    std::cerr << "Deserialization failed\n";
}
```

### Common Failure Causes

1. **Buffer too small**: Serialized data is shorter than expected type size
2. **Corrupted data**: Length prefix exceeds buffer bounds
3. **Version mismatch**: Struct layout changed between serialize/deserialize

### Validation Pattern

```cpp
bool validate_and_process(std::span<const std::byte> data) {
    // Minimum size check
    if (data.size() < sizeof(uint32_t)) {
        return false;
    }
    
    // Attempt deserialization
    auto msg = sertial::deserialize<Message>(data);
    if (!msg) {
        return false;
    }
    
    // Business logic validation
    if (msg->id == 0) {
        return false;
    }
    
    process(*msg);
    return true;
}
```

---

## Performance Tips

### 1. Use Fixed Containers

Replace dynamic containers with fixed-capacity equivalents:

```cpp
// Bad: Heap allocations
struct Message {
    std::vector<float> data;
    std::string name;
};

// Good: Stack-only
struct Message {
    sertial::fixed_vector<float, 100> data;
    sertial::fixed_string<64> name;
};
```

### 2. Avoid Padding

Reorder struct fields to minimize padding:

```cpp
// Bad: 8 bytes of padding
struct BadLayout {
    uint8_t  a;  // 1 byte + 3 padding
    uint32_t b;  // 4 bytes
    uint8_t  c;  // 1 byte + 3 padding
    uint32_t d;  // 4 bytes
};  // Total: 16 bytes (8 wasted)

// Good: No padding
struct GoodLayout {
    uint32_t b;  // 4 bytes
    uint32_t d;  // 4 bytes
    uint8_t  a;  // 1 byte
    uint8_t  c;  // 1 byte
};  // Total: 10 bytes
```

**Note**: SeRTial automatically skips padding during serialization, so padding doesn't affect wire format size.

### 3. Reuse Buffers

For high-frequency serialization, reuse objects:

```cpp
Message msg;
for (const auto& input : inputs) {
    msg.data.clear();
    msg.data.push_back(input.value);
    
    auto buffer = sertial::serialize(msg);  // msg reused, no allocation
    transmit(buffer.view());
}
```

### 4. Use serialize_to for Zero-Copy

When you already have a destination buffer:

```cpp
std::array<std::byte, 1024> network_buffer;
std::size_t size = sertial::serialize_to(msg, network_buffer.data());
socket.send(network_buffer.data(), size);  // No intermediate buffer
```

---

## Troubleshooting

### Compilation Errors

**Error: `Type is not reflectable`**
```
Solution: Add #include <rfl.hpp> and ensure struct has public fields
```

**Error: `max_size not found`**
```
Solution: Use fixed_vector/fixed_string instead of std::vector/std::string
```

**Error: `Serialization buffer too small`**
```
Solution: Increase container capacities or check max_packed_size calculation
```

### Runtime Issues

**Deserialization returns nullopt**
- Check buffer size: `data.size() >= expected_min_size`
- Verify struct layout matches between sender/receiver
- Ensure length prefixes don't exceed buffer bounds

**Unexpected serialized size**
- Remember: Only actual elements are serialized (not capacity)
- Check padding: Use `sizeof(YourStruct)` to see C++ layout
- Use schema viewer to inspect actual layout

### Debugging Tools

**Print serialized bytes**:
```cpp
#include <sertial/debug/print_utils.hpp>

auto buffer = sertial::serialize(msg);
sertial::debug::print_bytes(buffer.view());
```

**Inspect layout at compile time**:
```cpp
using Layout = sertial::StructLayout<MyType>;
std::cout << "Base size: " << Layout::base_packed_size << "\n";
std::cout << "Max size: " << Layout::max_packed_size << "\n";
std::cout << "Has variable fields: " << Layout::has_variable_fields << "\n";
```

**Generate schema for analysis**:
```bash
cd build
./schema_example > my_schema.json
# Open in tools/sertial-inspect/viewer.html
```

---

## Next Steps

- **Learn container usage**: [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md)
- **See code examples**: [EXAMPLES.md](EXAMPLES.md)
- **Explore schema viewer**: [SCHEMA_VIEWER.md](SCHEMA_VIEWER.md)
- **Understand internals**: [SERIALIZATION_MECHANISM.md](SERIALIZATION_MECHANISM.md)
- **API reference**: Run `make docs` for Doxygen documentation (coming soon)

---

**Questions or issues?** Open an issue on GitHub: [https://github.com/mattih11/SeRTial](https://github.com/mattih11/SeRTial)
