# Padding and Cross-Architecture Portability

## Current Status: **ARCHITECTURE-DEPENDENT SERIALIZATION**

### The Problem

**Element padding varies across architectures**, making serialized data **non-portable**:

```cpp
struct PaddedElement {
    uint8_t a;   // 1 byte
    // Padding here depends on architecture alignment rules!
    uint32_t b;  // 4 bytes
};

// x86-64:     sizeof = 8 bytes (3 bytes padding)
// ARM32:      sizeof = 8 bytes (3 bytes padding) 
// Some DSPs:  sizeof = 5 bytes (0 bytes padding - byte-aligned)
// Custom:     sizeof = 12 bytes (7 bytes padding - 8-byte align)
```

### Current Behavior: Padding IS Serialized

SeRTial currently uses **direct memcpy** from C arrays:

```cpp
std::size_t data_size = field.size() * sizeof(T);
std::memcpy(dest, field.data(), data_size);  // Copies WITH padding
```

**Consequences:**
- ✅ **Fast**: Single memcpy, no per-element processing
- ✅ **Simple**: No complex packing/unpacking logic
- ❌ **Non-portable**: Serialized data tied to source architecture
- ❌ **Wasteful**: Transmits padding bytes (undefined values)
- ❌ **Fragile**: sizeof(T) must match on sender/receiver

### Example: Cross-Platform Failure

```cpp
// Sender (x86-64, 4-byte alignment):
struct Data { uint8_t a; uint32_t b; };  // sizeof = 8
fixed_vector<Data, 10> vec = {{1, 100}};
auto buffer = serialize(vec);  // Writes: [len:4][8 bytes with padding]

// Receiver (ARM with 8-byte alignment):
// sizeof(Data) = 12 bytes!  // Different padding!
auto result = deserialize<decltype(vec)>(buffer);  // CORRUPT DATA!
```

## Solutions

### Option 1: Use `__attribute__((packed))` (Current Workaround)

Force no padding using compiler attributes:

```cpp
struct Data {
    uint8_t a;
    uint32_t b;
} __attribute__((packed));  // GCC/Clang
// Or: #pragma pack(push, 1) ... #pragma pack(pop)  // MSVC

static_assert(sizeof(Data) == 5);  // Guaranteed packed
```

**Pros:**
- ✅ Portable serialization (same size everywhere)
- ✅ No padding waste
- ✅ Works with current memcpy implementation

**Cons:**
- ❌ **Performance penalty**: Unaligned access (can be slow or trap on some architectures)
- ❌ **User burden**: Must remember to pack every message struct
- ❌ **Easy to forget**: No compile-time enforcement
- ❌ **Compiler-specific**: Syntax varies

### Option 2: Per-Field Serialization (Field-by-Field)

Serialize each field individually, skipping padding:

```cpp
template<typename T>
void serialize_no_padding(const T& obj, std::byte* dest) {
    size_t offset = 0;
    rfl::visit_fields(obj, [&](const auto& field) {
        using FieldType = std::decay_t<decltype(field.value())>;
        std::memcpy(dest + offset, &field.value(), sizeof(FieldType));
        offset += sizeof(FieldType);
    });
}
```

**Pros:**
- ✅ Portable (no padding serialized)
- ✅ No user burden (automatic)
- ✅ Works with any struct layout

**Cons:**
- ❌ **Slower**: Multiple small memcpy calls instead of one large one
- ❌ **Complex**: Need recursive handling for nested structs
- ❌ **Still not perfect**: Field order matters, endianness issues remain

### Option 3: Use `rfl::Flatten` for Composition

For struct "inheritance", use `rfl::Flatten` instead of nested structs:

```cpp
// Instead of:
struct Base { uint32_t id; };
struct Derived {
    Base base;  // Nested struct (may have padding)
    float value;
};

// Use:
struct Base { uint32_t id; };
struct Derived {
    rfl::Flatten<Base> base;  // Flattened - no nesting overhead
    float value;
};
```

**Benefit**: Fields are flattened into parent struct, eliminating one level of potential padding.

### Option 4: Schema-Based Serialization (Future)

Use schema to define wire format explicitly:

```cpp
// Schema defines exact layout (independent of C++ struct)
schema: {
    "Data": {
        "a": {"type": "uint8", "offset": 0},
        "b": {"type": "uint32", "offset": 1}  // No padding in wire format
    }
}

// Serialization uses schema, not sizeof(T)
```

**Pros:**
- ✅ Complete portability
- ✅ Version evolution (add fields, change types)
- ✅ Optimal wire format (no padding)

**Cons:**
- ❌ **Major implementation effort**
- ❌ **Schema must be kept in sync with code**
- ❌ **Slower** (schema lookup overhead)

## Recommendations

### Immediate (Current Phase)

1. **Document the limitation** in README and user-facing docs:
   - Current serialization is **architecture-dependent**
   - Works for same-architecture communication (real-time systems, same machine)
   - **Not suitable** for cross-platform protocols (x86 ↔ ARM, network protocols)

2. **Recommend `__attribute__((packed))`** for portable messages:
   ```cpp
   // Real-time message (portable)
   struct SensorData {
       uint64_t timestamp;
       float value;
   } __attribute__((packed));
   
   static_assert(sizeof(SensorData) == 12, "Must be packed");
   ```

3. **Add static_assert guidance**:
   ```cpp
   // User should verify expected size
   static_assert(sizeof(MyMessage) == EXPECTED_PACKED_SIZE,
                 "Struct has unexpected padding - use __attribute__((packed))");
   ```

4. **Document `rfl::Flatten` use case**:
   - For struct composition (avoid nested struct padding)
   - Keeps fields at same level (reduces padding opportunities)

### Future (Phase 5+)

5. **Add portability mode** (opt-in per-field serialization):
   ```cpp
   template<typename T>
   concept PortableSerializable = requires {
       // Mark types that need portable serialization
       T::portable_serialization_required;
   };
   ```

6. **Provide packing macro**:
   ```cpp
   #define SERTIAL_PACKED \
       __attribute__((packed))  // GCC/Clang
       // Add MSVC, ICC variants
   
   struct Data {
       uint8_t a;
       uint32_t b;
   } SERTIAL_PACKED;
   ```

7. **Consider schema-based approach** for cross-platform protocols (major undertaking).

## Current Use Case: Real-Time Same-Architecture

SeRTial's **primary use case** is:
- **Real-time systems** (deterministic, low-latency)
- **Same architecture** (CommRaT on same machine/processor)
- **Zero allocation** (stack-only buffers)

For this use case, **current implementation is correct**:
- Fast (memcpy)
- Simple (no schema overhead)
- Predictable (deterministic execution)

### Out of Scope (Currently)

- Cross-platform network protocols (use Protobuf, FlatBuffers, etc.)
- Persistent storage (use portable format)
- x86 ↔ ARM communication (different architectures)

## Action Items

See updated TODO list:
- Document portability limitation in README
- Add `rfl::Flatten` usage examples
- Recommend `__attribute__((packed))` for portable messages
- Add static_assert examples for size validation
- Consider portable mode in future phases

## Related Documentation

- `docs/CONTAINER_HANDLING.md` - Element padding explanation
- `docs/SERIALIZATION_MECHANISM.md` - Memcpy-based serialization
- `.github/copilot-instructions.md` - Design principles
- [rfl::Flatten documentation](https://rfl.getml.com/flatten_structs/)
