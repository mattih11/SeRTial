# SeRTial Next Steps

**Current Status**: Phase 4 Complete - StructLayout v5.0.0 + Cleanup ✅  
**Date**: February 6, 2026

## Completed Work

### Phase 3: Schema Migration to StructLayout
- ✅ Created StructLayout<T> as single source of truth
- ✅ Unified block-based execution model
- ✅ Clean schema export via rfl::json::write()
- ✅ All code migrated from HybridMemoryMap/MemoryMap
- ✅ SchemaGenerator v5.0.0

### Phase 4: Code Cleanup
- ✅ Archived legacy files (630 lines)
- ✅ Removed deprecated container detection aliases (40 lines)
- ✅ Modernized to use SerializableContainer concept
- ✅ All tests passing (11/11)
- ✅ Documentation updated

### Phase 5: Endianness Fix
- ✅ Fixed test_endianness (packed offset calculation)
- ✅ Endian swap now uses serialized offsets, not struct offsets
- ✅ All tests passing (12/12)

### Phase 6: Container Registration Consolidation
- ✅ Moved container_type_name<T> to container_registration.hpp
- ✅ Moved has_serializable_containers() helpers
- ✅ Deleted include/sertial/traits/container_detection.hpp (62 lines)
- ✅ True single registration point achieved

## Current Architecture

```
StructLayout<T>
├── Constexpr metadata (std::array)
│   ├── field_sizes, field_offsets, field_is_variable
│   ├── blocks[], execution_order[]
│   └── base_packed_size, max_packed_size
├── Block execution
│   ├── serialize(value, dest_span) → bytes_written
│   └── deserialize_opt(src_span) → std::optional<T>
└── Schema export
    └── export_schema<T>() → TypeSchema → JSON
```

## Immediate Next Steps

### ~~1. Fix test_endianness (Currently Disabled)~~ ✅ COMPLETE

**Status**: FIXED - All tests passing (12/12)

**Issue**: Endian swap was using struct offsets instead of packed offsets

**Root Cause**: 
- Serialization removes padding between fields (e.g., `uint16_t` + `uint32_t` → no 2-byte alignment gap)
- `swap_endianness()` was using `StructLayout<T>::field_offsets` (struct memory layout)
- Needed **packed offsets** (cumulative field sizes in serialized stream)

**Solution Implemented**:
```cpp
// OLD: Used struct offsets (includes padding)
constexpr std::size_t value = StructLayout<T>::field_offsets[Index];

// NEW: Calculate packed offsets (cumulative sizes, no padding)
constexpr std::size_t value = []() constexpr {
    std::size_t offset = 0;
    for (std::size_t i = 0; i < Index; ++i) {
        offset += StructLayout<T>::field_sizes[i];
    }
    return offset;
}();
```

**Changes**:
- `core/endian.hpp`: Replaced `offset_constant` with `packed_offset_constant`
- `CMakeLists.txt`: Re-enabled test_endianness in run_tests
- All 12 tests now passing

**Complexity**: Medium (2 hours actual)  
**Impact**: High (cross-platform endianness support now functional)

### ~~2. Container Registration Consolidation~~ ✅ COMPLETE

**Status**: COMPLETE - True single registration point achieved

**Goal**: Merge remaining helpers from container_detection.hpp into container_registration.hpp

**Changes Implemented**:
- Moved `container_type_name<T>` struct and specializations
- Moved `has_serializable_containers()` and `struct_has_serializable_containers()` functions
- Added `detail::extract_field_type` helper for rfl::Field type extraction
- Deleted `include/sertial/traits/container_detection.hpp` (62 lines removed)

**Updated References**:
- `struct_has_fixed_containers()` → `struct_has_serializable_containers()` (clearer naming)
- `endian.hpp`: Now uses container_registration.hpp directly
- `struct_layout.hpp`: Removed container_detection.hpp include

**Result**:
- ✅ Single file: `containers/container_registration.hpp`
- ✅ All container registration in one place
- ✅ All 12/12 tests passing
- ✅ Zero duplication

**Complexity**: Low (1 hour actual)  
**Impact**: Medium (architecture simplification, easier onboarding)

### 3. RingBuffer Schema Metadata Enhancement

**Current**: RingBuffer serialized but schema doesn't show FIFO behavior

**Goal**: Add overflow_behavior to schema
```json
{
  "type": "ring_buffer",
  "overflow_behavior": "fifo_overwrite",
  "capacity": 100,
  "serialized_as": "current_elements_only"
}
```

**Changes**:
- Add `overflow_behavior` field to BlockInfo/TypeSchema
- Update RingBuffer container_type_name to include metadata
- Update schema viewers to display overflow behavior

**Complexity**: Low  
**Impact**: Better schema documentation for circular buffers

### 4. Performance Optimization: Eliminate Unnecessary Copies

**Current**: Some serialization paths copy data

**Goal**: Full zero-copy serialization chain
- Direct span → span transfers
- No intermediate buffers

**Analysis Needed**:
- Profile current serialization
- Identify copy hotspots
- Measure improvement

**Complexity**: Medium  
**Impact**: Performance improvement (TBD - needs profiling)

## Future Enhancements

### 5. Nested Container Support

**Current Limitation**: 
```cpp
// ❌ NOT SUPPORTED:
fixed_vector<fixed_vector<float, 5>, 10> matrix;
```

**Reason**: Inner container has runtime state (size_) that would be serialized incorrectly

**Possible Solution**: Recursive block analysis
- Detect nested containers at compile time
- Generate nested block execution plan
- Serialize inner container size separately from data

**Complexity**: High  
**Impact**: Enables multi-dimensional data structures

### 6. Compile-Time Reflection for std::array

**Current**: std::array<T, N> treated as fixed block (single memcpy)

**Enhancement**: Recursively analyze array elements
- If T has padding → serialize each element separately
- If T is nested struct → apply StructLayout recursively

**Benefit**: Eliminates padding in array serialization

**Complexity**: Medium  
**Impact**: Smaller serialized sizes for arrays of structs

### 7. Custom Serialization Hooks

**Goal**: Allow user-defined serialization for specific types
```cpp
template<>
struct CustomSerializer<MyType> {
    static std::size_t serialize(const MyType&, std::span<std::byte>);
    static std::optional<MyType> deserialize(std::span<const std::byte>);
## Priority Ranking

| Priority | Task | Complexity | Impact | Effort | Status |
|----------|------|------------|--------|--------|--------|
| ~~**High**~~ | ~~Fix test_endianness~~ | ~~Medium~~ | ~~High (cross-platform)~~ | ~~2-3 hours~~ | ✅ **COMPLETE** |
| ~~**High**~~ | ~~Container registration consolidation~~ | ~~Low~~ | ~~Medium (simplification)~~ | ~~1 hour~~ | ✅ **COMPLETE** |
| **Medium** | RingBuffer schema metadata | Low | Low (documentation) | 1 hour | Ready |
| **Medium** | Performance profiling/optimization | Medium | TBD | 4-6 hours | Ready |
| **Low** | Nested container support | High | Medium (new feature) | 8-12 hours | Future |
| **Low** | Compile-time reflection for std::array | Medium | Low (optimization) | 4-6 hours | Future |
| **Low** | Custom serialization hooks | Medium | Low (advanced feature) | 4-6 hours | Future |
| **Low** | SIMD optimization | High | High (if profiling shows benefit) | 12+ hours | Future |
- NEON for ARM

**Complexity**: High  
**Impact**: Significant performance gain for large messages

## Priority Ranking

| Priority | Task | Complexity | Impact | Effort |
|----------|------|------------|--------|--------|
| **High** | Fix test_endianness | Medium | High (cross-platform) | 2-3 hours |
| **High** | Container registration consolidation | Low | Medium (simplification) | 1 hour |
| **Medium** | RingBuffer schema metadata | Low | Low (documentation) | 1 hour |
| **Medium** | Performance profiling/optimization | Medium | TBD | 4-6 hours |
| **Low** | Nested container support | High | Medium (new feature) | 8-12 hours |
| **Low** | Compile-time reflection for std::array | Medium | Low (optimization) | 4-6 hours |
| **Low** | Custom serialization hooks | Medium | Low (advanced feature) | 4-6 hours |
| **Low** | SIMD optimization | High | High (if profiling shows benefit) | 12+ hours |

## Recommended Path Forward

### ~~Short Term (Next Session)~~ ✅ COMPLETE
1. ~~**Fix test_endianness**~~ ✅ - Critical for cross-platform support
2. ~~**Container registration consolidation**~~ ✅ - Complete the single-registration-point goal

### Medium Term (This Week)
3. **RingBuffer schema metadata** - Low effort, improves documentation
4. **Performance profiling** - Establish baseline, identify bottlenecks

### Long Term (Next Month)
5. Decide on nested containers vs other features based on user needs
6. Consider SIMD if profiling shows significant gains

## Questions to Consider

1. ~~**Is endianness support critical?**~~ ✅ YES - Now fully functional
2. **What performance is needed?** Profile before optimizing
3. **Who are the users?** Nested containers vs simplicity trade-off
4. **What platforms matter?** Cross-platform vs platform-specific optimizations

## Current Project Health

- ✅ **Architecture**: Clean, maintainable, single source of truth
- ✅ **Tests**: 12/12 passing (all tests enabled and working)
- ✅ **Documentation**: Comprehensive, up-to-date
- ✅ **Code Quality**: Modern C++20, zero-allocation, real-time safe
- ✅ **Cross-Platform**: Endianness conversion fully functional
- ⚠️ **Feature Completeness**: Core features done, advanced features pending
- ⚠️ **Performance**: Assumed good, needs profiling

## Conclusion

The core architecture is solid and complete. Next steps are either:
- **Fixing known issues** (test_endianness)
- **Polishing** (consolidation, documentation)
- **New features** (nested containers, SIMD)

Choose based on project priorities and user needs.
