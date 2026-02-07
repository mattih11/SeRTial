# SeRTial Documentation & Development Roadmap

## ✅ Completed

### Documentation Created
1. **docs/SERIALIZATION_MECHANISM.md** - Complete serialization flow documentation
   - Block types (Fixed, Dynamic, RuntimeOffset, Padding)
   - Compile-time vs runtime analysis
   - Size calculations (max_packed_size, calculate_packed_size)
   - std::span zero-copy patterns
   - Wire format specifications
   - RingBuffer integration preview

2. **docs/CONTAINER_HANDLING.md** - Container type system documentation
   - Container categories (Fixed-capacity, Dynamic heap, Static arrays)
   - Current trait hierarchy (3-file problem documented)
   - Concept-based registration proposal
   - Serialization format for each container type
   - Size calculation formulas
   - RingBuffer wrap-around design
   - Best practices and schema compatibility

3. **docs/work/HYBRID_VS_MEMORY_MAP.md** - Architecture analysis
   - MemoryMap vs HybridMemoryMap comparison
   - Redundancy analysis
   - Three refactoring options
   - Recommendation: Option 2 (conservative cleanup)
   - Migration path for future unification

4. **.github/copilot-instructions.md** - Updated with
   - std::span zero-copy patterns and benefits
   - Schema viewer compatibility guidelines
   - rfl::json::write usage requirement
   - Updated "Questions to Ask Yourself" checklist

## 📋 Todo List (15 Items)

### Phase 1: Documentation & Infrastructure (Items 1-3)
**Goal**: Complete documentation and prepare for refactoring

1. **Update Copilot instructions with std::span emphasis** - NOT STARTED
   - Add dedicated section on zero-copy views
   - Document when/why to use span vs containers

2. **Add schema viewer compatibility to Copilot instructions** - NOT STARTED  
   - ✅ DONE (already added in copilot-instructions.md update)

3. **Document size calculation mechanisms** - NOT STARTED
   - Create docs/SIZE_CALCULATIONS.md
   - Explain max_packed_size, base_packed_size, calculate_packed_size
   - Include formulas and examples

### Phase 2: Trait System Simplification (Items 4-5)
**Goal**: Consolidate container registration using C++20 concepts

4. **Implement concept-based container registration** - NOT STARTED
   - Create containers/container_registration.hpp
   - Define SerializableContainer concept
   - Single registration point for all containers

5. **Refactor trait system to use unified registration** - NOT STARTED
   - Update container_traits.hpp to use concept
   - Update memory_map.hpp to use concept
   - Remove/refactor container_detection.hpp

### Phase 3: Validation (Item 6)
**Goal**: Ensure current system stable before RingBuffer

6. **Test existing functionality without RingBuffer** - NOT STARTED
   - Run all tests (foundation, serialization, hybrid_binary)
   - Document any failures
   - Establish baseline before new features

### Phase 4: RingBuffer Integration (Items 7-11)
**Goal**: Add RingBuffer with wrap-around handling

7. **Design RingBuffer serialization strategy** - NOT STARTED
   - Document in docs/work/RINGBUFFER_DESIGN.md
   - Runtime wrap detection approach
   - 1 vs 2 memcpy decision

8. **Implement RingBuffer wrap-around serialization** - NOT STARTED
   - Add to serialize_to_unified
   - Handle contiguous (1 memcpy) vs wrapped (2 memcpy)
   - Always produce linearized output

9. **Implement RingBuffer deserialization** - NOT STARTED
   - Modify HybridMemoryMap::deserialize
   - Use data_unsafe() + set_size_unsafe()
   - Always deserialize to non-wrapped state

10. **Add RingBuffer to schema generation** - NOT STARTED
    - Update schema_generator.hpp
    - Export RingBuffer-specific metadata
    - Include wrap_around_possible flag

11. **Update Python viewers for RingBuffer** - NOT STARTED
    - Update visualize_schema.py (CLI)
    - Update visualize_schema_gui.py (GUI)
    - Create shared Python module for container rendering

### Phase 5: Finalization (Items 12-15)
**Goal**: Complete documentation and testing

12. **Document MemoryMap vs HybridMemoryMap decision** - NOT STARTED
    - Finalize approach from docs/work/HYBRID_VS_MEMORY_MAP.md
    - Update code comments
    - Document in README.md

13. **Create comprehensive test for RingBuffer serialization** - NOT STARTED
    - Write test_ring_buffer_serialization.cpp
    - Cover all edge cases (empty, partial, full, wrapped)
    - Verify round-trip and size calculations

14. **Update README with RingBuffer documentation** - NOT STARTED
    - Add RingBuffer section to bounded containers
    - Document API and serialization behavior
    - Include CommRaT use case example

15. **Review and finalize documentation structure** - NOT STARTED
    - Cross-check all docs for consistency
    - Fix outdated information
    - Add cross-references

## Key Insights Documented

### Serialization Flow
- **Compile-time**: HybridMemoryMap analyzes struct, generates block plan
- **Runtime**: Execute blocks in order (Fixed → Dynamic → RuntimeOffset)
- **Container info needed**: element_size, capacity (compile-time), size(), data() (runtime)

### Container Requirements (Minimal)
```cpp
// Compile-time traits:
- is_fixed_container_v<T> == true
- fixed_container_element_size_v<T> → sizeof(element_type)
- fixed_container_capacity_v<T> → max_size

// Runtime interface:
- .size() const → std::size_t
- .data() const → const value_type*
- typename value_type
```

### Size Calculations
```cpp
// Compile-time:
max_packed_size = base_packed_size + sum(4 + capacity*elem_size)
base_packed_size = sum(FixedBlock.size + RuntimeOffsetBlock.size)

// Runtime:
actual_size = base_packed_size + sum(4 + container.size()*elem_size)
```

### std::span Usage
- **Output**: `buffer.view()` returns `std::span<const std::byte>` for zero-copy
- **Input**: `deserialize(std::span<const std::byte>)` accepts any buffer
- **Benefits**: Type-safe, bounds-checked, container-agnostic, zero-copy

### RingBuffer Strategy
```cpp
// Serialization:
if (head < tail) {  // Not wrapped
    1 memcpy: [head, tail)
} else {            // Wrapped
    2 memcpy: [head, capacity) + [0, tail)
}
// Always output in logical order (oldest→newest)

// Deserialization:
// Always create non-wrapped: head=0, tail=size
// Simple: write to [0, size), set_size_unsafe(size)
```

## Next Steps

**Immediate**:
1. Confirm this proposal structure
2. Start with Phase 1 (documentation completion)
3. Move to Phase 2 (trait simplification) once Phase 1 done

**Review Cadence**:
- Complete each phase before moving to next
- Validate with tests after each major change
- Update docs as implementation progresses

**Priority Order**:
1. Documentation (establish baseline understanding)
2. Trait simplification (makes RingBuffer integration easier)
3. Validation (ensure stability)
4. RingBuffer (new feature on stable foundation)
5. Finalization (polish and release)
