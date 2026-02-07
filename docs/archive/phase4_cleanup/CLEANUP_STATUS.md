# Architecture Status & Cleanup Analysis

## Current State vs Architecture Goals

### ✅ Completed

1. **StructLayout<T> as Single Source of Truth**
   - ✅ Created `include/sertial/core/layout/struct_layout.hpp`
   - ✅ Compile-time field analysis
   - ✅ Block decomposition (Fixed, Dynamic, RuntimeOffset, Padding)
   - ✅ Size computation (base_packed_size, max_packed_size)
   - ✅ Buffer type alias (`buffer_type`)
   - ✅ Schema metadata (as constexpr std::array, not std::vector)

2. **block_executor.hpp Created**
   - ✅ Created `include/sertial/core/layout/block_executor.hpp`
   - ✅ Symmetric serialize/deserialize per block type
   - ✅ FixedBlock, DynamicBlock, RuntimeOffsetBlock, PaddingBlock
   - ✅ No raw pointers - only std::span
   - ✅ No temporary buffers - direct memory operations

3. **Message<T> Simplified**
   - ✅ Removed HybridMemoryMap/MemoryMap dependencies
   - ✅ Direct delegation to StructLayout (ONE layer, not 3!)
   - ✅ Kept Result/DeserializeResult wrappers
   - ✅ Removed internal details (memcpy_count, can_single_memcpy, has_padding)
   - ✅ Clean API: base_packed_size, max_packed_size, has_variable_fields

4. **Exception-Free API**
   - ✅ All deserialize functions return std::optional
   - ✅ No throw/catch in hot paths
   - ✅ Updated all tests and examples

5. **Code Consolidation Complete** (NEW)
   - ✅ Created `block_types.hpp` - single source for block definitions
   - ✅ Eliminated duplication between hybrid_memory_map.hpp and block_executor.hpp
   - ✅ ~150 lines of duplicate code removed
   - ✅ All tests pass (100% success rate)
   - 📄 See: `docs/work/CODE_CONSOLIDATION_COMPLETE.md`
   - ✅ No throws in serialization paths
   - ✅ DeserializeResult works with optional returns

## ⚠️ Still Using Old Architecture

### Schema Generation System
**Status**: Uses hybrid_memory_map.hpp for JSON export (acceptable - not in hot path)

**Current flow:**
```cpp
SchemaGenerator<MyMessages>::generate()
  └─> get_hybrid_schema<T>()  // hybrid_memory_map.hpp
      └─> MemoryMap<T>::get_schema()  // memory_map.hpp
      └─> HybridMemoryMap<T>::get_block_info()  // Returns std::vector for JSON
```

**Note**: This is ONLY for schema export to JSON (Python visualization tools).
Runtime serialization uses StructLayout<T> directly. No hot-path impact.

**Files involved:**
- `include/sertial/integration/schema_generator.hpp` - Calls get_hybrid_schema<T>()
- `include/sertial/core/traits/hybrid_memory_map.hpp` - Schema generation functions
- `include/sertial/core/traits/memory_map.hpp` - TypeSchema definitions

**Why it's OK**: 
- ✅ Not in serialization hot path
- ✅ std::vector allocation acceptable for one-time JSON export
- ✅ Works correctly with Python viewers
- ✅ Users rarely interact with schema generation directly

**Future improvement**: Could read from StructLayout constexpr metadata instead

### User-Facing API Files (NOT Legacy!)

#### `io/unified_binary.hpp` ✅ 
**Purpose**: Primary user-facing API  
**Function**: Provides `serialize()` and `deserialize()` convenience functions  
**Status**: **This is the RECOMMENDED API** - not legacy!

#### `core/layout/unified_api.hpp` ✅
**Purpose**: Implementation layer for user API  
**Function**: Delegates to StructLayout<T> internally  
**Status**: Active implementation file - not legacy!

**Architecture**:
```
User Code → io/unified_binary.hpp → core/layout/unified_api.hpp → StructLayout<T>
```

**Clarification**: The "unified" naming makes these files sound legacy, but they're
actually the primary user interface. Consider future rename:
- `unified_binary.hpp` → `serialize.hpp`
- `unified_api.hpp` → `serialize_impl.hpp`
```cpp
SchemaGenerator<MyMessages>::generate()
  └─> get_hybrid_schema<T>()  // hybrid_memory_map.hpp:738
      └─> MemoryMap<T>::get_schema()  // memory_map.hpp:543
      └─> HybridMemoryMap<T>::get_block_info()  // Returns std::vector!
```

**Files involved:**
- `include/sertial/integration/schema_generator.hpp` - Uses get_hybrid_schema<T>()
- `include/sertial/core/traits/hybrid_memory_map.hpp` - get_block_info() returns std::vector
- `include/sertial/core/traits/memory_map.hpp` - TypeSchema struct, get_schema()

**Problem**: Schema generation uses runtime std::vector allocation, not constexpr arrays from StructLayout

### TypeSchema Structure
**Location**: `include/sertial/core/traits/memory_map.hpp:55`

**Purpose**: JSON export format for Python schema viewers  
**Status**: Keep - used by visualization tools

**Structure includes runtime std::vector** (acceptable for one-time JSON generation):
```cpp
struct TypeSchema {
    std::string name, category;
    std::size_t sizeof_bytes, packed_size, base_packed_size;
    bool has_variable_fields;
    
    // Runtime vectors for JSON export (not used in serialization hot path)
    std::vector<FieldInfo> field_info;
    std::vector<BlockInfo> blocks;
    std::vector<MemcpyRegion> memcpy_regions;
};
```

**Why std::vector is OK here**:
- ✅ Only used during schema generation (one-time operation)
- ✅ Not in serialization hot paths
- ✅ Necessary for JSON export (variable number of fields/blocks)
- ✅ Python viewers expect this format

### Documentation Files

**✅ Updated:**
- `README.md` - Fully updated to reference StructLayout/Message, removed has_padding
- `scripts/README.md` - Updated block visualization description
- All code examples use new API (Message<T>::base_packed_size, etc.)

**Outdated docs (reference old 3-file architecture):**
- `docs/HYBRID_MEMORY_MAP_DESIGN.md` - References old 3-phase plan
- `docs/SERIALIZATION_MECHANISM.md` - May reference old flow
- `docs/SIZE_CALCULATIONS.md` - May reference old traits
- `docs/TEMPLATE_PATTERNS.md` - May reference old patterns
- `docs/CONTAINER_HANDLING.md` - May reference old container integration

**Current work docs:**
- `docs/work/ARCHITECTURE_REFACTOR.md` - Our blueprint (up-to-date)
- `docs/work/MESSAGE_HPP_ANALYSIS.md` - Message.hpp analysis (up-to-date)
- `docs/work/FIXES_APPLIED.md` - Historical fixes

## 🔧 Required Cleanup Tasks

### 1. Update Schema Generation to Use StructLayout

**Goal**: Schema generation should read from StructLayout constexpr metadata

**Steps:**
1. Create `include/sertial/integration/schema_export.hpp`
   - Read StructLayout<T> constexpr data
   - Convert to runtime SchemaOutput for JSON
   - Use rfl::json::write() for export

2. Update `TypeSchema` struct:
   - Remove internal details (has_padding, can_single_memcpy, memcpy_region_count)
   - Rename packed_size → base_packed_size
   - Keep std::vector fields (only for JSON export, not hot path)

3. Update `SchemaGenerator<T>`:
   - Call new schema export functions
   - Remove dependency on get_hybrid_schema()

4. Update Python viewers:
   - Adapt to new field names if needed
   - Test with new schema format

### 2. Mark Old Code as Legacy/Deprecated

**Files to deprecate:**
- `include/sertial/core/layout/unified_api.hpp` - Add deprecation warnings
- `include/sertial/io/unified_binary.hpp` - Document as compatibility layer
- Consider moving to `include/sertial/legacy/` directory

**HybridMemoryMap status:**
- Keep for now (used by schema generation)
- After schema migration, evaluate if needed for anything else
- Likely can be archived to `docs/archive/`

**MemoryMap status:**
- Keep for now (used by TypeSchema)
- After schema migration, evaluate if needed
- Likely can be archived to `docs/archive/`

### 3. Update Documentation

#### Main README.md
- ✅ Check if it references old API names
- ✅ Update schema generation examples if needed
- ✅ Verify buffer usage examples

#### Architecture Docs
**Move to archive:**
- `docs/HYBRID_MEMORY_MAP_DESIGN.md` → `docs/archive/` (historical, Phase 1-4 complete)

**Update:**
- `docs/SERIALIZATION_MECHANISM.md` - Update to show StructLayout flow
- `docs/SIZE_CALCULATIONS.md` - Update to reference StructLayout
- `docs/TEMPLATE_PATTERNS.md` - Update examples
- `docs/CONTAINER_HANDLING.md` - Update to show container_registration.hpp

**Promote from work/ to docs/:**
- Consider moving `docs/work/ARCHITECTURE_REFACTOR.md` to `docs/ARCHITECTURE.md` (finalized design)

### 4. Add Direct StructLayout API Examples

**Create**: `examples/direct_api_example.cpp`

**Show:**
```cpp
// Advanced: Direct StructLayout usage
using Layout = StructLayout<Player>;
Layout::buffer_type buffer;
std::size_t size = Layout::serialize(player, buffer);
auto result = Layout::deserialize_opt(std::span{buffer.data(), size});

// Simple: Message<T> wrapper
auto msg_result = Message<Player>::serialize(player);
auto restored = Message<Player>::deserialize(msg_result.view());
```

**Document when to use each:**
- StructLayout: Hot paths, advanced control, embedded systems
- Message<T>: Simple usage, convenience wrappers, prototyping

### 5. Create Migration Guide

**Create**: `docs/MIGRATION_GUIDE.md`

**Cover:**
- Old API → New API field name mappings
- `packed_size` → `base_packed_size`
- `has_padding` → removed (internal detail)
- Schema generation changes (if any API changes)
- unified_api.hpp → StructLayout direct usage

## 📋 Priority Order

### Phase 1: Schema System (PRIORITY)
1. **Create schema_export.hpp** - Read from StructLayout constexpr
2. **Update SchemaGenerator** - Use new export functions
3. **Test schema generation** - Ensure JSON output works
4. **Test Python viewers** - Verify compatibility

### Phase 2: Documentation
5. **Update main docs** - SERIALIZATION_MECHANISM, SIZE_CALCULATIONS, etc.
6. **Archive old docs** - Move HYBRID_MEMORY_MAP_DESIGN to archive/
7. **Create MIGRATION_GUIDE** - Help users transition
8. **Add direct API examples** - Show both StructLayout and Message<T>

### Phase 3: Cleanup
9. **Mark legacy code** - Add deprecation warnings
10. **Evaluate HybridMemoryMap** - Can it be archived after schema migration?
11. **Evaluate MemoryMap** - Can it be archived?
12. **Clean up includes** - Remove unused headers

## 🎯 Current Blockers

**None!** All core architecture is in place. Schema generation is the main remaining integration point.

## 📊 Architecture Health Check

| Component | Status | Notes |
|-----------|--------|-------|
| StructLayout<T> | ✅ Complete | Single source of truth |
| block_executor.hpp | ✅ Complete | Symmetric operations |
| block_types.hpp | ✅ Complete | Single source, zero duplication |
| Message<T> | ✅ Simplified | One delegation layer |
| Exception-free API | ✅ Complete | All optional-based |
| Buffer aliases | ✅ Complete | Clean buffer_type |
| README.md | ✅ Updated | 20+ references fixed |
| Schema generation | ⚠️ Legacy | Still uses HybridMemoryMap (acceptable - JSON export only) |
| Other docs | ⚠️ Outdated | SERIALIZATION_MECHANISM, SIZE_CALCULATIONS need updates |
| Examples | ⚠️ Limited | Need direct API examples |
| Tests | ✅ Passing | 100% success rate |

## 🔍 Files to Review

### Check if they reference old API:
- `README.md` - Main documentation
- `examples/serialization_example.cpp` - Updated ✅
- `src/main.cpp` - Updated ✅
- `include/sertial/debug/print_utils.hpp` - Updated ✅

### Schema generation files:
- `include/sertial/integration/schema_generator.hpp` - Needs update
- `include/sertial/core/traits/hybrid_memory_map.hpp` - Legacy, but keep for now
- `include/sertial/core/traits/memory_map.hpp` - Legacy, but keep for now

### Python viewers:
- `scripts/sertial-inspect`
- `scripts/sertial-gui`
- `scripts/sertial_common.py`

## ✅ Summary

**Great progress!** Core architecture is complete and tested. Main remaining work:

1. **Schema generation migration** - Primary task
2. **Documentation updates** - Clear old references
3. **Direct API examples** - Show advanced usage
4. **Legacy code cleanup** - After validation period

The codebase is in excellent shape - all tests passing, clean API, zero allocations. Schema generation is the last piece using the old 3-file system.
