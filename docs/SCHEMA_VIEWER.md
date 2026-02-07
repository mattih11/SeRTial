# SeRTial Schema Viewer

**Interactive visualization for understanding SeRTial message layouts**

## Documentation

**User Guides**: [USER_GUIDE.md](USER_GUIDE.md) | [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md) | [EXAMPLES.md](EXAMPLES.md) | [SCHEMA_VIEWER.md](SCHEMA_VIEWER.md)  
**Developer Guides**: [ADDING_CONTAINERS.md](ADDING_CONTAINERS.md) | [CONTAINER_INTERNALS.md](CONTAINER_INTERNALS.md)  
**Technical References**: [SERIALIZATION_MECHANISM.md](SERIALIZATION_MECHANISM.md) | [REFLECTOR_BASED_SCHEMA.md](REFLECTOR_BASED_SCHEMA.md) | [SIZE_CALCULATIONS.md](SIZE_CALCULATIONS.md) | [TEMPLATE_PATTERNS.md](TEMPLATE_PATTERNS.md)

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Features](#features)
4. [Generating Schemas](#generating-schemas)
5. [Understanding the Visualization](#understanding-the-visualization)
6. [Interactive Controls](#interactive-controls)
7. [Technical Details](#technical-details)

---

## Overview

The **SeRTial Schema Viewer** is an interactive HTML-based tool that visualizes how your message types are laid out in memory and how they're serialized to binary format.

### Why Use It?

- **Debug layout issues**: See actual field offsets and padding
- **Optimize wire format**: Visualize serialized size with different container contents
- **Understand serialization**: Watch how blocks are copied to packed layout
- **Documentation**: Generate visual documentation of your message protocols

### Live Demo

**Try it now**: [https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html](https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html)

No installation needed - works entirely in your browser!

---

## Quick Start

### Option 1: Use Live Demo

1. Go to [https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html](https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html)
2. Drag & drop your schema JSON file
3. Explore your message layouts interactively

### Option 2: Load from URL

```
https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html?schema=https://raw.githubusercontent.com/mattih11/SeRTial/main/examples/schemas/example_schemas.json
```

### Option 3: Local Usage

1. Open `tools/sertial-inspect/viewer.html` in any browser
2. Use file picker to load schema JSON
3. No server or dependencies needed!

---

## Features

### 1. Interactive Visualization

Three synchronized views:

```
┌─────────────────────────────────────┐
│ Struct Layout (C++ Memory)          │
│ ┌─────┬─────┬─────────┬─────────┐  │
│ │ id  │ pad │  data   │  stamp  │  │
│ └─────┴─────┴─────────┴─────────┘  │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│ Serialized Layout (Wire Format)     │
│ ┌─────┬───┬───────────┬─────────┐  │
│ │ id  │len│   data    │  stamp  │  │
│ └─────┴───┴───────────┴─────────┘  │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│ Operations (Block Execution)        │
│ Block 0: memcpy(id) [Fixed]         │
│ Block 1: serialize(data) [Dynamic]  │
│ Block 2: memcpy(stamp) [RuntimeOff] │
└─────────────────────────────────────┘
```

### 2. Variable Field Controls

For containers (fixed_vector, fixed_string, RingBuffer):

```
data: [====    ] 40/100 elements
      ^
      Drag slider to change runtime size
      → Watch serialized layout update in real-time
```

### 3. Animation Mode

Click "Animate" to see packed size change automatically:
- Containers fill from 0% to 100%
- Observe how wire format size grows
- Understand base_packed_size vs max_packed_size

### 4. Hover Highlighting

**Hover over struct field** → Highlights:
- Corresponding serialized region
- Related block operation
- Size calculations

**Hover over serialized region** → Highlights:
- Source struct field(s)
- Block operation that copied it

**Hover over operation** → Highlights:
- All affected fields
- Serialized memory region

### 5. Collapsible Sections

- Fold/unfold each visualization independently
- Save screen space for complex types
- Focus on what matters

### 6. Multi-Type Schemas

Load multiple message types in one JSON:
- Dropdown to switch between types
- Compare layouts side-by-side (open in multiple tabs)

---

## Generating Schemas

### Method 1: Complete Example Program

```cpp
#include <sertial/integration/message_collection.hpp>
#include <sertial/integration/schema_generator.hpp>

// Define your types
struct SensorData {
    uint64_t timestamp;
    uint32_t sensor_id;
    sertial::fixed_vector<float, 100> readings;
};

// Register types
using MyMessages = sertial::MessageCollection<
    SensorData
    // Add more types here
>;

int main() {
    // Generate schema JSON
    sertial::SchemaGenerator<MyMessages>::write_verbose("my_schemas.json");
    return 0;
}
```

**Build and run**:
```bash
cd build
cmake --build . --target my_schema_generator
./my_schema_generator
# Creates my_schemas.json - ready for viewer
```

### Method 2: Individual Type Export

```cpp
#include <sertial/integration/schema_export.hpp>
#include <fstream>

struct MyType {
    uint32_t id;
    sertial::fixed_string<64> name;
};

int main() {
    // Export layout data (for viewer)
    std::string json = sertial::export_layout_data<MyType>();
    
    std::ofstream out("my_type_schema.json");
    out << json;
    
    return 0;
}
```

### Method 3: From Existing Example

SeRTial includes a complete example:

```bash
cd build
./schema_example my_schemas.json
# Open tools/sertial-inspect/viewer.html with my_schemas.json
```

**See**: [examples/schema_example.cpp](../examples/schema_example.cpp) for full code

---

## Understanding the Visualization

### Struct Layout (C++ Memory)

Shows how the compiler lays out your struct in memory:

```cpp
struct Example {
    uint32_t id;         // Offset: 0, Size: 4
    // 4 bytes padding
    uint64_t timestamp;  // Offset: 8, Size: 8
    float value;         // Offset: 16, Size: 4
};
```

**Visualization**:
```
┌──────┬────────┬──────────────┬────────┐
│  id  │  pad   │  timestamp   │ value  │
│  4B  │  4B    │     8B       │  4B    │
└──────┴────────┴──────────────┴────────┘
Offset: 0      4      8           16    20
```

**Colors**:
- **Blue**: Fixed-size fields
- **Gray**: Padding (not serialized)
- **Green**: Variable-size containers
- **Orange**: Fixed-size fields after variable content (RuntimeOffset)

### Serialized Layout (Wire Format)

Shows the packed binary format (no padding):

```
┌──────┬──────────────┬────────┐
│  id  │  timestamp   │ value  │
│  4B  │     8B       │  4B    │
└──────┴──────────────┴────────┘
Size: 16 bytes (not 20 - padding removed)
```

For variable containers:
```
┌──────┬─────┬──────────────┐
│  id  │ len │     data     │
│  4B  │ 4B  │  len * 4B    │
└──────┴─────┴──────────────┘
        ↑
        4-byte length prefix
```

### Operations (Block Execution)

Shows how serialization copies data:

```
Block 0: Fixed [offset=0, size=4]
  → memcpy(id) from struct[0] to packed[0]

Block 1: Dynamic [field=data, capacity=100]
  → Write length prefix: 4 bytes
  → memcpy(data.data(), data.size() * 4) 

Block 2: RuntimeOffset [offset=?, size=8]
  → memcpy(timestamp) from struct[16] to packed[offset]
  → offset computed at runtime (depends on data.size())
```

**Block Types**:
- **Fixed**: Contiguous fixed fields → single memcpy
- **Padding**: Skipped (not serialized)
- **Dynamic**: Variable container → length prefix + data
- **RuntimeOffset**: Fixed field after dynamic content → offset varies

---

## Interactive Controls

### Size Sliders

For each variable-size container:

```
readings: [========        ] 40/100
          ← Drag to change runtime size →
```

**Behavior**:
- Adjust slider → Serialized layout updates instantly
- Watch packed size change in real-time
- Understand: base_packed_size vs actual vs max_packed_size

### Animation Controls

**Start Animation**:
- Click "Animate" button
- All containers fill from 0% → 100%
- Loop continuously

**Pause**:
- Click "Pause" to freeze at current state
- Adjust sliders manually

**Speed**:
- Animation cycles every 2 seconds
- Smooth transitions

### Type Selector

For multi-type schemas:
```
┌────────────────────┐
│ Select Type:       │
│ ▼ SensorData       │
│   Position         │
│   PointCloud       │
└────────────────────┘
```

Switch between types instantly - no reload needed.

### Collapse/Expand

Each section has a toggle:
```
▼ Struct Layout        ← Click to fold
  [visualization]

▶ Serialized Layout    ← Click to unfold
```

Useful for complex types with many fields.

---

## Technical Details

### Schema Format

The viewer expects JSON with this structure (generated automatically):

```json
{
  "name": "SensorData",
  "sizeof_bytes": 424,
  "alignof_bytes": 8,
  "field_names": ["timestamp", "sensor_id", "readings"],
  "field_sizes": [8, 4, 412],
  "field_offsets": [0, 8, 12],
  "is_variable_length": [false, false, true],
  "max_elements": [1, 1, 100],
  "element_sizes": [8, 4, 4],
  "container_types": ["none", "none", "fixed_vector"],
  "base_packed_size": 16,
  "max_packed_size": 420,
  "fixed_block_count": 2,
  "dynamic_block_count": 1,
  "runtime_offset_block_count": 0
}
```

**Key fields**:
- `field_*`: Arrays with one entry per field
- `is_variable_length`: true for containers
- `max_elements`: Container capacity (compile-time)
- `base_packed_size`: Minimum serialized size (empty containers)
- `max_packed_size`: Maximum serialized size (full containers)

### Implementation

- **Zero dependencies**: Pure HTML/CSS/JavaScript
- **Runs offline**: No network requests after loading
- **File API**: Drag & drop uses browser FileReader
- **URL params**: `?schema=<url>` fetches via fetch API
- **Reflector-based**: Schema generated via `rfl::Reflector<StructLayout<T>>`

**See**: [REFLECTOR_BASED_SCHEMA.md](REFLECTOR_BASED_SCHEMA.md) for reflector architecture

### Browser Compatibility

Works in all modern browsers:
- Chrome/Edge (recommended)
- Firefox
- Safari
- Opera

**Requirements**:
- ES6 support (2015+)
- FileReader API
- CSS Grid
- Fetch API (for URL loading)

---

## Troubleshooting

### Schema File Not Loading

**Problem**: "Failed to load schema" error

**Solutions**:
1. Check JSON syntax: Use `jq . my_schemas.json` to validate
2. Verify file encoding: Must be UTF-8
3. Check console: Press F12 → Console tab for errors
4. Try different browser

### CORS Issues (URL Loading)

**Problem**: "CORS policy" error when loading from URL

**Solutions**:
1. Host schema on GitHub (raw.githubusercontent.com allows CORS)
2. Use GitHub Pages deployment
3. Load file locally instead of via URL

### Layout Looks Wrong

**Problem**: Visualization doesn't match expectations

**Solutions**:
1. Regenerate schema: Code might have changed
2. Check padding: Use `sizeof()` to verify struct size
3. Verify alignment: Use `alignof()` for field alignment
4. Compare with compiler output: Use `pahole` or similar tools

### Slider Not Working

**Problem**: Container size slider doesn't change visualization

**Solutions**:
1. Check `is_variable_length` in JSON: Must be true for container fields
2. Verify `max_elements` > 0
3. Try refreshing page
4. Check browser console for errors

---

## Next Steps

- **Generate your schemas**: [examples/schema_example.cpp](../examples/schema_example.cpp)
- **Learn serialization internals**: [SERIALIZATION_MECHANISM.md](SERIALIZATION_MECHANISM.md)
- **Understand reflector architecture**: [REFLECTOR_BASED_SCHEMA.md](REFLECTOR_BASED_SCHEMA.md)
- **CLI tool documentation**: [tools/sertial-inspect/README.md](../tools/sertial-inspect/README.md)

---

**Questions?** Open an issue: [https://github.com/mattih11/SeRTial](https://github.com/mattih11/SeRTial)
