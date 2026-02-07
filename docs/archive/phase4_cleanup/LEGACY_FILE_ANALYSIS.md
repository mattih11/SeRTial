# Legacy File Analysis & Cleanup Plan

## Current Situation

After code consolidation and README updates, we still have legacy files that serve different purposes. This document analyzes what can be removed vs what should be kept.

## File Status Analysis

### ✅ Keep - Active Use (Compatibility Layers)

#### 1. `include/sertial/io/unified_binary.hpp`
**Status**: Keep  
**Reason**: Backward-compatible API wrapper  
**Used by**: 
- `sertial.hpp` (main header)
- `test_serialization.cpp`
- `test_hybrid_binary.cpp`
- `runtime_test.hpp`

**Function**: Provides `serialize()` and `deserialize()` convenience functions that delegate to StructLayout. This is the PRIMARY user-facing API.

**Note**: This is NOT legacy - it's the simple API users should use!

#### 2. `include/sertial/core/layout/unified_api.hpp`
**Status**: Keep  
**Reason**: Compatibility layer for serialize/deserialize functions  
**Used by**: `unified_binary.hpp`

**Function**: Provides `serialize_unified()` and `deserialize_unified()` that delegate to StructLayout. Handles primitives separately.

**Note**: This is the implementation layer for the simple API.

### ⚠️ Keep - Schema Generation (Legacy but Functional)

#### 3. `include/sertial/core/traits/hybrid_memory_map.hpp`
**Status**: Keep for now (schema generation dependency)  
**Used by**: `schema_generator.hpp` (calls `get_hybrid_schema<T>()`)

**Function**: 
- Provides compile-time block analysis
- Exports `get_hybrid_schema<T>()` for JSON schema generation
- Called by SchemaGenerator to create TypeSchema objects

**Future**: Could be replaced by reading from StructLayout constexpr metadata

#### 4. `include/sertial/core/traits/memory_map.hpp`
**Status**: Keep for now (schema generation dependency)  
**Used by**: `hybrid_memory_map.hpp`, `schema_generator.hpp`

**Function**:
- Defines TypeSchema, FieldInfo, BlockInfo, MemcpyRegion structs
- Provides `MemoryMap<T>::get_schema()` for field analysis
- Used by Python schema viewers

**Future**: Could consolidate schema types into integration/schema_types.hpp

### ❌ Can Remove - Truly Unused

After analysis: **NONE** - all files are actively used!

## Recommendations

### Short Term (Now)

1. **Add Deprecation Comments** to clarify file purposes:

```cpp
// unified_binary.hpp - USER-FACING API (not deprecated!)
/// @file unified_binary.hpp
/// @brief High-level serialization API
/// 
/// This is the RECOMMENDED API for users. Provides simple serialize()/deserialize()
/// functions that delegate to StructLayout<T> internally.
```

```cpp
// hybrid_memory_map.hpp - SCHEMA GENERATION ONLY
/// @file hybrid_memory_map.hpp
/// @brief Legacy schema generation support
/// 
/// Used by SchemaGenerator for JSON export. Not used in serialization hot paths.
/// All runtime serialization uses StructLayout<T> directly.
```

2. **Update Comments** in files to clarify architecture

3. **Document Architecture** - Show file relationships clearly

### Medium Term (Future)

1. **Consolidate Schema Types**:
   - Move FieldInfo, BlockInfo, etc. from memory_map.hpp to integration/schema_types.hpp
   - Make memory_map.hpp thin wrapper

2. **Migrate Schema Generation**:
   - Create `integration/schema_export.hpp`
   - Read from StructLayout constexpr metadata
   - Generate TypeSchema at runtime for JSON export
   - Remove dependency on hybrid_memory_map.hpp

3. **Archive Old Files** (after schema migration):
   - Move hybrid_memory_map.hpp to archive/
   - Move memory_map.hpp to archive/
   - Update includes in schema_generator.hpp

## Current Architecture Map

```
User Code
    ↓
sertial.hpp
    ↓
io/unified_binary.hpp ────────────┐ (User-facing API)
    ↓                             │
core/layout/unified_api.hpp       │ (Compatibility layer)
    ↓                             │
core/layout/struct_layout.hpp ←───┘ (Single source of truth)
    ↓
core/layout/block_executor.hpp    (Execution logic)
    ↓
core/layout/block_types.hpp       (Type definitions)


Schema Generation (Parallel Path)
    ↓
integration/schema_generator.hpp
    ↓
core/traits/hybrid_memory_map.hpp  (get_hybrid_schema)
    ↓
core/traits/memory_map.hpp         (TypeSchema, FieldInfo)
```

## Summary

**No files should be removed right now** because:

1. **unified_binary.hpp & unified_api.hpp** - These ARE the user-facing API, not legacy code
2. **hybrid_memory_map.hpp & memory_map.hpp** - Still needed for schema generation
3. **StructLayout is used internally** by the compatibility layers

The "3-file architecture" we removed was:
- ❌ Direct user exposure to HybridMemoryMap (now hidden behind Message<T>)
- ❌ Multiple sources of truth for block types (now consolidated)
- ❌ Duplication between files (now eliminated)

What we kept:
- ✅ Clean user API (serialize/deserialize functions)
- ✅ Internal implementation flexibility (StructLayout)
- ✅ Backward compatibility (existing code works)
- ✅ Schema generation (for visualization tools)

## Action Items

1. ✅ Add clarifying comments to key files
2. ✅ Update CLEANUP_STATUS.md to reflect file purposes
3. ⏳ Future: Migrate schema generation to StructLayout-based approach
4. ⏳ Future: Archive hybrid_memory_map.hpp and memory_map.hpp after migration

## Conclusion

The architecture is actually **cleaner than it appears**:
- Users see simple `serialize(obj)` / `deserialize<T>(data)` API
- StructLayout handles all serialization internally
- Schema generation uses legacy traits (acceptable - not in hot path)
- No actual "old files" to remove - everything serves a purpose!

The confusion came from naming: "unified" files sound legacy, but they're actually the primary user API. Consider renaming in future:
- `unified_binary.hpp` → `serialize.hpp`
- `unified_api.hpp` → `serialize_impl.hpp`
