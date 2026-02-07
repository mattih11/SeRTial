<div align="center">
  <img src="docs/SeRTial.png" alt="SeRTial Logo" width="400"/>
</div>

# SeRTial

A high-performance, zero-allocation C++20 binary serialization library using compile-time reflection with [reflect-cpp](https://github.com/getml/reflect-cpp).

## License

Copyright (C) 2026 Matthias Haase <mattihaae@proton.me>

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

## Key Features

- **Zero Runtime Allocation**: Stack-allocated buffers with compile-time max size computation
- **Compile-Time Reflection**: Automatic struct serialization via reflect-cpp
- **Variable-Size Fields**: Support for `fixed_vector<T,N>`, `fixed_string<N>`, and `RingBuffer<T,N>` with runtime sizes
- **StructLayout<T>**: Single source of truth for compile-time type analysis and serialization
- **Block-Based Optimization**: Efficient block execution for optimal performance
- **Simple API**: `serialize(obj)` / `deserialize<T>(data)` - zero boilerplate

## Current Status

### Phase 1: Core Functionality (Complete)
- Fixed-size struct serialization with automatic padding elimination
- Variable-size field support (fixed_vector, fixed_string, RingBuffer)
- StructLayout<T> as single source of truth for type analysis
- Block-based serialization with symmetric operations
- Comprehensive documentation and test coverage

### Phase 2: Concept-Based Container Registration (Complete)
- **C++20 Concepts**: `SerializableContainer` concept for automatic container detection
- **Single Registration Point**: New containers work immediately by satisfying the concept interface
- **Zero Duplication**: Eliminated 9+ manual trait specializations per container type
- **Clear Error Messages**: Compiler shows exactly which interface requirement is missing
- **Backward Compatibility**: Legacy trait APIs still work (internally delegate to concepts)

**See**: `docs/TEMPLATE_PATTERNS.md` for metaprogramming patterns and `docs/CONTAINER_HANDLING.md` for container integration guide.

### Phase 3: Generic Span-Based Serialization (Complete)
- **RingBuffer Serialization**: Full wrap-around handling with automatic span decomposition
- **Generic Architecture**: Zero container-specific branches via `serialization_view_provider<T>`
- **Schema Transparency**: Complete metadata export at field and block levels
  - Field level: `container_type`, `overflow_behavior`, `serialization_order`
  - Block level: `span_based_serialization`, `max_span_count`
- **Python Viewer Support**: CLI and GUI display container characteristics and multi-span info
- **Single Registration Point**: `container_registration.hpp` - add new containers with minimal code

**Key Achievement**: RingBuffer returns 1-2 spans based on wrap-around state, fixed_vector returns 1 span (contiguous). Schema shows exactly how many memcpy operations each container needs.

### Phase 4: Reflector-Based Schema Export (Complete)
- **Zero-Boilerplate Introspection**: Compile-time metadata automatically exposed via `rfl::Reflector`
- **Container Reflectors**: `fixed_vector`, `fixed_string`, `RingBuffer` teach reflect-cpp how to generate schemas
- **StructLayout Reflector**: Single source exports all metadata (20+ fields) with one line: `rfl::json::to_schema<StructLayout<T>>()`
- **Compile-Time Safeguards**: Field count assertions, member existence checks prevent synchronization bugs
- **No Runtime Parsing**: Schema generation is direct - no manual JSON manipulation needed

**Key Innovation**: Teaching reflect-cpp to "see" our compile-time metadata eliminates ~300 lines of manual extraction code while providing stronger type safety and automatic field name/type introspection.

**See**: `docs/REFLECTOR_BASED_SCHEMA.md` for the complete architecture and design rationale.

### Future Work (Phase 5+)
- Cross-platform serialization (endianness handling, portable padding)
- Nested container support (fixed_vector<fixed_vector<T, M>, N>)
- Performance profiling tools
- Additional container types as needed

## Quick Start

```cpp
#include <sertial/sertial.hpp>

struct Player {
    uint32_t id;
    float health;
    float x, y, z;
};

int main() {
    using namespace sertial;
    
    Player player{42, 100.0f, 1.5f, 2.5f, 3.5f};
    
    // Serialize (zero heap allocations - stack buffer)
    auto buffer = serialize(player);
    
    // Deserialize
    auto restored = deserialize<Player>(buffer.view());
    
    // Compile-time analysis
    static_assert(Message<Player>::base_packed_size == 20);
    static_assert(!Message<Player>::has_variable_fields);
}
```

## Prerequisites

- CMake 3.20 or higher
- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- reflect-cpp installed on your system

### Installing reflect-cpp

```bash
git clone https://github.com/getml/reflect-cpp.git
cd reflect-cpp
mkdir build && cd build
cmake ..
cmake --build .
sudo cmake --install .
```

## Building SeRTial

```bash
git clone <repository-url>
cd SeRTial
mkdir build && cd build
cmake ..
cmake --build .
```

## Installing SeRTial

SeRTial is a header-only library and can be installed system-wide:

```bash
cd build
sudo cmake --install .
```

This installs:
- **Headers**: `/usr/local/include/sertial/` - All library headers
- **Examples**: `/usr/local/include/sertial/examples/` - Example message types for reference
- **CMake Config**: `/usr/local/lib/cmake/SeRTial/` - For `find_package(SeRTial)`
- **CLI Tool**: `/usr/local/bin/sertial-inspect` - Terminal-based schema inspection
- **HTML Viewer**: `/usr/local/share/sertial/sertial-inspect/viewer.html` - Interactive browser-based viewer

### Using Installed SeRTial

In your project's CMakeLists.txt:

```cmake
find_package(SeRTial REQUIRED)
target_link_libraries(your_target PRIVATE SeRTial::sertial)
```

### Using the Inspection Tools

After installation:

```bash
# Generate schema from your app
./your_app --generate-schema my_schemas.json

# Inspect via CLI (terminal-based)
sertial-inspect my_schemas.json --summary
sertial-inspect my_schemas.json --message "TypeName"

# View in browser (interactive HTML viewer)
# Open /usr/local/share/sertial/sertial-inspect/viewer.html in your browser
# Use file picker or URL parameter: viewer.html?schema=path/to/schemas.json
```

## Running Tests

```bash
cd build

# Individual test suites
./test_foundation       # Core containers and type traits
./test_serialization    # High-level serialize/deserialize API
./test_padding          # Padding analysis
./test_hybrid_binary    # Block-based serialization with variable-size fields
./test_endianness       # Endianness conversion

# Run all tests
make run_tests
```

## Interactive Schema Viewer

SeRTial includes a web-based interactive schema viewer for visualizing your message structures:

**[Launch Interactive Viewer](https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html?schema=https://raw.githubusercontent.com/mattih11/SeRTial/main/examples/schemas/example_schemas.json)**

### Features
- **Visual Memory Layouts**: See struct vs. serialized layout side-by-side
- **Variable Field Controls**: Interactive sliders to adjust runtime sizes
- **Animation Mode**: Watch how packed size changes dynamically
- **Hover Highlighting**: Highlight fields across table, layouts, and operations
- **Collapsible Sections**: Fold/unfold different visualizations
- **Block Operations**: See exact memcpy operations for serialize/deserialize

### Usage

```bash
# Generate schema from your types
cd build
./schema_example  # Creates my_schemas.json
cp my_schemas.json ../examples/schemas/example_schemas.json

# Option 1: View in browser (no server needed for local files)
# Open tools/sertial-inspect/viewer.html in your browser
# Use file picker to load schema JSON

# Option 2: Serve locally for URL parameter support
cd ../tools/sertial-inspect
python3 -m http.server 8080
# Open: http://localhost:8080/viewer.html?schema=../../examples/schemas/example_schemas.json

# Option 3: Use CLI tool for terminal viewing
cd ../../build
./sertial-inspect ../examples/schemas/example_schemas.json --summary
```

See [`tools/sertial-inspect/README.md`](tools/sertial-inspect/README.md) for detailed usage, URL parameters, and GitHub Pages embedding instructions.

## Examples Guide

SeRTial includes several example programs demonstrating different aspects of the library.

### Example 1: Basic Serialization (`examples/serialization_example.cpp`)

This comprehensive example demonstrates six core scenarios:

```bash
cd build
./serialization_example
```

**Six example functions covering:**

1. **Basic Serialization** - Zero-allocation serialization
   ```cpp
   Position<> pos;
   pos.header.seq = 42;
   pos.pose.position = Point3D<>{1.5f, 2.5f, 3.5f};
   
   // serialize() returns a static_buffer (stack-allocated)
   auto buffer = serialize(pos);
   // buffer.view() gives std::span<const std::byte>
   // buffer.size() returns actual bytes written
   ```

2. **Deserialization** - Restore objects from binary data
   ```cpp
   auto restored = deserialize<Position<>>(buffer.view());
   if (restored) {
       // Use restored->header, restored->pose, etc.
   }
   ```

3. **Memory Analysis** - Compile-time layout inspection
   ```cpp
   // All computed at compile-time, zero runtime cost
   using Layout = Message<Position<>>;
   constexpr auto max_size = Layout::max_packed_size;
   constexpr auto has_var = HMM::has_variable_fields;
   constexpr auto regions = HMM::fixed_count;
   ```

4. **Static Buffers** - Pre-allocated reusable buffers
   ```cpp
   // Pre-defined buffer sizes for reuse
   static_buffer_128 buffer;  // 128-byte stack buffer
   serialize_to(pos, buffer);
   
   // Reuse same buffer for multiple serializations
   buffer.clear();
   serialize_to(other_pos, buffer);
   ```

5. **Performance** - Benchmarking serialization speed
   ```cpp
   // Measures throughput and latency
   auto bench = [&] { serialize(pos); };
   // Typical: 10-15 ns/op for small structs
   ```

6. **Error Handling** - Safe deserialization with validation
   ```cpp
   std::vector<std::byte> truncated = {std::byte{0x01}};
   auto result = deserialize<Position<>>(truncated);
   if (!result) {
       // Deserialization failed safely, no crash
   }
   ```

### Example 2: Schema Generation (`examples/schema_example.cpp`)

Generate JSON schemas describing your message types:

```bash
cd build
./schema_example ../scripts/message_schemas.json
```

This creates a JSON file with:
- Field names, types, sizes, offsets
- Padding information
- Memcpy regions for optimization
- Nested structure analysis

### Example 3: Runtime Testing (`examples/test_example.cpp`)

Verify message round-trip integrity:

```bash
cd build
./test_example
```

This demonstrates:
- Automatic round-trip verification
- Statistics collection
- Error detection

### Example 4: Main Demo (`src/main.cpp`)

Interactive demonstration of all features:

```bash
cd build
./SeRTial
```

Shows:
- Type analysis output
- Serialization/deserialization
- Padding detection and optimization
- Nested struct handling

## API Reference

### Core Functions

```cpp
namespace sertial {

// Serialize a value to a stack-allocated buffer
// Returns static_buffer<max_packed_size> where max size is computed at compile-time
template<typename T>
auto serialize(const T& value) -> static_buffer<Message<T>::max_packed_size>;

// Serialize into an existing buffer (returns actual bytes written)
template<typename T>
std::size_t serialize_to(const T& value, std::byte* dest);

// Deserialize from bytes
template<typename T>
std::optional<T> deserialize(std::span<const std::byte> data);

}
```

### Message<T> - High-Level API

```cpp
template<typename T>
struct Message {
    // Size information
    static constexpr std::size_t max_packed_size;   // Maximum serialized size
    static constexpr std::size_t base_packed_size;  // Size of fixed fields only
    
    // Type characteristics
    static constexpr bool has_variable_fields;      // Contains vectors/strings?
    static constexpr std::size_t field_count;       // Number of fields
    
    using buffer_type = std::array<std::byte, max_packed_size>;
    
    // Simple API
    static Result<buffer_type> serialize(const T& value);
    static DeserializeResult<T> deserialize(std::span<const std::byte> data);
    
    // Advanced API
    static std::size_t serialize_to(const T& value, buffer_type& buffer);
    static std::size_t calculate_packed_size(const T& value);
};
```

### StructLayout<T> - Advanced Direct API

For hot paths or when you need maximum control:

```cpp
template<typename T>
struct StructLayout {
    // Same compile-time info as Message<T>
    static constexpr std::size_t max_packed_size;
    static constexpr std::size_t base_packed_size;
    static constexpr bool has_variable_fields;
    
    using buffer_type = std::array<std::byte, max_packed_size>;
    
    // Direct serialization (no Result wrapper)
    static std::size_t serialize(const T& obj, std::span<std::byte, max_packed_size> dest);
    static std::size_t serialize(const T& obj, std::span<std::byte> dest);  // Returns 0 on error
    
    // Direct deserialization (returns optional)
    static std::optional<T> deserialize_opt(std::span<const std::byte> src);
    
    // Runtime size calculation
    static std::size_t calculate_packed_size(const T& obj);
};
```

**When to use which:**
- **Message<T>**: Simple usage, prototyping, convenience wrappers (Result types)
- **StructLayout<T>**: Hot paths, embedded systems, when you need direct control

Both provide identical compile-time guarantees and zero-allocation serialization.
    
    // Convenience wrapper (returns static_buffer)
    static auto to_buffer(const T& value);
    
    // Deserialization
    static DeserializeResult<T> deserialize(std::span<const std::byte> data);
};
```

### Bounded Containers

For zero-allocation serialization, use bounded container types:

```cpp
#include <sertial/containers/fixed_string.hpp>
#include <sertial/containers/fixed_vector.hpp>

struct GameMessage {
    fixed_string<32> player_name;      // Max 32 chars
    fixed_vector<int32_t, 16> scores;  // Max 16 elements
    fixed_vector<Vec3, 100> waypoints; // Max 100 points
};
```

### Ring Buffer - Realtime-Safe Circular Buffer

Fixed-capacity circular buffer for realtime systems (perfect for message history):

```cpp
#include <sertial/containers/ring_buffer.hpp>

// Create buffer with 100-message capacity
RingBuffer<TimsMessage, 100> buffer;

// Push messages (O(1), no allocation)
buffer.push_back(msg);  // Automatically overwrites oldest when full

// Access elements
auto oldest = buffer.front();  // Oldest message
auto newest = buffer.back();   // Newest message
auto at_idx = buffer[5];       // By index (0 = oldest)

// Query
size_t count = buffer.size();
bool is_full = buffer.full();

// Iterate oldest to newest
for (const auto& msg : buffer) {
    process(msg);
}

// CommRaT-style timestamp lookup
std::optional<TimsMessage> find_at_timestamp(uint64_t target_ts) {
    size_t best_idx = 0;
    uint64_t min_diff = UINT64_MAX;
    
    for (size_t i = 0; i < buffer.size(); ++i) {
        uint64_t diff = std::abs(static_cast<int64_t>(
            buffer[i].timestamp - target_ts));
        if (diff < min_diff) {
            min_diff = diff;
            best_idx = i;
        }
    }
    return buffer[best_idx];
}
```

**Features:**
- **Zero allocation**: Fixed capacity determined at compile-time
- **Realtime-safe**: No malloc/free, deterministic O(1) operations
- **Circular overwrite**: Oldest elements automatically replaced when full
- **STL-compatible**: Forward iterators, range-based for loops
- **Thread-safe wrapper ready**: Use with `std::shared_mutex` for multi-reader access

**Example:** `examples/ring_buffer_example.cpp` - CommRaT-style message buffering with timestamp-based retrieval

### Static Buffers

Pre-allocated buffer types for reuse:

```cpp
#include <sertial/containers/static_buffer.hpp>

// Pre-defined sizes
static_buffer_64 small;    // 64 bytes
static_buffer_128 medium;  // 128 bytes
static_buffer_256 large;   // 256 bytes
static_buffer_1k big;      // 1024 bytes
static_buffer_4k huge;     // 4096 bytes

// Custom size
static_buffer<512> custom; // 512 bytes
```

### Debug Utilities

```cpp
#include <sertial/debug/print_utils.hpp>

using namespace sertial::debug;

// Print hex bytes (produces stdout output)
print_hex(buffer.view());           // "2a 00 00 00 ..."
print_bytes(buffer.view());         // "  [20 bytes]: 2a 00 00 00 ..."

// Type analysis (produces stdout output)
print_type_info<Player>("Player");  // Detailed type info

// String conversion (no output)
auto hex = to_hex_string(buffer.view());  // Returns string

// Benchmarking
print_benchmark("serialize", 100000, [&]{ serialize(player); });
```

## Project Structure

```
SeRTial/
├── include/sertial/
│   ├── sertial.hpp              # Main include header
│   ├── message.hpp              # Message<T> API
│   ├── core/
│   │   ├── concepts.hpp         # C++20 concepts for type constraints
│   │   ├── endian.hpp           # Endianness conversion utilities
│   │   │   ├── traits.hpp           # Type trait aggregations
│   │   └── layout/
│   │       └── struct_layout.hpp    # Compile-time type analysis (single source of truth)
│   ├── containers/
│   │   ├── container_registration.hpp  # SerializableContainer concept
│   │   ├── fixed_string.hpp     # Bounded string (stack-allocated)
│   │   ├── fixed_vector.hpp     # Bounded vector (stack-allocated)
│   │   ├── ring_buffer.hpp      # Circular buffer (FIFO)
│   │   ├── static_buffer.hpp    # Stack-allocated byte buffers
│   │   └── reflectors.hpp       # Container reflection for schema export
│   ├── io/
│   │   └── unified_binary.hpp   # Block-based serialization
│   ├── integration/
│   │   ├── runtime_test.hpp     # Round-trip testing framework
│   │   └── schema_export.hpp    # JSON schema generation (reflector-based)
│   ├── traits/
│   │   └── container_detection.hpp  # Container type name mapping
│   └── debug/
│       └── print_utils.hpp      # Debug output utilities
├── src/
│   └── main.cpp                 # Demo application
├── examples/
│   ├── serialization_example.cpp  # Core serialization examples
│   ├── schema_example.cpp         # Schema generation demo
│   ├── test_example.cpp           # Runtime testing demo
│   ├── defines/                   # Example type definitions
│   │   ├── point3d.hpp
│   │   ├── quaternion.hpp
│   │   └── timestamp.hpp
│   └── messages/                  # Example message types
│       ├── header.hpp
│       ├── position.hpp
│       ├── pointcloud.hpp
│       ├── camera.hpp
│       └── imu.hpp
├── test/
│   ├── test_foundation.cpp      # Container and traits tests
│   ├── test_serialization.cpp   # High-level API tests
│   └── test_hybrid_binary.cpp   # Block-based serialization tests
└── tools/
    └── sertial-inspect/
        ├── viewer.html          # Interactive browser-based viewer
        ├── README.md            # Viewer documentation
        └── main.cpp             # C++ CLI tool source
```

## Architecture

### Design Philosophy

SeRTial uses a **unified block-based serialization** approach that handles both fixed-size and variable-size types through a single code path:

1. **StructLayout<T>**: Analyzes struct layout at compile-time and generates a block execution plan
2. **Block Types**:
   - **Fixed blocks**: Contiguous runs of fixed-size fields (copied via memcpy)
   - **Padding blocks**: Gaps between fields (skipped during serialization)
   - **Dynamic blocks**: Variable-size containers with 4-byte length prefix
   - **RuntimeOffset blocks**: Fixed-size fields after dynamic content (position varies at runtime)

3. **Unified Serialization**: Single `serialize()` function executes the block plan
   - Fixed-only types: Direct memcpy of fixed blocks
   - Variable-size types: Copy fixed blocks → serialize dynamic blocks → copy runtime offset blocks

### Key Optimizations

- **Padding Elimination**: Serialized format is tightly packed (no alignment gaps)
  - Example: `Header` struct with padding: 32 bytes in memory → 24 bytes serialized
- **Zero Allocation**: All buffers are stack-allocated with compile-time max size
- **Block Execution**: Contiguous fields copied in single memcpy operations
- **Compile-Time Analysis**: All type information computed at compile-time (zero runtime overhead)

### How Variable-Size Fields Work

For types with `fixed_vector<T,N>` or `fixed_string<N>`:

```cpp
struct WithVariable {
    uint32_t id;                  // Fixed block: 4 bytes
    fixed_vector<uint16_t, 100> values;  // Dynamic block: 4-byte length + N×2 bytes
    uint64_t timestamp;           // RuntimeOffset block: 8 bytes (position varies)
};
```

**Serialization format:**
```
[id: 4 bytes][length: 4 bytes][values: N×2 bytes][timestamp: 8 bytes]
```

- `base_packed_size = 4` (bytes before first dynamic field)
- `calculate_packed_size(obj) = 4 + 4 + N×2 + 8`
- `max_packed_size = 4 + 4 + 100×2 + 8 = 216` (worst case)

## Performance

SeRTial achieves high performance through several key design decisions:

### Zero-Overhead Design

1. **Zero Heap Allocation**: All buffers are stack-allocated with compile-time sizing
2. **Optimized Memcpy**: Contiguous fields are copied in single operations (block-based execution)
3. **Compile-Time Analysis**: All type information and layout computed at compile-time
4. **Padding Elimination**: Only actual data bytes are serialized (no alignment gaps)
5. **Direct Memory Operations**: No intermediate buffering or copying

### Typical Performance Characteristics

On modern x86-64 hardware (example measurements):

**Fixed-size types (pure memcpy):**
- Serialize: ~10-15 ns/op for small structs (< 100 bytes)
- Deserialize: ~50-100 ns/op (includes validation)
- Throughput: 50-100 million messages/second

**Variable-size types:**
- Overhead: +4 bytes per dynamic field (length prefix)
- Performance: Depends on element count and block layout
- Small vectors (< 10 elements): ~20-30 ns/op additional

### Benchmarking

Use the serialization_example to measure performance on your hardware:

```bash
cd build
./serialization_example  # Includes performance benchmark section
```

The example includes timing measurements for:
- Single serialization operations
- Round-trip (serialize + deserialize)
- Multiple iterations to measure sustained throughput

## Introspection & Visualization

SeRTial provides **zero-boilerplate introspection** through an innovative reflector-based schema system. Your compile-time metadata is automatically exported to JSON with complete type information - no manual code required.

### How It Works: The Reflector Magic

SeRTial uses `rfl::Reflector` specializations to teach reflect-cpp how to "see" compile-time metadata:

1. **Container Reflectors** (`include/sertial/containers/reflectors.hpp`):
   - `fixed_vector<T,N>` → reflected as `std::vector<T>` for schemas
   - `fixed_string<N>` → reflected as `std::string`
   - `RingBuffer<T,N>` → reflected as `std::vector<T>`
   
2. **StructLayout Reflector** (`include/sertial/core/layout/struct_layout_reflector.hpp`):
   - Exposes all 20+ compile-time metadata fields automatically
   - Field names, types, sizes, offsets, capacities, block counts, packed sizes
   - Includes nested schema of the actual type T
   
3. **One-Line Export** (`include/sertial/integration/schema_export.hpp`):
   ```cpp
   template<typename T>
   std::string export_schema() {
       return rfl::json::to_schema<StructLayout<T>>();  // That's it!
   }
   ```

**Result**: Complete JSON schemas with all metadata - field names, types, offsets, capacities, block execution order - generated automatically from your C++ types.

**See `docs/REFLECTOR_BASED_SCHEMA.md`** for the complete architecture, comparison with manual approaches, and why this is a breakthrough in C++ introspection.

### Workflow: Schema Generation and Viewing

1. **Schema Generation**: The C++ example `schema_example.cpp` (or the `make generate_schemas` target) generates a JSON schema file (typically `message_schemas.json`) describing all registered message types, including field names, types, sizes, offsets, padding, and memcpy regions.

2. **Visualization**: The viewer tools (C++ CLI tool `sertial-inspect` or HTML viewer `viewer.html`) read this JSON schema file to provide interactive or terminal-based exploration of your message layouts.

3. **Automated Workflow**: The `make viewer` target automates this process:
    - Runs the schema generator to produce the latest `message_schemas.json`
    - Launches the GUI viewer with the generated schema file

This ensures that the viewer always reflects the current state of your message types.

#### Example: Full Workflow

```bash
cd build
make viewer
# (generates message_schemas.json and opens the GUI viewer)
```

#### Manual Usage

You can also generate schemas and launch viewers manually:

```bash
# Generate schema JSON
./schema_example  # Creates my_schemas.json
cp my_schemas.json ../examples/schemas/example_schemas.json

# View in browser
# Open ../tools/sertial-inspect/viewer.html in your browser

# Or use the CLI tool
./sertial-inspect ../examples/schemas/example_schemas.json --summary
```

### GUI Viewer Features

The HTML viewer (`tools/sertial-inspect/viewer.html`) provides:

- **Message Browser**: Browse all message types by category
  - `[1]` = Single memcpy (fixed-size, fastest)
  - `o` = Multiple memcpy regions (field splitting)
  - `P` = Has padding between fields
  - `~` = Has variable-size fields (new!)
- **Memory Layout Visualization**: See struct layout with color-coded fields
- **Padding Analysis**: Visual highlighting of padding bytes between fields
- **Memcpy Region Display**: Shows optimized copy regions for serialization
- **Field Details Table**: Offsets, sizes, types, and padding information

#### Interactive Variable-Size Field Controls (NEW!)

For messages with variable-size fields (vectors, strings, etc.):

- **Interactive Sliders**: Adjust element counts for each variable field
- **Runtime Size Calculator**: Real-time display of serialized size based on slider values
- **Dynamic Layout Visualization**: Three memory bars show:
  1. **Struct layout**: In-memory representation with maximum capacity
  2. **Max capacity**: Serialized with all containers at maximum size
  3. **Runtime layout**: Actual serialized format based on current slider values
- **Block Execution Order**: Visual representation of serialization blocks:
  - Fixed blocks (blue/green): Contiguous fixed-size fields
  - Length prefixes (gray): 4-byte headers before dynamic fields
  - Dynamic blocks (striped): Variable-size container data
  - Runtime offset blocks: Fields that shift position based on dynamic content

Example: PointCloud message with `vector<Point3D>`:
- Base size: 32 bytes (header fields)
- Dynamic field: 4-byte length prefix + N×12 bytes (N points)
- Adjust slider from 0 to 256 elements to see size change from 36 to 3108 bytes
- Watch the third memory bar update to show actual serialized layout

### CLI Visualizer

For terminal-based inspection:

```bash
# Summary of all messages
sertial-inspect schemas.json --summary

# Detailed view of specific message
sertial-inspect schemas.json --message "Header<>"

# All messages with full details
sertial-inspect schemas.json --all
```


### Schema Generation API

Generate schemas programmatically:

```cpp
#include <sertial/integration/schema_generator.hpp>

// Register your types
SchemaGenerator gen;
gen.add<MyMessage>("category");
gen.add<AnotherMessage>("category");

// Generate JSON
gen.write_to_file("schemas.json");
```

### Make Targets

| Target | Description |
|--------|-------------|
| `make viewer` | Generate schemas and launch GUI viewer |
| `make visualize` | Generate schemas and print CLI summary |
| `make generate_schemas` | Generate `message_schemas.json` only |

## Adding Custom Container Types

With SeRTial's concept-based registration system (Phase 2), adding new serializable containers is straightforward:

### Step 1: Implement the Interface

Your container must satisfy the `SerializableContainer` concept:

```cpp
template<typename T, std::size_t N>
class MyContainer {
public:
    // Required nested type
    using value_type = T;
    
    // Required compile-time constant (note: max_size_v, not max_size)
    static constexpr std::size_t max_size_v = N;
    
    // Required runtime methods
    constexpr std::size_t size() const noexcept;
    constexpr const T* data() const noexcept;
    
    // Required for deserialization
    constexpr T* data_unsafe() noexcept;
    constexpr void set_size_unsafe(std::size_t n) noexcept;
    
    // Your custom interface...
};
```

### Step 2: Verify and Use

```cpp
static_assert(SerializableContainer<MyContainer<int, 10>>, 
              "MyContainer must satisfy SerializableContainer");

// Use immediately in structs
struct Message {
    MyContainer<float, 100> data;  // Works automatically
};
```

**No manual trait specializations needed!** The concept system handles everything.

**See**: `docs/CONTAINER_HANDLING.md` for complete integration guide and `docs/TEMPLATE_PATTERNS.md` for template patterns.

## Performance

SeRTial achieves high performance through:

1. **Zero Heap Allocation**: All buffers are stack-allocated with compile-time sizing
2. **Optimized Memcpy**: Contiguous fields are copied in single operations
3. **Compile-Time Analysis**: All type information computed at compile-time
4. **Padding Elimination**: Only actual data bytes are serialized

Typical performance (example on modern x86-64):
- Serialize: ~10-15 ns/op for small structs
- Deserialize: ~50-100 ns/op
- Throughput: 50-100 million messages/second

## Contributing

Contributions are welcome! Please ensure:
1. Code follows existing style
2. All tests pass
3. New features include tests
4. Documentation is updated

## Author

Matthias Haase <mattihaae@proton.me>

## Acknowledgments

- [reflect-cpp](https://github.com/getml/reflect-cpp) - Compile-time reflection library
