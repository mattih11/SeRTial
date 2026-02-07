# SeRTial v2.0.0 Release Notes

**Release Date**: February 7, 2026

## Major Release - Complete Overhaul

This is a major release representing 4 development phases with significant architectural improvements, new features, and a complete tooling modernization.

## What's New

### Phase 1: Core Architecture Refinement
- **Unified Block-Based Serialization**: Single code path for fixed and variable-size types
- **StructLayout<T>**: Single source of truth for compile-time type analysis
- **Symmetric Operations**: Block-based serialize/deserialize with identical structure
- **Comprehensive Documentation**: Complete architectural guides and usage examples

### Phase 2: Concept-Based Container Registration
- **C++20 Concepts**: `SerializableContainer` concept for automatic detection
- **Single Registration Point**: New containers work by satisfying the interface
- **Zero Duplication**: Eliminated 9+ manual trait specializations per container
- **Clear Error Messages**: Compiler shows exactly which requirement is missing

### Phase 3: Generic Span-Based Serialization
- **RingBuffer Support**: Full circular buffer serialization with wrap-around handling
- **Generic Architecture**: Zero container-specific branches via `serialization_view_provider<T>`
- **Schema Transparency**: Complete metadata export at field and block levels
- **Multi-Span Serialization**: Automatic 1-2 span decomposition based on memory layout

### Phase 4: Reflector-Based Introspection & Interactive Viewer
- **Zero-Boilerplate Schema Export**: Automatic via `rfl::Reflector` specializations
- **Container Reflectors**: fixed_vector, fixed_string, RingBuffer teach reflect-cpp
- **StructLayout Reflector**: Exposes all 20+ metadata fields automatically
- **Interactive HTML Viewer**: Browser-based, zero dependencies, URL parameter support
- **C++ CLI Tool**: Terminal-based inspection replacing Python dependencies

## New Components

### Libraries & Headers
- `include/sertial/containers/reflectors.hpp` - Container reflection support
- `include/sertial/core/layout/struct_layout_reflector.hpp` - StructLayout introspection
- `include/sertial/containers/ring_buffer.hpp` - Circular buffer implementation

### Tools
- `sertial-inspect` - C++ CLI tool for terminal-based schema inspection
- `tools/sertial-inspect/viewer.html` - Interactive HTML viewer (~1350 lines)
- Example schemas committed: `examples/schemas/example_schemas.json` (30KB, 15 types)

### Documentation
- `docs/REFLECTOR_BASED_SCHEMA.md` - Complete reflector architecture
- `docs/TEMPLATE_PATTERNS.md` - Metaprogramming patterns guide
- `docs/CONTAINER_HANDLING.md` - Container integration guide
- `tools/sertial-inspect/README.md` - Viewer usage documentation

## Breaking Changes

### Removed
- **Python Dependencies**: All Python scripts removed (sertial-gui, sertial-inspect)
- **Legacy Traits**: Old manual trait specialization system removed
- **MessageService**: Legacy runtime service removed

### Changed
- **Schema Generation**: Now uses reflector-based approach
- **CMake Targets**: `make viewer` now generates schemas and instructions (no Python)
- **Container Registration**: Must satisfy `SerializableContainer` concept

### Migration Guide
```cpp
// Old approach - manual traits
template<typename T, std::size_t N>
struct is_fixed_container<MyContainer<T, N>> : std::true_type {};
// + 8 more trait specializations...

// New approach - satisfy concept
template<typename T, std::size_t N>
class MyContainer {
    using value_type = T;
    static constexpr std::size_t max_size_v = N;
    std::size_t size() const;
    const T* data() const;
    T* data_unsafe();
    void set_size_unsafe(std::size_t);
};
// That's it! Automatically works.
```

## Key Features

### Compile-Time Everything
- Type analysis, size computation, layout mapping all at compile time
- Zero runtime overhead for metadata
- Static assertions catch errors early

### Zero Allocation
- Stack-allocated buffers with compile-time max size
- No heap allocation in serialization paths
- Deterministic execution for real-time systems

### Real-Time Safe
- Bounded execution time
- No exceptions in hot paths
- Lock-free container operations

### Interactive Visualization
- Browser-based schema viewer with:
  - Variable field size controls (sliders)
  - Animation mode for dynamic visualization
  - Hover highlighting across tables/layouts/operations
  - Collapsible sections
  - Multi-field block highlighting
- Terminal-based CLI with colored output

## Statistics

### Codebase
- **21 commits** since v1.0.0
- **+3,215 insertions, -2,066 deletions**
- **3 new containers**: RingBuffer, concept-based system
- **4 development phases** completed

### Performance
- Serialize: ~10-15 ns/op for small structs (unchanged)
- Deserialize: ~50-100 ns/op (unchanged)
- Zero heap allocations maintained
- Compile-time size computation

## Installation

```bash
# Build and install
git clone https://github.com/mattih11/SeRTial.git
cd SeRTial
git checkout v2.0.0
mkdir build && cd build
cmake ..
cmake --build .
sudo cmake --install .
```

Installs:
- Headers: `/usr/local/include/sertial/`
- CLI Tool: `/usr/local/bin/sertial-inspect`
- HTML Viewer: `/usr/local/share/sertial/sertial-inspect/viewer.html`
- Example schemas: `/usr/local/share/sertial/examples/`

## Usage

### Basic Serialization
```cpp
#include <sertial/sertial.hpp>

struct Player {
    uint32_t id;
    float health;
    float x, y, z;
};

int main() {
    Player player{42, 100.0f, 1.5f, 2.5f, 3.5f};
    
    auto buffer = serialize(player);  // Stack-allocated
    auto restored = deserialize<Player>(buffer.view());
    
    static_assert(Message<Player>::base_packed_size == 20);
}
```

### Variable-Size Fields
```cpp
struct SensorData {
    uint64_t timestamp;
    fixed_vector<float, 100> readings;  // 0-100 elements
    fixed_string<64> location;           // Max 64 chars
};

SensorData data = {1234567890, {1.0f, 2.0f}, "Lab-A"};
auto buffer = serialize(data);  // Variable runtime size
```

### Schema Generation & Viewing
```bash
# Generate schemas
cd build && make viewer

# CLI inspection
./sertial-inspect ../examples/schemas/example_schemas.json --summary

# HTML viewer (open in browser)
# tools/sertial-inspect/viewer.html?schema=../../examples/schemas/example_schemas.json
```

### Live Demo
[Interactive Schema Viewer](https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html?schema=https://raw.githubusercontent.com/mattih11/SeRTial/main/examples/schemas/example_schemas.json)

## Bug Fixes

- Fixed endianness handling in serialization (use packed offsets)
- Fixed padding detection in hybrid memory maps
- Fixed RingBuffer wrap-around serialization
- Fixed multi-span container handling in schema export

## Acknowledgments

- [reflect-cpp](https://github.com/getml/reflect-cpp) for compile-time reflection
- C++20 concepts for elegant type constraints
- Community feedback on real-time requirements

## What's Next (Phase 5+)

- Cross-platform serialization (endianness handling)
- Nested container support
- Additional container types
- Performance profiling tools
- ROS 2 adapter

---

**Full Changelog**: https://github.com/mattih11/SeRTial/compare/v1.0.0...v2.0.0
