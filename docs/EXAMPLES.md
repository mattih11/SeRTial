# SeRTial Examples

**Comprehensive code examples demonstrating SeRTial usage**

## Documentation

**User Guides**: [USER_GUIDE.md](USER_GUIDE.md) | [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md) | [EXAMPLES.md](EXAMPLES.md) | [SCHEMA_VIEWER.md](SCHEMA_VIEWER.md)  
**Developer Guides**: [ADDING_CONTAINERS.md](ADDING_CONTAINERS.md) | [CONTAINER_INTERNALS.md](CONTAINER_INTERNALS.md)  
**Technical References**: [SERIALIZATION_MECHANISM.md](SERIALIZATION_MECHANISM.md) | [REFLECTOR_BASED_SCHEMA.md](REFLECTOR_BASED_SCHEMA.md) | [SIZE_CALCULATIONS.md](SIZE_CALCULATIONS.MD) | [TEMPLATE_PATTERNS.md](TEMPLATE_PATTERNS.md)

---

## Table of Contents

1. [Basic Examples](#basic-examples)
2. [Container Examples](#container-examples)
3. [Advanced Examples](#advanced-examples)
4. [Real-World Patterns](#real-world-patterns)
5. [Performance Examples](#performance-examples)
6. [Complete Example Programs](#complete-example-programs)

---

## Basic Examples

### Hello World - Serialize Simple Struct

```cpp
#include <sertial/sertial.hpp>
#include <iostream>

struct Point3D {
    float x, y, z;
};

int main() {
    // Create point
    Point3D point{1.5f, 2.5f, 3.5f};
    
    // Serialize
    auto buffer = sertial::serialize(point);
    std::cout << "Serialized size: " << buffer.size() << " bytes\n";
    
    // Deserialize
    auto restored = sertial::deserialize<Point3D>(buffer.view());
    if (restored) {
        std::cout << "Point: (" << restored->x << ", " 
                  << restored->y << ", " << restored->z << ")\n";
    }
    
    return 0;
}
```

**Output**:
```
Serialized size: 12 bytes
Point: (1.5, 2.5, 3.5)
```

---

### Nested Structs

```cpp
#include <sertial/sertial.hpp>

struct Quaternion {
    float x, y, z, w;
};

struct Pose3D {
    Point3D position;
    Quaternion orientation;
};

struct RobotState {
    uint64_t timestamp;
    Pose3D pose;
    float velocity;
};

int main() {
    RobotState state;
    state.timestamp = 1234567890;
    state.pose.position = {1.0f, 2.0f, 3.0f};
    state.pose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};
    state.velocity = 0.5f;
    
    // Serialization automatically handles nesting
    auto buffer = sertial::serialize(state);
    auto restored = sertial::deserialize<RobotState>(buffer.view());
    
    std::cout << "Timestamp: " << restored->timestamp << "\n";
    std::cout << "Position: (" << restored->pose.position.x << ", "
              << restored->pose.position.y << ", "
              << restored->pose.position.z << ")\n";
    
    return 0;
}
```

---

### Fixed-Size Arrays

```cpp
#include <sertial/sertial.hpp>
#include <array>

struct Matrix3x3 {
    std::array<float, 9> elements;
};

int main() {
    Matrix3x3 mat;
    mat.elements = {1, 0, 0,
                    0, 1, 0,
                    0, 0, 1};  // Identity matrix
    
    auto buffer = sertial::serialize(mat);
    // Size: 9 * 4 = 36 bytes (always, no length prefix)
    
    auto restored = sertial::deserialize<Matrix3x3>(buffer.view());
    std::cout << "Matrix[0][0] = " << (*restored).elements[0] << "\n";
    
    return 0;
}
```

---

## Container Examples

### fixed_vector - Variable-Size Data

```cpp
#include <sertial/sertial.hpp>
#include <sertial/containers/fixed_vector.hpp>

struct SensorReading {
    uint32_t sensor_id;
    uint64_t timestamp;
    sertial::fixed_vector<float, 100> measurements;
};

int main() {
    SensorReading reading;
    reading.sensor_id = 42;
    reading.timestamp = 1234567890;
    
    // Add actual measurements (not full capacity)
    reading.measurements.push_back(21.5f);
    reading.measurements.push_back(22.0f);
    reading.measurements.push_back(21.8f);
    
    auto buffer = sertial::serialize(reading);
    // Size: 4 + 8 + 4 + 3*4 = 28 bytes (not 4 + 8 + 4 + 100*4 = 416!)
    
    std::cout << "Serialized " << reading.measurements.size() 
              << " measurements (" << buffer.size() << " bytes)\n";
    
    auto restored = sertial::deserialize<SensorReading>(buffer.view());
    std::cout << "Restored " << restored->measurements.size() 
              << " measurements\n";
    
    return 0;
}
```

---

### fixed_string - Variable-Length Text

```cpp
#include <sertial/sertial.hpp>
#include <sertial/containers/fixed_string.hpp>

struct LogEntry {
    uint64_t timestamp;
    uint32_t level;  // 0=INFO, 1=WARN, 2=ERROR
    sertial::fixed_string<256> message;
};

int main() {
    LogEntry log;
    log.timestamp = 1234567890;
    log.level = 1;  // WARN
    log.message = "Temperature exceeds threshold";  // 30 chars
    
    auto buffer = sertial::serialize(log);
    // Size: 8 + 4 + 4 + 30 = 46 bytes (not 8 + 4 + 4 + 256 = 272!)
    
    std::cout << "Serialized log: " << buffer.size() << " bytes\n";
    
    auto restored = sertial::deserialize<LogEntry>(buffer.view());
    std::cout << "Message: " << restored->message.c_str() << "\n";
    
    return 0;
}
```

---

### fixed_string - Compile-Time Construction (NEW)

**NEW in v2.0.0**: Full constexpr support with automatic size deduction

```cpp
#include <sertial/sertial.hpp>
#include <sertial/containers/fixed_string.hpp>

// CTAD - auto-deduces size from string literal
constexpr sertial::fixed_string name{"SeRTial"};  // fixed_string<7>
static_assert(name.size() == 7);
static_assert(name.capacity() == 7);

// User-defined literal
using namespace sertial::literals;
constexpr auto msg = "RealTime"_fs;  // fixed_string<8>
static_assert(msg.size() == 8);

// NTTP (Non-Type Template Parameter)
constexpr auto project = sertial::make_fixed<"Library">();  // fixed_string<7>

// Use in compile-time configuration
struct Config {
    sertial::fixed_string<32> name;
    uint32_t timeout_ms;
    bool enabled;
};

constexpr Config runtime_config{
    .name = "Sensor"_fs,      // Compile-time construction
    .timeout_ms = 100,
    .enabled = true
};

int main() {
    // Runtime copy of compile-time config
    Config active = runtime_config;
    
    auto buffer = sertial::serialize(active);
    std::cout << "Config size: " << buffer.size() << " bytes\n";
    
    return 0;
}
```

---

### RingBuffer - Circular History

```cpp
#include <sertial/sertial.hpp>
#include <sertial/containers/ring_buffer.hpp>

struct TemperatureHistory {
    uint32_t sensor_id;
    sertial::RingBuffer<float, 100> last_readings;
};

int main() {
    TemperatureHistory history;
    history.sensor_id = 1;
    
    // Simulate 150 readings (buffer capacity is 100)
    for (int i = 0; i < 150; ++i) {
        history.last_readings.push_back(20.0f + i * 0.1f);
    }
    
    // Only last 100 readings are kept (oldest 50 overwritten)
    std::cout << "Buffer size: " << history.last_readings.size() << "\n";
    std::cout << "Oldest reading: " << history.last_readings.front() << "\n";
    std::cout << "Newest reading: " << history.last_readings.back() << "\n";
    
    // Serialize current state
    auto buffer = sertial::serialize(history);
    // Size: 4 + 4 + 100*4 = 408 bytes
    
    auto restored = sertial::deserialize<TemperatureHistory>(buffer.view());
    std::cout << "Restored " << restored->last_readings.size() 
              << " readings\n";
    
    return 0;
}
```

**Output**:
```
Buffer size: 100
Oldest reading: 25.0
Newest reading: 34.9
Restored 100 readings
```

---

### static_buffer - Raw Byte Buffer

```cpp
#include <sertial/sertial.hpp>
#include <sertial/containers/static_buffer.hpp>

int main() {
    // static_buffer is typically the RESULT of serialization
    Point3D point{1.0f, 2.0f, 3.0f};
    auto buffer = sertial::serialize(point);  // Returns static_buffer<12>
    
    // Access serialized bytes
    std::cout << "Buffer capacity: " << buffer.capacity() << " bytes\n";
    std::cout << "Buffer used: " << buffer.size() << " bytes\n";
    
    // Get zero-copy view for transmission
    std::span<const std::byte> view = buffer.view();
    // socket.send(view.data(), view.size());
    
    // Manual buffer usage
    sertial::static_buffer<1024> manual_buf;
    std::size_t size = sertial::serialize_to(point, manual_buf.data());
    manual_buf.resize(size);
    
    std::cout << "Manual buffer used: " << manual_buf.size() << " bytes\n";
    
    return 0;
}
```

---

## Advanced Examples

### Message with Header Pattern

```cpp
#include <sertial/sertial.hpp>
#include <sertial/containers/fixed_string.hpp>

struct Header {
    uint64_t timestamp;
    uint32_t sequence_number;
    uint32_t frame_id;
};

struct NamedMeasurement {
    Header header;
    sertial::fixed_string<64> sensor_name;
    float value;
    float confidence;
};

int main() {
    uint32_t seq = 0;
    
    NamedMeasurement msg;
    msg.header.timestamp = get_timestamp();
    msg.header.sequence_number = seq++;
    msg.header.frame_id = 1;
    msg.sensor_name = "temperature_01";
    msg.value = 25.5f;
    msg.confidence = 0.95f;
    
    auto buffer = sertial::serialize(msg);
    // Transmit buffer.view()
    
    return 0;
}
```

---

### Multiple Message Types

```cpp
#include <sertial/sertial.hpp>
#include <variant>

struct TemperatureMsg {
    uint32_t sensor_id;
    float temperature;
};

struct PressureMsg {
    uint32_t sensor_id;
    float pressure;
};

struct HumidityMsg {
    uint32_t sensor_id;
    float humidity;
};

// Type tag for runtime dispatch
enum class MsgType : uint8_t {
    Temperature = 1,
    Pressure = 2,
    Humidity = 3
};

struct GenericMessage {
    MsgType type;
    sertial::static_buffer<64> payload;
};

int main() {
    // Serialize temperature
    TemperatureMsg temp{42, 25.5f};
    auto temp_buf = sertial::serialize(temp);
    
    // Pack into generic message
    GenericMessage gen_msg;
    gen_msg.type = MsgType::Temperature;
    std::memcpy(gen_msg.payload.data(), temp_buf.data(), temp_buf.size());
    gen_msg.payload.resize(temp_buf.size());
    
    // Transmit/deserialize...
    if (gen_msg.type == MsgType::Temperature) {
        auto restored = sertial::deserialize<TemperatureMsg>(
            gen_msg.payload.view()
        );
        std::cout << "Temperature: " << restored->temperature << "°C\n";
    }
    
    return 0;
}
```

---

### Compile-Time Size Introspection

```cpp
#include <sertial/sertial.hpp>
#include <sertial/core/layout/struct_layout.hpp>

struct SimpleMsg {
    uint32_t id;
    float value;
};

struct VariableMsg {
    uint32_t id;
    sertial::fixed_vector<float, 100> data;
};

int main() {
    using SimpleLayout = sertial::StructLayout<SimpleMsg>;
    using VariableLayout = sertial::StructLayout<VariableMsg>;
    
    // Compile-time introspection
    std::cout << "SimpleMsg:\n";
    std::cout << "  base_packed_size: " << SimpleLayout::base_packed_size << "\n";
    std::cout << "  max_packed_size: " << SimpleLayout::max_packed_size << "\n";
    std::cout << "  has_variable_fields: " << SimpleLayout::has_variable_fields << "\n";
    
    std::cout << "\nVariableMsg:\n";
    std::cout << "  base_packed_size: " << VariableLayout::base_packed_size << "\n";
    std::cout << "  max_packed_size: " << VariableLayout::max_packed_size << "\n";
    std::cout << "  has_variable_fields: " << VariableLayout::has_variable_fields << "\n";
    
    // Compile-time assertions
    static_assert(!SimpleLayout::has_variable_fields);
    static_assert(VariableLayout::has_variable_fields);
    static_assert(SimpleLayout::base_packed_size == 8);
    static_assert(VariableLayout::base_packed_size == 4);
    static_assert(VariableLayout::max_packed_size == 4 + 4 + 100*4);
    
    return 0;
}
```

**Output**:
```
SimpleMsg:
  base_packed_size: 8
  max_packed_size: 8
  has_variable_fields: 0

VariableMsg:
  base_packed_size: 4
  max_packed_size: 408
  has_variable_fields: 1
```

---

## Real-World Patterns

### Real-Time Periodic Publisher

```cpp
#include <sertial/sertial.hpp>
#include <sertial/containers/fixed_vector.hpp>
#include <chrono>
#include <thread>

struct PeriodicData {
    uint64_t timestamp;
    uint32_t sequence_number;
    sertial::fixed_vector<float, 10> samples;
};

void periodic_publisher(int hz) {
    PeriodicData data;
    uint32_t seq = 0;
    auto period = std::chrono::milliseconds(1000 / hz);
    
    while (running) {
        // Update timestamp and sequence
        data.timestamp = get_timestamp_ns();
        data.sequence_number = seq++;
        
        // Refill samples (reuse container)
        data.samples.clear();
        for (int i = 0; i < 10; ++i) {
            data.samples.push_back(read_sensor());
        }
        
        // Serialize (zero allocation)
        auto buffer = sertial::serialize(data);
        
        // Transmit (zero-copy view)
        socket.send(buffer.view().data(), buffer.size());
        
        std::this_thread::sleep_for(period);
    }
}
```

---

### Batch Processing with Reuse

```cpp
#include <sertial/sertial.hpp>
#include <vector>

struct BatchItem {
    uint32_t id;
    float value;
};

void process_batch(const std::vector<RawData>& raw_data) {
    BatchItem item;  // Reuse object
    sertial::static_buffer<1024> buffer;  // Reuse buffer
    
    for (const auto& raw : raw_data) {
        // Update item
        item.id = raw.id;
        item.value = compute(raw);
        
        // Serialize into reused buffer
        std::size_t size = sertial::serialize_to(item, buffer.data());
        buffer.resize(size);
        
        // Process
        database.store(buffer.view());
    }
}
```

---

### Network Packet Handler

```cpp
#include <sertial/sertial.hpp>
#include <span>

struct Packet {
    uint32_t packet_id;
    uint64_t timestamp;
    sertial::fixed_vector<uint8_t, 1400> payload;  // MTU-sized
};

bool handle_packet(std::span<const std::byte> network_data) {
    // Validate minimum size
    if (network_data.size() < sizeof(uint32_t)) {
        return false;
    }
    
    // Deserialize
    auto packet = sertial::deserialize<Packet>(network_data);
    if (!packet) {
        return false;
    }
    
    // Business logic validation
    if (packet->packet_id == 0) {
        return false;
    }
    
    // Process payload
    process_payload(packet->payload);
    return true;
}
```

---

### Historical Data Snapshot

```cpp
#include <sertial/sertial.hpp>
#include <sertial/containers/ring_buffer.hpp>
#include <fstream>

struct SensorSnapshot {
    uint64_t snapshot_time;
    uint32_t sensor_id;
    sertial::RingBuffer<float, 1000> history;
};

void save_snapshot(const SensorSnapshot& snapshot, const char* filename) {
    // Serialize
    auto buffer = sertial::serialize(snapshot);
    
    // Save to disk
    std::ofstream file(filename, std::ios::binary);
    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
}

std::optional<SensorSnapshot> load_snapshot(const char* filename) {
    // Read file
    std::ifstream file(filename, std::ios::binary);
    std::vector<std::byte> data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
    
    // Deserialize
    return sertial::deserialize<SensorSnapshot>(std::span{data});
}
```

---

## Performance Examples

### Zero-Allocation Loop

```cpp
#include <sertial/sertial.hpp>

struct Message {
    uint64_t timestamp;
    uint32_t id;
    float values[3];
};

void high_frequency_loop() {
    Message msg;
    
    for (int i = 0; i < 1000000; ++i) {
        // Update message
        msg.timestamp = get_timestamp();
        msg.id = i;
        msg.values[0] = i * 0.1f;
        msg.values[1] = i * 0.2f;
        msg.values[2] = i * 0.3f;
        
        // Serialize (stack buffer, zero allocation)
        auto buffer = sertial::serialize(msg);
        
        // Process (zero-copy view)
        process(buffer.view());
    }
    // NO heap allocations throughout!
}
```

---

### Benchmark Serialization

```cpp
#include <sertial/sertial.hpp>
#include <chrono>

struct BenchmarkMsg {
    uint64_t timestamp;
    uint32_t seq;
    float data[100];
};

void benchmark() {
    constexpr int ITERATIONS = 100000;
    BenchmarkMsg msg{};
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < ITERATIONS; ++i) {
        msg.seq = i;
        auto buffer = sertial::serialize(msg);
        // Prevent optimization
        volatile auto size = buffer.size();
        (void)size;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Serializations: " << ITERATIONS << "\n";
    std::cout << "Total time: " << duration.count() << " µs\n";
    std::cout << "Per serialization: " 
              << (duration.count() / (double)ITERATIONS) << " µs\n";
}
```

---

## Complete Example Programs

The repository includes complete, compilable examples:

### serialization_example.cpp

Comprehensive demonstration of all API features:
- Basic serialize/deserialize
- Container usage (fixed_vector, fixed_string)
- Compile-time size analysis
- Performance benchmarks

**Build and run**:
```bash
cd build
./serialization_example
```

### fixed_string_compile_time.cpp

Compile-time string literal features (NEW in v2.0.0):
- CTAD (auto size deduction)
- NTTP (non-type template parameters)
- User-defined literals
- Constexpr operations
- String literal integration

**Build and run**:
```bash
cd build
./fixed_string_compile_time
```

### ring_buffer_example.cpp

Real-world RingBuffer usage for message history:
- FIFO overflow behavior
- Timestamp-based retrieval
- Historical data management

**Build and run**:
```bash
cd build
./ring_buffer_example
```

### schema_example.cpp

Schema generation and export:
- JSON schema export
- Multiple message types
- Interactive viewer integration

Build and run:
```bash
cd build
./schema_example my_schemas.json
# Open tools/sertial-inspect/viewer.html with my_schemas.json
```

---

## Next Steps

- **Learn container details**: [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md)
- **Understand serialization**: [SERIALIZATION_MECHANISM.md](SERIALIZATION_MECHANISM.md)
- **Visualize schemas**: [SCHEMA_VIEWER.md](SCHEMA_VIEWER.md)
- **Add custom containers**: [ADDING_CONTAINERS.md](ADDING_CONTAINERS.md)

---

**More examples needed?** Open an issue: [https://github.com/mattih11/SeRTial](https://github.com/mattih11/SeRTial)
