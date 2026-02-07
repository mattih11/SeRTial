# Documentation Update Checklist

## Files That Need Updates

### 1. README.md (PRIORITY - Main documentation)
**Lines to update:**
- Line 24: "HybridMemoryMap" → "StructLayout"
- Line 33: Update architecture description
- Lines 86-87: `HybridMemoryMap<Player>` → `StructLayout<Player>` or `Message<Player>`
- Line 168: test_hybrid_binary description still relevant (tests StructLayout internally)
- Lines 213-214: `HybridMemoryMap<Position>` → `StructLayout<Position>`
- Line 298-300: Update API signature to show StructLayout
- Lines 313-332: Entire section - Replace "HybridMemoryMap<T>" with "StructLayout<T>"
- Lines 341-343: Message<T> delegates to StructLayout

**Recommended changes:**
```markdown
## OLD (Lines 313-332):
### HybridMemoryMap<T> - Compile-Time Layout Analysis

## NEW:
### StructLayout<T> - Single Source of Truth

All compile-time analysis, block decomposition, and serialization execution
happens in StructLayout<T>. It provides:

```cpp
template<typename T>
struct StructLayout {
    // Buffer type alias
    using buffer_type = std::array<std::byte, max_packed_size>;
    
    // Size information
    static constexpr std::size_t base_packed_size;  // Fixed fields only
    static constexpr std::size_t max_packed_size;   // With all containers at max capacity
    static constexpr bool has_variable_fields;      // Has dynamic containers?
    
    // Field analysis
    static constexpr std::size_t num_fields;
    static constexpr auto field_sizes;              // constexpr std::array
    static constexpr auto field_offsets;            // constexpr std::array
    
    // Block decomposition
    static constexpr auto fixed_blocks;             // constexpr std::array
    static constexpr auto dynamic_blocks;           // constexpr std::array
    static constexpr auto runtime_offset_blocks;    // constexpr std::array
    static constexpr auto execution_order;          // constexpr std::array
    
    // Serialization API
    static std::size_t serialize(const T& obj, std::span<std::byte, max_packed_size> dest);
    static std::size_t serialize(const T& obj, std::span<std::byte> dest);  // Returns optional
    
    // Deserialization API
    static bool deserialize(T& out, std::span<const std::byte> src);
    static std::optional<T> deserialize_opt(std::span<const std::byte> src);
    
    // Runtime size (for variable-size types)
    static std::size_t calculate_packed_size(const T& value);
};
```

**Usage:**
```cpp
// Direct StructLayout API (advanced, hot paths)
using Layout = StructLayout<Player>;
Layout::buffer_type buffer;
std::size_t size = Layout::serialize(player, buffer);

// Or use Message<T> wrapper (simple, convenient)
auto result = Message<Player>::serialize(player);
```
```

### 2. docs/SERIALIZATION_MECHANISM.md
**Check for:**
- References to 3-file architecture (memory_map/hybrid_memory_map/unified_binary)
- Old serialization flow diagrams
- serialize_to_unified function references

**Update to:**
- Show StructLayout as single point
- Block-based execution model
- Symmetric serialize/deserialize per block type

### 3. docs/SIZE_CALCULATIONS.md
**Check for:**
- References to MemoryMap or HybridMemoryMap traits
- Old packed_size vs unpacked_size terminology

**Update to:**
- StructLayout::base_packed_size (fixed fields)
- StructLayout::max_packed_size (max capacity)
- Compile-time vs runtime size computation

### 4. docs/TEMPLATE_PATTERNS.md
**Check for:**
- Old trait specialization patterns
- References to memory_map.hpp or hybrid_memory_map.hpp

**Update to:**
- StructLayout template patterns
- Container registration via container_registration.hpp
- SerializableContainer concept

### 5. docs/CONTAINER_HANDLING.md
**Check for:**
- Old container trait specializations
- Multiple file registration pattern

**Update to:**
- Single registration point: containers/container_registration.hpp
- serialization_view_provider<T> pattern
- SerializableContainer concept
- Span-based serialization (1-2 spans for all containers)

### 6. docs/HYBRID_MEMORY_MAP_DESIGN.md
**Action:** Move to `docs/archive/HYBRID_MEMORY_MAP_DESIGN.md`
- Historical document (Phase 1-4 complete)
- Describes old 3-file architecture evolution
- Useful for understanding the journey, but not current architecture

### 7. scripts/README.md
**Lines 81, 110-111:**
- References to HybridMemoryMap visualization
- Schema field names (packed_size, has_padding)

**Update to:**
- StructLayout block visualization
- New field names (base_packed_size, has_variable_fields)
- Note: has_padding removed (internal detail)

### 8. examples/serialization_example.cpp
**Status:** ✅ Already updated (lines 153-162)

### 9. src/main.cpp  
**Status:** ✅ Already updated

### 10. include/sertial/debug/print_utils.hpp
**Status:** ✅ Already updated

## New Documentation to Create

### 1. docs/ARCHITECTURE.md (NEW)
**Purpose:** Replace scattered architecture docs with unified view

**Content:**
- Overview: StructLayout as single source of truth
- Block executor pattern (symmetric operations)
- Message<T> as thin wrapper layer
- Buffer management (buffer_type aliases)
- Zero-allocation guarantees
- Compile-time vs runtime paths

**Source:** Promote from `docs/work/ARCHITECTURE_REFACTOR.md`

### 2. docs/MIGRATION_GUIDE.md (NEW)
**Purpose:** Help users transition from old API

**Content:**
```markdown
# Migration Guide

## API Changes

### Type Traits
| Old | New | Notes |
|-----|-----|-------|
| `HybridMemoryMap<T>` | `StructLayout<T>` | Single source of truth |
| `Message<T>::packed_size` | `Message<T>::base_packed_size` | Fixed fields only |
| `Message<T>::has_padding` | Removed | Internal detail |
| `Message<T>::memcpy_count` | Removed | Internal detail |
| `Message<T>::can_single_memcpy` | Removed | Internal detail |

### Buffer Types
| Old | New | Notes |
|-----|-----|-------|
| `std::array<std::byte, N>` | `StructLayout<T>::buffer_type` | Clean alias |
| `std::array<std::byte, HybridMemoryMap<T>::max_packed_size>` | `Message<T>::buffer_type` | Via StructLayout |

### Serialization Functions
| Old | New | Notes |
|-----|-----|-------|
| `serialize_unified(obj)` | `StructLayout<T>::serialize(obj, buffer)` | Direct API |
| `deserialize_unified<T>(data, size)` | `StructLayout<T>::deserialize_opt(span)` | Returns optional |
| `Message<T>::serialize(obj)` | Still valid | Thin wrapper over StructLayout |

## Code Examples

### Before
```cpp
#include <sertial/sertial.hpp>

Player player{...};
std::array<std::byte, HybridMemoryMap<Player>::max_packed_size> buffer;
std::size_t size = serialize_to_unified(player, buffer.data());

auto restored = deserialize_unified<Player>(buffer.data(), size);
if (!restored) {
    // Error handling
}
```

### After (Direct StructLayout API)
```cpp
#include <sertial/sertial.hpp>

Player player{...};
StructLayout<Player>::buffer_type buffer;  // Clean alias!
std::size_t size = StructLayout<Player>::serialize(player, buffer);

auto restored = StructLayout<Player>::deserialize_opt(std::span{buffer.data(), size});
if (!restored) {
    // Error handling
}
```

### After (Message<T> Wrapper)
```cpp
#include <sertial/sertial.hpp>

Player player{...};
auto result = Message<Player>::serialize(player);  // Convenience wrapper
send_data(result.view());

auto restored = Message<Player>::deserialize(received_data);
if (!restored) {
    // Error handling via DeserializeResult
}
```

## When to Use Each API

- **StructLayout<T>** - Hot paths, embedded systems, maximum control
- **Message<T>** - Simple usage, prototyping, convenience wrappers
- **Both work!** - Choose based on your needs
```

### 3. examples/direct_api_example.cpp (NEW)
**Purpose:** Show direct StructLayout usage vs Message<T> wrapper

**Content:**
```cpp
/// Direct API Example - StructLayout vs Message<T>
///
/// This demonstrates when to use each API:
/// - StructLayout<T>: Advanced usage, hot paths, maximum control
/// - Message<T>: Simple usage, convenience wrappers

#include <sertial/sertial.hpp>
#include <iostream>

struct Sensor {
    uint32_t id;
    float temperature;
    uint64_t timestamp;
};

int main() {
    Sensor sensor{42, 23.5f, 1234567890};
    
    // ============================================================================
    // Method 1: Direct StructLayout API (Advanced)
    // ============================================================================
    std::cout << "Method 1: Direct StructLayout API\\n";
    std::cout << "===================================\\n";
    
    using Layout = StructLayout<Sensor>;
    
    // Compile-time information
    std::cout << "  base_packed_size: " << Layout::base_packed_size << " bytes\\n";
    std::cout << "  max_packed_size:  " << Layout::max_packed_size << " bytes\\n";
    std::cout << "  has_variable_fields: " << (Layout::has_variable_fields ? "yes" : "no") << "\\n\\n";
    
    // Serialize with clean buffer alias
    Layout::buffer_type buffer;
    std::size_t size = Layout::serialize(sensor, buffer);
    std::cout << "  Serialized: " << size << " bytes\\n";
    
    // Deserialize
    auto restored = Layout::deserialize_opt(std::span{buffer.data(), size});
    if (restored) {
        std::cout << "  Restored: id=" << restored->id 
                  << " temp=" << restored->temperature << "\\n\\n";
    }
    
    // ============================================================================
    // Method 2: Message<T> Wrapper (Simple)
    // ============================================================================
    std::cout << "Method 2: Message<T> Wrapper API\\n";
    std::cout << "=================================\\n";
    
    // Even simpler - one-liner serialize
    auto result = Message<Sensor>::serialize(sensor);
    std::cout << "  Serialized: " << result.size << " bytes\\n";
    
    // One-liner deserialize with error info
    auto restored2 = Message<Sensor>::deserialize(result.view());
    if (restored2) {
        std::cout << "  Restored: id=" << restored2->id 
                  << " temp=" << restored2->temperature << "\\n\\n";
    } else {
        std::cerr << "  Error: " << restored2.error().what << "\\n";
    }
    
    // ============================================================================
    // When to Use Each
    // ============================================================================
    std::cout << "When to use each API:\\n";
    std::cout << "====================\\n";
    std::cout << "  StructLayout<T>: Hot paths, embedded systems, maximum control\\n";
    std::cout << "  Message<T>:      Prototyping, simple code, convenience\\n";
    
    return 0;
}
```

## Update Priority

1. **README.md** (HIGH) - Main user-facing documentation
2. **Create MIGRATION_GUIDE.md** (HIGH) - Help users transition
3. **Create direct_api_example.cpp** (MEDIUM) - Show both APIs
4. **Update SERIALIZATION_MECHANISM.md** (MEDIUM) - Core concepts
5. **Archive HYBRID_MEMORY_MAP_DESIGN.md** (LOW) - Historical
6. **Update other docs/** (LOW) - Nice to have

## Validation Checklist

After updates:
- [ ] README examples compile and run
- [ ] All code snippets are tested
- [ ] Links between docs work
- [ ] Python viewers still work with schema changes (if any)
- [ ] CMake targets still work (make viewer, make visualize)
- [ ] Examples build successfully
- [ ] Tests still pass
