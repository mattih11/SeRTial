# Schema Generation Migration Plan

## Goal
Migrate schema generation from HybridMemoryMap/MemoryMap to StructLayout-based approach, eliminating compatibility layers and achieving a clean, direct architecture.

## Current Problems

1. **Compatibility Layers**: unified_binary.hpp and unified_api.hpp exist only for backward compatibility
2. **Duplicate Paths**: Schema generation uses HybridMemoryMap while serialization uses StructLayout
3. **Manual Synchronization**: TypeSchema must be manually kept in sync with StructLayout
4. **Old Files**: hybrid_memory_map.hpp and memory_map.hpp serve only schema generation

## Proposed Architecture

```
User Code
    ↓
StructLayout<T>::serialize() / deserialize_opt()  (Direct API)
    ↓
block_executor.hpp (Execution logic)
    ↓
block_types.hpp (Type definitions)

Schema Generation (Separate concern)
    ↓
integration/schema_export.hpp
    ↓
Read StructLayout<T> constexpr metadata
    ↓
Convert to runtime SchemaOutput for JSON
    ↓
rfl::json::write(schema)
```

## Implementation Steps

### Step 1: Create schema_export.hpp
Read StructLayout constexpr data and convert to JSON-friendly format.

```cpp
// integration/schema_export.hpp
template<typename T>
SchemaOutput export_schema() {
    SchemaOutput output;
    output.name = rfl::type_name_t<T>().str();
    output.base_packed_size = StructLayout<T>::base_packed_size;
    output.max_packed_size = StructLayout<T>::max_packed_size;
    output.has_variable_fields = StructLayout<T>::has_variable_fields;
    
    // Copy constexpr arrays to runtime vectors (only for JSON)
    for (const auto& [offset, size] : StructLayout<T>::field_info) {
        output.fields.push_back({...});
    }
    
    return output;
}
```

### Step 2: Update SchemaGenerator
Replace get_hybrid_schema<T>() with export_schema<T>().

### Step 3: Remove Compatibility Layers
- Delete unified_binary.hpp (use StructLayout directly)
- Delete unified_api.hpp (no longer needed)
- Update sertial.hpp to expose StructLayout directly

### Step 4: Archive Legacy Files
- Move hybrid_memory_map.hpp to docs/archive/
- Move memory_map.hpp to docs/archive/
- Update all includes

### Step 5: Update Tests
- Replace serialize()/deserialize() with StructLayout<T>::serialize()/deserialize_opt()
- Update examples to show clean direct API

## Benefits

1. ✅ **Clean Direct API**: Users call StructLayout<T>::serialize() directly
2. ✅ **Zero Compatibility Layers**: No unified_* wrappers
3. ✅ **Single Source**: StructLayout is the ONLY analysis engine
4. ✅ **Automatic Schema Sync**: Schema reads from StructLayout constexpr data
5. ✅ **Simpler Codebase**: Fewer files, clearer architecture

## Files to Create
- `include/sertial/integration/schema_export.hpp` - NEW

## Files to Modify
- `include/sertial/integration/schema_generator.hpp` - Use export_schema<T>()
- `include/sertial/sertial.hpp` - Expose StructLayout directly
- All test files - Use direct API
- All example files - Use direct API

## Files to Delete
- `include/sertial/io/unified_binary.hpp` - Remove compatibility layer
- `include/sertial/core/layout/unified_api.hpp` - Remove compatibility layer

## Files to Archive
- `include/sertial/core/traits/hybrid_memory_map.hpp` → `docs/archive/`
- `include/sertial/core/traits/memory_map.hpp` → `docs/archive/`

## Migration Strategy

Since you're the only user, we can do this in one clean sweep:
1. Create schema_export.hpp
2. Update SchemaGenerator to use it
3. Delete all compatibility layers
4. Update all includes in one commit
5. Test everything
6. Archive old files

No gradual migration needed - just a clean break to the new architecture!
