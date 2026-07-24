# SeRTial Schema Viewer

**Interactive visualization for understanding SeRTial message layouts**

## Documentation

**User Guides**: [USER_GUIDE.md](USER_GUIDE.md) | [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md) | [EXAMPLES.md](EXAMPLES.md) | [SCHEMA_VIEWER.md](SCHEMA_VIEWER.md)  
**Developer Guides**: [ADDING_CONTAINERS.md](ADDING_CONTAINERS.md) | [CONTAINER_INTERNALS.md](CONTAINER_INTERNALS.md)  
**Technical References**: [SERIALIZATION_MECHANISM.md](SERIALIZATION_MECHANISM.md) | [REFLECTOR_BASED_SCHEMA.md](REFLECTOR_BASED_SCHEMA.md) | [SIZE_CALCULATIONS.md](SIZE_CALCULATIONS.md) | [TEMPLATE_PATTERNS.md](TEMPLATE_PATTERNS.md)

---

## Overview

Interactive HTML-based tool that visualizes how your message types are laid out in memory and serialized to binary format. No installation needed - runs entirely in your browser.

**Try it now**: [https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html?schema=https://raw.githubusercontent.com/mattih11/SeRTial/main/examples/schemas/example_schemas.json](https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html?schema=https://raw.githubusercontent.com/mattih11/SeRTial/main/examples/schemas/example_schemas.json)

---

## Quick Start

1. Open the viewer (link above or `tools/sertial-inspect/viewer.html`)
2. Drag & drop your schema JSON file, or
3. Load via URL: `?schema=<your-schema-url>`

### Generate Schema

**Option A — CMake helper (recommended for projects)**

Add one call to your `CMakeLists.txt` after `find_package(SeRTial)`.
The schema JSON is regenerated automatically every time your executable is rebuilt:

```cmake
# my_registry.hpp defines:  using MyMessages = MessageCollection<Msg1, Msg2>;
sertial_generate_schema(
    TARGET          my_app
    REGISTRY_HEADER "${CMAKE_SOURCE_DIR}/include/my_registry.hpp"
    COLLECTION_TYPE MyMessages
    OUTPUT          "${CMAKE_BINARY_DIR}/my_schemas.json"
)
```

```bash
cmake --build build   # schema auto-generated as post-build step
```

**Option B — write a small main() yourself**

```cpp
#include <sertial/integration/message_collection.hpp>
#include <sertial/integration/schema_generator.hpp>

struct SensorData {
    uint64_t timestamp;
    uint32_t sensor_id;
    sertial::fixed_vector<float, 100> readings;
};

using MyMessages = sertial::MessageCollection<SensorData>;

int main() {
    sertial::SchemaGenerator<MyMessages>::write_verbose("my_schemas.json");
    return 0;
}
```

Build and run:
```bash
cmake --build build --target my_schema_generator
./build/my_schema_generator
# Creates my_schemas.json - load in viewer
```

**See**: [examples/schema_example.cpp](../examples/schema_example.cpp) for complete example

---

Document## Features

### Three Synchronized Views

1. **Struct Layout** - C++ memory layout with padding
2. **Serialized Layout** - Wire format (packed, no padding)
3. **Operations** - Block execution plan (memcpy operations)

### Interactive Controls

- **Size sliders**: Adjust container runtime size, watch serialization update
- **Animation**: Auto-fill containers from 0% → 100%
- **Hover highlighting**: Cross-highlight related regions across all views
- **Type selector**: Switch between multiple types in one schema

---

## What It Shows

### Memory Layout
- Field offsets and sizes
- Padding bytes (gray, not serialized)
- Variable-size containers (green)

### Wire Format
- Packed binary layout
- Length prefixes for containers
- Actual vs maximum size

### Serialization Operations
- **Fixed blocks**: Single memcpy for contiguous fields
- **Dynamic blocks**: Length prefix + data for containers
- **RuntimeOffset blocks**: Fields after variable content

---

## Schema Format

Auto-generated JSON with compile-time metadata:

```json
{
  "name": "SensorData",
  "field_names": ["timestamp", "readings"],
  "field_sizes": [8, 412],
  "is_variable_length": [false, true],
  "max_elements": [1, 100],
  "base_packed_size": 12,
  "max_packed_size": 412
}
```

**Generation**: Uses `rfl::Reflector<StructLayout<T>>` for zero-boilerplate export

**See**: [REFLECTOR_BASED_SCHEMA.md](REFLECTOR_BASED_SCHEMA.md) for architecture details

---

## Browser Compatibility

Works in all modern browsers (Chrome, Firefox, Safari, Edge). Requires:
- ES6 support
- FileReader API for drag & drop
- Fetch API for URL loading

---

## Troubleshooting

**Schema not loading?**
- Validate JSON syntax: `jq . my_schemas.json`
- Check console (F12) for errors

**CORS issues with URL?**
- Use GitHub raw URLs (raw.githubusercontent.com)
- Or load file locally

**Layout looks wrong?**
- Regenerate schema if code changed
- Verify with `sizeof(YourStruct)`

---

**CLI alternative**: [tools/sertial-inspect/README.md](../tools/sertial-inspect/README.md)
