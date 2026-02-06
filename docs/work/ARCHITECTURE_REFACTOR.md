# Architecture Refactoring Analysis

## Current Problems

### 1. **Three Separate Files for Related Concerns**
- `memory_map.hpp`: Field analysis, schema generation
- `hybrid_memory_map.hpp`: Block analysis, size computation
- `unified_binary.hpp`: Serialization/deserialization execution

**Issue**: Changes require modifying 3 files. Schema generation is manually synchronized.

### 2. **std::vector Usage (Runtime Allocation)**
```cpp
// memory_map.hpp
std::vector<std::string> field_names;
std::vector<FieldInfo> field_info;
std::vector<BlockInfo> blocks;

// hybrid_memory_map.hpp
static std::vector<BlockInfo> get_block_info() {
    std::vector<BlockInfo> blocks;  // Runtime allocation!
    ...
}
```

**Issue**: Goes against zero-allocation mandate. Schema generation allocates.

### 3. **Serialization/Deserialization Split**
```cpp
// unified_binary.hpp
serialize_to_unified()   // Line 108
deserialize_from_unified() // Separate location

// Different blocks handled separately
// Fixed block: serialize here, deserialize there
// Dynamic block: serialize here, deserialize there
```

**Issue**: Logic for one block type split across files. Hard to maintain symmetry.

### 4. **Manual Schema Synchronization**
When adding BlockInfo fields (span_based_serialization, max_span_count):
- Update BlockInfo struct in memory_map.hpp
- Update get_block_info() in hybrid_memory_map.hpp
- Update TypeSchema in memory_map.hpp
- Update Python parsers

**Issue**: Error-prone, no compile-time enforcement of sync.

### 5. **Unclear Separation of Concerns**
- memory_map: Has both type analysis AND schema export
- hybrid_memory_map: Has both block analysis AND serialize() method
- unified_binary: Has serialize_to_unified() but uses HybridMemoryMap

**Issue**: Circular dependencies, unclear ownership.

## Proposed Architecture

### Core Principle
**"Serialization descriptor IS the schema"** - no manual sync needed

### New Structure

```
sertial/
├── core/
│   └── layout/
│       ├── struct_layout.hpp    # SINGLE SOURCE OF TRUTH
│       │   └── StructLayout<T>
│       │       - Compile-time field analysis
│       │       - Block decomposition
│       │       - Size computation
│       │       - Schema metadata (as constexpr, not std::vector)
│       │
│       └── block_executor.hpp    # Execution engine
│           ├── serialize_block()   # Per-block type
│           ├── deserialize_block() # Symmetric to serialize
│           └── Block types:
│               - FixedBlock: serialize + deserialize together
│               - DynamicBlock: serialize + deserialize together
│               - RuntimeOffsetBlock: serialize + deserialize together
│
├── integration/
│   └── schema_export.hpp        # Convert constexpr → JSON
│       └── Convert StructLayout compile-time data to runtime JSON
```

### Key Changes

#### 1. StructLayout<T> - Single Source of Truth
```cpp
template<typename T>
struct StructLayout {
    // Compile-time block descriptors (std::array, not std::vector)
    static constexpr auto blocks = analyze_blocks<T>();
    
    // Size information (constexpr)
    static constexpr std::size_t base_packed_size = ...;
    static constexpr std::size_t max_packed_size = ...;
    
    // Schema metadata (constexpr arrays)
    static constexpr auto field_info = build_field_info<T>();
    
    // Execution methods with compile-time size validation
    static std::size_t serialize(const T& obj, std::span<std::byte, max_packed_size> dest) {
        // Compile-time check: span size matches max_packed_size
        // This ensures buffer is always large enough
        // Execute blocks...
    }
    
    static std::optional<T> deserialize(std::span<const std::byte> src) {
        // Runtime check: src.size() >= minimum required
        // Execute blocks...
    }
};
```

#### 2. Block with Serialize/Deserialize Together (Using std::span)
```cpp
struct FixedBlock {
    std::size_t src_offset;
    std::size_t size;
    
    // Compile-time size checking via span
    static std::size_t serialize(std::span<const std::byte> src, 
                                  std::span<std::byte> dest) {
        // Caller must ensure dest.size() >= size (checked at call site)
        std::memcpy(dest.data(), src.data() + src_offset, size);
        return size;
    }
    
    static void deserialize(std::span<const std::byte> src, 
                            std::span<std::byte> dest) {
        // Caller must ensure src.size() >= size (checked at call site)
        std::memcpy(dest.data() + src_offset, src.data(), size);
    }
};

struct DynamicBlock {
    std::size_t field_index;
    std::size_t elem_size;
    std::size_t max_elements;  // Known at compile time!
    
    template<typename T>
    static std::size_t serialize(const T& field, std::span<std::byte> dest) {
        // Compile-time check: dest must fit max possible size
        constexpr std::size_t max_size = 4 + max_elements * elem_size;
        static_assert(dest.size() >= max_size, "Destination buffer too small");
        
        auto spans = get_serialization_spans(field);
        uint32_t len = field.size();
        std::memcpy(dest.data(), &len, 4);
        std::size_t offset = 4;
        for (const auto& span : spans) {
            if (span.empty()) continue;
            std::memcpy(dest.data() + offset, span.data(), span.size_bytes());
            offset += span.size_bytes();
        }
        return offset;
    }
    
    template<typename T>
    static void deserialize(std::span<const std::byte> src, T& field) {
        uint32_t len;
        std::memcpy(&len, src.data(), 4);
        
        // Runtime check: actual length must fit in container
        if (len > max_elements) {
            // Handle error (or static_assert at compile time if possible)
            return;
        }
        
        field.set_size_unsafe(len);
        std::memcpy(field.data_unsafe(), src.data() + 4, len * elem_size);
    }
};
```

#### 3. No std::vector - Use Compile-Time Arrays
```cpp
// OLD (runtime allocation)
std::vector<BlockInfo> blocks;
for (...) { blocks.push_back(...); }

// NEW (compile-time size)
static constexpr std::size_t block_count = count_blocks<T>();
static constexpr std::array<BlockDescriptor, block_count> blocks = make_blocks<T>();
```

#### 4. Schema Export Using reflect-cpp JSON
```cpp
// StructLayout has constexpr data
template<typename T>
struct StructLayout {
    static constexpr auto field_info = ...;  // std::array
    static constexpr auto blocks = ...;       // std::array
};

// Schema export: reflect-cpp automatically generates JSON from constexpr data
template<typename T>
std::string export_schema() {
    // Create runtime-friendly schema struct that reflect-cpp can serialize
    struct SchemaData {
        std::string name;
        std::size_t base_packed_size;
        std::size_t max_packed_size;
        std::vector<FieldMetadata> fields;  // Runtime vector for JSON export only
        std::vector<BlockMetadata> blocks;
    };
    
    SchemaData schema;
    schema.name = rfl::type_name_t<T>().str();
    schema.base_packed_size = StructLayout<T>::base_packed_size;
    schema.max_packed_size = StructLayout<T>::max_packed_size;
    
    // Copy constexpr arrays to runtime vectors (only for schema export)
    for (const auto& field : StructLayout<T>::field_info) {
        schema.fields.push_back(field);
    }
    for (const auto& block : StructLayout<T>::blocks) {
        schema.blocks.push_back(block);
    }
    
    // Let reflect-cpp generate JSON automatically
    return rfl::json::write(schema);
}
```

**Key insight**: We use `std::vector` ONLY in the schema export layer (which is not real-time critical). The actual serialization engine uses `constexpr std::array` everywhere.

## Benefits

1. **Single source of truth**: StructLayout<T> contains everything
2. **Zero allocation in hot paths**: All metadata is constexpr std::array
   - std::vector used ONLY in schema export (not real-time critical)
3. **Symmetry**: serialize/deserialize for each block type together
4. **No manual sync**: Schema derives from StructLayout via reflect-cpp
5. **Clear ownership**: StructLayout owns analysis, block_executor just executes
6. **Easier testing**: Test serialize/deserialize together per block type
7. **Compile-time size validation**: Using std::span<std::byte, N> catches buffer overflow at compile time
   ```cpp
   // Example: Compile-time error if buffer too small
   Player p;
   std::array<std::byte, 10> buffer;  // Too small!
   StructLayout<Player>::serialize(p, buffer);  // Compile error: span size mismatch
   
   std::array<std::byte, StructLayout<Player>::max_packed_size> correct_buffer;
   StructLayout<Player>::serialize(p, correct_buffer);  // OK: sizes match
   ```
8. **Type-safe spans**: Raw pointers replaced with std::span - no pointer arithmetic bugs

## Migration Path

1. Create new struct_layout.hpp with StructLayout<T>
2. Port block analysis from hybrid_memory_map.hpp
3. Port serialize/deserialize to block_executor.hpp
4. Update schema_generator.hpp to use StructLayout + rfl::json::write()
5. Remove old memory_map/hybrid_memory_map/unified_binary
6. Update tests

## Concrete Example: Before vs After

### Before (Current - Using raw pointers)
```cpp
// User code
Player player{42, 100.0f, 1.0f, 2.0f, 3.0f};
std::vector<std::byte> buffer(HybridMemoryMap<Player>::max_packed_size);  // Runtime allocation!
std::size_t size = serialize_to_unified(player, buffer.data());  // Raw pointer - no size check

// Schema generation (separate, manual)
auto schema = get_hybrid_schema<Player>();  // Returns TypeSchema with std::vector fields
std::string json = rfl::json::write(schema);  // Must manually keep TypeSchema in sync
```

### After (Proposed - Using std::span)
```cpp
// User code - compile-time size validation
Player player{42, 100.0f, 1.0f, 2.0f, 3.0f};
std::array<std::byte, StructLayout<Player>::max_packed_size> buffer;  // Stack allocated!
std::size_t size = StructLayout<Player>::serialize(player, buffer);  // Compile-time size check

// Schema generation (automatic from StructLayout)
std::string json = export_schema<Player>();  // Derives from StructLayout constexpr data
// No manual sync needed - schema is the serialization descriptor
```

### Key Improvements
1. **No `std::vector` in serialization**: Stack-allocated `std::array`
2. **Compile-time buffer size check**: `std::span<std::byte, N>` enforces size
3. **Single source of truth**: Schema derives from same StructLayout used for serialization
4. **No raw pointers**: Type-safe `std::span` throughout

## Open Questions

1. ~~How to handle constexpr std::string_view vs std::string for names?~~ 
   → Use `std::string_view` in constexpr, convert to `std::string` only in schema export
   
2. ~~Keep reflect-cpp integration or replace with simpler reflection?~~
   → **KEEP reflect-cpp** - it's perfect for JSON generation from constexpr metadata
   
3. ~~Should schema export use reflect-cpp's JSON or raw nlohmann::json?~~
   → **Use rfl::json::write()** - automatic, type-safe, no manual JSON construction

4. **NEW**: How to handle `std::span<std::byte, N>` with dynamic extent for deserialization?
   → Deserialize can't know size at compile time, use `std::span<const std::byte>` (dynamic extent)

5. **NEW**: Should block descriptors store field references or field indices?
   → Field indices (like current approach) - simpler for constexpr arrays

