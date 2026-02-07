# Phase 3: Decision Point and Implementation Plan

## Current State (Post-Phase 2)

### ✅ Completed
- **Phase 1**: Core serialization (fixed + variable-size types)
- **Phase 2**: Concept-based container registration
- **RingBuffer**: Full implementation with comprehensive tests
  - `include/sertial/containers/ring_buffer.hpp` (460 lines)
  - `test/test_ring_buffer.cpp` (comprehensive unit tests)
  - `examples/ring_buffer_example.cpp` (CommRaT-style demo)

### ⏸️ Pending
- **RingBuffer Serialization**: Currently NOT serializable
  - Missing `max_size_v` member (intentionally excluded from SerializableContainer)
  - Needs special wrap-around handling during serialization
  - Deserialization strategy not implemented

---

## Phase 3 Options

### Option A: Complete RingBuffer Serialization ⭐ (Recommended)

**Goal**: Make RingBuffer fully serializable with wrap-around handling

**Why prioritize this?**
- RingBuffer already implemented (460 lines of code)
- Clear use case (CommRaT message history)
- Example demonstrates timestamp-based retrieval
- Tests validate correctness
- **Missing piece**: Serialization integration

**Tasks** (8-12 hours):

#### 1. Add `max_size_v` to RingBuffer
```cpp
// ring_buffer.hpp - add static member
template<typename T, size_t MaxSize>
class RingBuffer {
public:
    using value_type = T;
    static constexpr std::size_t max_size_v = MaxSize;  // NEW
    // ... existing members ...
};
```

**Decision**: Should RingBuffer satisfy `SerializableContainer`?
- **Option 1**: Add `max_size_v`, satisfy concept → automatic serialization
- **Option 2**: Keep excluded, add explicit specialization → custom logic

**Recommendation**: **Option 2** (custom specialization)
- RingBuffer has unique wrap-around behavior
- Explicit control over linearization strategy
- Clear separation of concerns

#### 2. Design Serialization Strategy

**Document in**: `docs/work/RINGBUFFER_SERIALIZATION.md`

**Key Decisions**:

**Q1: How to handle wrap-around during serialization?**
- **Option A**: Runtime detection (if head < tail → 2 memcpy, else 1 memcpy)
- **Option B**: Always linearize (use `get_ordered_data()` helper)

**Recommendation**: **Option A** (runtime detection)
- More efficient (no extra copy for non-wrapped case)
- Aligns with zero-allocation philosophy
- Performance matters for real-time systems

**Q2: Deserialization state**
- **Always restore to non-wrapped state** (head=size, tail=0)
- Simplifies consumer logic
- Consistent post-deserialization behavior

**Q3: Serialize wrap state?**
- **No** - Always serialize in logical order (oldest → newest)
- Wrap state is internal implementation detail
- Consumers see logical sequence

#### 3. Implement Custom Serialization

**File**: `io/unified_binary.hpp`

Add specialization for RingBuffer:

```cpp
namespace detail {

template<typename T, std::size_t N>
struct serialize_ring_buffer {
    static std::size_t execute(const RingBuffer<T, N>& buf, std::byte* dest) {
        // Write length prefix
        uint32_t length = static_cast<uint32_t>(buf.size());
        std::memcpy(dest, &length, sizeof(uint32_t));
        std::byte* data_dest = dest + sizeof(uint32_t);
        
        if (length == 0) {
            return sizeof(uint32_t);
        }
        
        // Check if data wraps around
        if (buf.is_wrapped()) {
            // Two-region copy
            std::size_t first_chunk = buf.capacity() - buf.tail_index();
            std::size_t elem_size = sizeof(T);
            
            // Copy tail → end of buffer
            std::memcpy(data_dest, 
                       buf.data_unsafe() + buf.tail_index(), 
                       first_chunk * elem_size);
            
            // Copy start of buffer → head
            std::memcpy(data_dest + first_chunk * elem_size,
                       buf.data_unsafe(),
                       (length - first_chunk) * elem_size);
        } else {
            // Single contiguous region
            std::memcpy(data_dest,
                       buf.data_unsafe() + buf.tail_index(),
                       length * sizeof(T));
        }
        
        return sizeof(uint32_t) + length * sizeof(T);
    }
};

} // namespace detail
```

**Required RingBuffer API additions**:
```cpp
// ring_buffer.hpp - add internal accessors
constexpr bool is_wrapped() const noexcept {
    return size_ > 0 && head_ <= tail_;
}

constexpr size_type tail_index() const noexcept {
    return tail_;
}

constexpr size_type head_index() const noexcept {
    return head_;
}
```

#### 4. Implement Deserialization

```cpp
template<typename T, std::size_t N>
struct deserialize_ring_buffer {
    static bool execute(RingBuffer<T, N>& buf, const std::byte* src, std::size_t available) {
        if (available < sizeof(uint32_t)) {
            return false;
        }
        
        uint32_t length;
        std::memcpy(&length, src, sizeof(uint32_t));
        
        if (length > N) {
            return false;  // Exceeds capacity
        }
        
        const std::byte* data_src = src + sizeof(uint32_t);
        std::size_t data_size = length * sizeof(T);
        
        if (available < sizeof(uint32_t) + data_size) {
            return false;
        }
        
        // Deserialize to non-wrapped state
        std::memcpy(buf.data_unsafe(), data_src, data_size);
        buf.set_size_unsafe(length);  // Sets head=length, tail=0
        
        return true;
    }
};
```

#### 5. Update HybridMemoryMap Traits

**File**: `core/traits/memory_map.hpp`

Add RingBuffer detection:

```cpp
// Detect RingBuffer as variable-length field
template<typename T, std::size_t N>
struct is_variable_length_field<RingBuffer<T, N>> : std::true_type {};

template<typename T, std::size_t N>
struct variable_length_element_size<RingBuffer<T, N>> {
    static constexpr std::size_t value = sizeof(T);
};

template<typename T, std::size_t N>
struct variable_length_max_elements<RingBuffer<T, N>> {
    static constexpr std::size_t value = N;
};
```

#### 6. Schema Generation

**File**: `integration/schema_generator.hpp`

Update TypeSchema for RingBuffer:

```cpp
{
    "field_name": "history",
    "type": "RingBuffer<float, 100>",
    "container_type": "ring_buffer",
    "is_variable_length": true,
    "element_type": "float",
    "element_size": 4,
    "max_elements": 100,
    "circular_behavior": "fifo_overwrite",
    "serialization_format": "linearized_oldest_to_newest"
}
```

#### 7. Python Viewer Updates

**Files**: 
- `scripts/visualize_schema.py` (CLI)
- `scripts/visualize_schema_gui.py` (GUI)

Add RingBuffer rendering:

```python
def render_ring_buffer(field_info, indent=0):
    """Render RingBuffer field information"""
    prefix = "  " * indent
    print(f"{prefix}RingBuffer<{field_info['element_type']}, {field_info['max_elements']}>")
    print(f"{prefix}  Circular FIFO (oldest overwritten when full)")
    print(f"{prefix}  Serialization: Linearized (logical order)")
    print(f"{prefix}  Max elements: {field_info['max_elements']}")
    print(f"{prefix}  Element size: {field_info['element_size']} bytes")
```

#### 8. Comprehensive Testing

**New test file**: `test/test_ring_buffer_serialization.cpp`

Test scenarios:
1. Empty buffer
2. Partial fill (size < capacity)
3. Full capacity (size == capacity)
4. Post-overwrite (has wrapped around)
5. Single element
6. Non-trivial element type (struct with padding)

Verify:
- Size calculations match actual
- Data preserved correctly
- Logical order maintained (oldest → newest)
- Deserialization produces non-wrapped state

#### 9. Documentation

Update:
- `README.md` - Phase 3 status
- `docs/CONTAINER_HANDLING.md` - RingBuffer serialization section
- `docs/SERIALIZATION_MECHANISM.md` - RingBuffer special case
- Create `docs/work/PHASE3_COMPLETION.md` - Summary report

---

### Option B: Portable Serialization (Cross-Platform)

**Goal**: Enable serialization across different architectures

**Why consider this?**
- Current serialization is architecture-dependent (padding, endianness)
- Enables heterogeneous systems (x86 ↔ ARM, different compilers)
- Production deployment requirement for many users

**Challenges**:
- More complex implementation
- Performance trade-offs (byte-swapping overhead)
- Struct layout portability difficult
- May require wire format versioning

**Tasks** (16-24 hours):

1. **Endianness Handling**
   - Detect native endianness at compile time
   - Implement byte-swapping for multi-byte integers
   - Add `serialize_portable()` / `deserialize_portable()` APIs

2. **Alignment Portability**
   - Document which structs are portable vs architecture-specific
   - Consider `__attribute__((packed))` for portable structs
   - Add static assertions for portable struct validation

3. **CI/Docker Multi-Arch Testing**
   - GitHub Actions workflow for x86_64, ARM, RISC-V
   - Docker containers for cross-compilation
   - Serialization compatibility tests between architectures

4. **Documentation**
   - Update `PADDING_AND_PORTABILITY.md` with concrete examples
   - Add portable struct design guidelines
   - Document performance implications

**Recommendation**: **Defer to Phase 4+**
- Less immediate value (most users same-arch IPC)
- More complex with less clear ROI
- Better tackled after RingBuffer completion

---

## Recommendation: Option A (RingBuffer Serialization)

### Why Option A First?

1. **Momentum**: RingBuffer 90% complete, just needs serialization integration
2. **Clear Value**: CommRaT use case already demonstrated
3. **Learning**: Validates Phase 2 architecture with non-trivial container
4. **Completeness**: Finishes what we started
5. **Simpler**: More focused scope than cross-platform portability

### Estimated Effort

- **Design & Doc**: 2-3 hours
- **Implementation**: 3-4 hours (serialization + deserialization)
- **Testing**: 2-3 hours (comprehensive test suite)
- **Schema/Viewers**: 1-2 hours (Python updates)
- **Total**: **8-12 hours**

### Success Criteria

- [ ] RingBuffer serializes correctly (empty, partial, full, wrapped)
- [ ] Deserialization produces non-wrapped state
- [ ] Size calculations accurate (compile-time max, runtime actual)
- [ ] Schema generation exports RingBuffer metadata
- [ ] Python viewers render RingBuffer fields
- [ ] All tests pass (unit + serialization round-trip)
- [ ] Documentation updated (Phase 3 complete)

---

## Phase 4 Preview

After RingBuffer serialization (Phase 3):

**High-Priority Options**:
1. **Nested Containers** - Support `fixed_vector<fixed_vector<T, M>, N>`
2. **Portable Serialization** - Endianness + alignment handling
3. **Performance Profiling** - Benchmark suite, optimization opportunities
4. **Additional Containers** - `fixed_map`, `fixed_set`, ring_buffer variants

**Medium-Priority**:
- Schema versioning
- Optional fields
- Compression support
- DDS/ROS 2 integration adapters

---

## Decision

**Proceeding with: Option A - RingBuffer Serialization**

**Next Steps**:
1. Create `docs/work/RINGBUFFER_SERIALIZATION.md` (design document)
2. Add internal accessors to `ring_buffer.hpp` (is_wrapped, tail_index, head_index)
3. Implement serialize/deserialize specializations
4. Add comprehensive tests
5. Update schema generation and Python viewers
6. Document completion

**Status**: Ready to begin implementation
