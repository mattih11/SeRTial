# Adding Custom Containers to SeRTial

**Guide for extending SeRTial with your own container types**

## Documentation

**User Guides**: [USER_GUIDE.md](USER_GUIDE.md) | [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md) | [EXAMPLES.md](EXAMPLES.md) | [SCHEMA_VIEWER.md](SCHEMA_VIEWER.md)  
**Developer Guides**: [ADDING_CONTAINERS.md](ADDING_CONTAINERS.md) | [CONTAINER_INTERNALS.md](CONTAINER_INTERNALS.md)  
**Technical References**: [SERIALIZATION_MECHANISM.md](SERIALIZATION_MECHANISM.md) | [REFLECTOR_BASED_SCHEMA.md](REFLECTOR_BASED_SCHEMA.md) | [SIZE_CALCULATIONS.md](SIZE_CALCULATIONS.md) | [TEMPLATE_PATTERNS.md](TEMPLATE_PATTERNS.md)

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [SerializableContainer Concept](#serializablecontainer-concept)
3. [Step-by-Step Implementation](#step-by-step-implementation)
4. [Serialization Views](#serialization-views)
5. [Schema Metadata](#schema-metadata)
6. [Testing](#testing)
7. [Advanced: Non-Contiguous Containers](#advanced-non-contiguous-containers)

---

## Quick Start

### Minimal Example

```cpp
// 1. Implement your container
template<typename T, std::size_t N>
class my_container {
public:
    using value_type = T;
    static constexpr std::size_t max_size_v = N;
    
    std::size_t size() const { return size_; }
    const T* data() const { return data_.data(); }
    
    // For serialization
    T* data_unsafe() { return data_.data(); }
    void set_size_unsafe(std::size_t n) { size_ = n; }
    
private:
    std::array<T, N> data_;
    std::size_t size_{0};
};

// 2. Add type name in container_registration.hpp
namespace sertial {
template<typename T, std::size_t N>
struct container_type_name<my_container<T, N>> {
    static constexpr const char* value = "my_container";
};
}

// 3. Done! Container is now serializable
my_container<float, 100> container;
auto buffer = sertial::serialize(container);
```

**That's it!** No trait specializations needed - the `SerializableContainer` concept handles everything automatically.

---

## SerializableContainer Concept

SeRTial uses a **C++20 concept** to detect serializable containers at compile time.

### Requirements

From `include/sertial/containers/container_registration.hpp`:

```cpp
template<typename T>
concept SerializableContainer = requires(const T& c, T& mut_c) {
    // 1. Element type
    typename T::value_type;
    
    // 2. Compile-time maximum capacity
    { T::max_size_v } -> std::convertible_to<std::size_t>;
    
    // 3. Runtime size query
    { c.size() } -> std::same_as<std::size_t>;
    
    // 4. Read-only data access
    { c.data() } -> std::convertible_to<const typename T::value_type*>;
    
    // 5. Mutable data access (for deserialization)
    { mut_c.data_unsafe() } -> std::convertible_to<typename T::value_type*>;
    
    // 6. Unsafe size setter (for deserialization)
    { mut_c.set_size_unsafe(std::size_t{}) } -> std::same_as<void>;
};
```

### Key Points

1. **Compile-time capacity**: `max_size_v` must be a static constexpr member
2. **Runtime size**: Actual element count at runtime
3. **Contiguous storage**: `data()` returns pointer to contiguous array
4. **Unsafe methods**: Used internally for deserialization (skip validation)
5. **No nested containers**: `value_type` cannot itself be a container

---

## Step-by-Step Implementation

### Step 1: Define Container Class

```cpp
template<typename T, std::size_t N>
class my_container {
public:
    // Required type alias
    using value_type = T;
    
    // Required: compile-time maximum capacity
    static constexpr std::size_t max_size_v = N;
    
    // Standard container interface
    constexpr my_container() noexcept : size_(0) {}
    
    constexpr std::size_t size() const noexcept { return size_; }
    constexpr std::size_t capacity() const noexcept { return N; }
    constexpr bool empty() const noexcept { return size_ == 0; }
    constexpr bool full() const noexcept { return size_ == N; }
    
    // Element access
    constexpr T& operator[](std::size_t i) { return data_[i]; }
    constexpr const T& operator[](std::size_t i) const { return data_[i]; }
    
    constexpr T* data() noexcept { return data_.data(); }
    constexpr const T* data() const noexcept { return data_.data(); }
    
    // Modifiers
    constexpr void push_back(const T& value) {
        if (size_ >= N) throw std::length_error("my_container full");
        data_[size_++] = value;
    }
    
    constexpr void clear() noexcept { size_ = 0; }
    
    // ===================================================================
    // SERIALIZATION INTERFACE (required by SerializableContainer)
    // ===================================================================
    
    /// @brief Direct mutable access (unsafe - for deserialization only)
    constexpr T* data_unsafe() noexcept {
        return data_.data();
    }
    
    /// @brief Set size without validation (unsafe - for deserialization only)
    constexpr void set_size_unsafe(std::size_t n) noexcept {
        size_ = n;
    }
    
private:
    std::array<T, N> data_;
    std::size_t size_;
};
```

### Step 2: Verify Concept Satisfaction

```cpp
// Compile-time check
static_assert(sertial::SerializableContainer<my_container<int, 10>>);
static_assert(sertial::SerializableContainer<my_container<float, 100>>);
```

**If this fails**, the compiler will tell you which requirement is missing.

### Step 3: Add Type Name for Schema

Edit `include/sertial/containers/container_registration.hpp`:

```cpp
// Near the end of the file, with other container_type_name specializations:

template<typename T, std::size_t N>
struct container_type_name<my_container<T, N>> {
    static constexpr const char* value = "my_container";
};
```

This name appears in schema JSON for visualization tools.

### Step 4: Use Your Container

```cpp
struct Message {
    uint32_t id;
    my_container<float, 50> values;
};

Message msg;
msg.id = 42;
msg.values.push_back(1.5f);
msg.values.push_back(2.5f);

// Serialization works automatically!
auto buffer = sertial::serialize(msg);
auto restored = sertial::deserialize<Message>(buffer.view());

assert(restored->values.size() == 2);
assert(restored->values[0] == 1.5f);
```

---

## Serialization Views

### Default Behavior (Contiguous Storage)

For containers with contiguous storage (like `std::vector`, `fixed_vector`), the **default implementation works automatically**:

```cpp
// From container_registration.hpp - applies to your container automatically
template<SerializableContainer T>
struct serialization_view_provider {
    static constexpr auto get_spans(const T& container) {
        return std::array<std::span<const element_type>, 2>{
            std::span{container.data(), container.size()},  // All data
            std::span{}                                      // Empty
        };
    }
    
    static constexpr std::size_t span_count = 1;
};
```

**No specialization needed** unless your container has non-contiguous storage.

### When to Specialize

Only specialize `serialization_view_provider` if:
1. Your container has **non-contiguous memory** (like `RingBuffer`)
2. You need to return **multiple memory regions**

See [Advanced: Non-Contiguous Containers](#advanced-non-contiguous-containers) below.

---

## Schema Metadata

### Container Type Name

The `container_type_name<T>` specialization provides a human-readable name for schema tools:

```cpp
template<typename T, std::size_t N>
struct container_type_name<my_container<T, N>> {
    static constexpr const char* value = "my_container";
};
```

**Used in**:
- JSON schema export
- Interactive viewer
- Documentation generation

**Example schema output**:
```json
{
  "field_names": ["values"],
  "container_types": ["my_container"],
  "max_elements": [50],
  "is_variable_length": [true]
}
```

### Automatic Metadata

Once your container satisfies `SerializableContainer`, the system automatically extracts:

```cpp
using Meta = sertial::container_metadata<my_container<int, 10>>;

// Automatically available:
Meta::element_type;         // int
Meta::max_size;             // 10
Meta::element_size;         // 4
Meta::is_variable_length;   // true
Meta::is_fixed_capacity;    // true
Meta::is_serializable;      // true
```

No manual trait specializations required!

---

## Testing

### Unit Tests

```cpp
#include <catch2/catch_test_macros.hpp>
#include <sertial/sertial.hpp>

TEST_CASE("my_container serialization") {
    SECTION("Empty container") {
        my_container<int, 10> container;
        
        auto buffer = sertial::serialize(container);
        auto restored = sertial::deserialize<my_container<int, 10>>(buffer.view());
        
        REQUIRE(restored.has_value());
        REQUIRE(restored->size() == 0);
        REQUIRE(restored->empty());
    }
    
    SECTION("With elements") {
        my_container<float, 10> container;
        container.push_back(1.5f);
        container.push_back(2.5f);
        container.push_back(3.5f);
        
        auto buffer = sertial::serialize(container);
        // Wire format: [length:4][elem0:4][elem1:4][elem2:4] = 16 bytes
        REQUIRE(buffer.size() == 16);
        
        auto restored = sertial::deserialize<my_container<float, 10>>(buffer.view());
        REQUIRE(restored.has_value());
        REQUIRE(restored->size() == 3);
        REQUIRE((*restored)[0] == 1.5f);
        REQUIRE((*restored)[1] == 2.5f);
        REQUIRE((*restored)[2] == 3.5f);
    }
    
    SECTION("Full capacity") {
        my_container<uint8_t, 5> container;
        for (int i = 0; i < 5; ++i) {
            container.push_back(i);
        }
        
        auto buffer = sertial::serialize(container);
        auto restored = sertial::deserialize<my_container<uint8_t, 5>>(buffer.view());
        
        REQUIRE(restored->size() == 5);
        REQUIRE(restored->full());
    }
}

TEST_CASE("my_container concept compliance") {
    // Compile-time checks
    static_assert(sertial::SerializableContainer<my_container<int, 10>>);
    static_assert(sertial::container_max_size_v<my_container<int, 10>> == 10);
    
    using Meta = sertial::container_metadata<my_container<float, 20>>;
    static_assert(std::is_same_v<Meta::element_type, float>);
    static_assert(Meta::max_size == 20);
    static_assert(Meta::element_size == 4);
    static_assert(Meta::is_variable_length);
}

TEST_CASE("my_container in structs") {
    struct Message {
        uint32_t id;
        my_container<int, 100> data;
    };
    
    Message msg;
    msg.id = 42;
    msg.data.push_back(1);
    msg.data.push_back(2);
    
    auto buffer = sertial::serialize(msg);
    auto restored = sertial::deserialize<Message>(buffer.view());
    
    REQUIRE(restored.has_value());
    REQUIRE(restored->id == 42);
    REQUIRE(restored->data.size() == 2);
}
```

### Integration Tests

```cpp
TEST_CASE("Schema generation for my_container") {
    struct TestType {
        my_container<float, 50> values;
    };
    
    // Generate schema
    std::string schema = sertial::export_layout_data<TestType>();
    
    // Verify schema contains container metadata
    REQUIRE(schema.find("my_container") != std::string::npos);
    REQUIRE(schema.find("50") != std::string::npos);  // max_size
}
```

---

## Advanced: Non-Contiguous Containers

### When Storage is Not Contiguous

Example: Circular buffer with wrap-around:

```
Logical:  [3, 4, 5, 6, 7]
Physical: [6, 7, _, _, _, 3, 4, 5]
           ^head         ^tail
```

For such containers, specialize `serialization_view_provider`:

### Example: Circular Buffer

```cpp
template<typename T, std::size_t N>
class circular_buffer {
public:
    using value_type = T;
    static constexpr std::size_t max_size_v = N;
    
    // Standard interface...
    std::size_t size() const { return size_; }
    const T* data() const { return data_.data(); }
    T* data_unsafe() { return data_.data(); }
    void set_size_unsafe(std::size_t n) { size_ = n; }
    
    // Circular-specific
    std::size_t tail_index() const { return tail_; }
    std::size_t head_index() const { return head_; }
    bool is_wrapped() const { return tail_ > head_; }
    
private:
    std::array<T, N> data_;
    std::size_t size_{0};
    std::size_t head_{0};
    std::size_t tail_{0};
};
```

### Serialization View Specialization

```cpp
namespace sertial {

template<typename T, std::size_t N>
struct serialization_view_provider<circular_buffer<T, N>> {
    using element_type = T;
    using SpanType = std::span<const element_type>;
    
    static constexpr auto get_spans(const circular_buffer<T, N>& cb) {
        if (cb.is_wrapped()) {
            // Two spans: tail→end, start→head
            return std::array<SpanType, 2>{
                SpanType{cb.data_unsafe() + cb.tail_index(), 
                         N - cb.tail_index()},              // Tail to end
                SpanType{cb.data_unsafe(), cb.head_index()} // Start to head
            };
        } else {
            // Single span: tail→head
            return std::array<SpanType, 2>{
                SpanType{cb.data_unsafe() + cb.tail_index(), cb.size()},
                SpanType{}  // Empty
            };
        }
    }
    
    static constexpr std::size_t span_count = 2;  // May use both spans
};

} // namespace sertial
```

### Serialization Handles Multiple Spans Automatically

```cpp
// In io/unified_binary.hpp - handles 1-2 spans transparently
auto spans = sertial::get_serialization_spans(container);
for (const auto& span : spans) {
    if (span.empty()) continue;
    std::memcpy(dest, span.data(), span.size_bytes());
    dest += span.size_bytes();
}
```

**See**: `RingBuffer` implementation in `include/sertial/containers/ring_buffer.hpp` for complete example.

---

## Summary Checklist

When adding a custom container:

- [ ] Implement required members:
  - [ ] `using value_type = T;`
  - [ ] `static constexpr std::size_t max_size_v = N;`
  - [ ] `std::size_t size() const;`
  - [ ] `const T* data() const;`
  - [ ] `T* data_unsafe();`
  - [ ] `void set_size_unsafe(std::size_t);`

- [ ] Verify concept satisfaction:
  - [ ] `static_assert(sertial::SerializableContainer<MyContainer<T, N>>);`

- [ ] Add type name:
  - [ ] Specialize `container_type_name<MyContainer<T, N>>` in `container_registration.hpp`

- [ ] (Optional) Specialize serialization views:
  - [ ] Only if non-contiguous storage
  - [ ] Specialize `serialization_view_provider<MyContainer<T, N>>`

- [ ] Test:
  - [ ] Empty container serialization
  - [ ] With elements serialization
  - [ ] Round-trip (serialize → deserialize)
  - [ ] In struct fields
  - [ ] Schema generation

---

## Next Steps

- **Understand internals**: [CONTAINER_INTERNALS.md](CONTAINER_INTERNALS.md)
- **See examples**: [CONTAINER_GUIDE.md](CONTAINER_GUIDE.md)
- **Study RingBuffer**: `include/sertial/containers/ring_buffer.hpp`
- **Review serialization**: [SERIALIZATION_MECHANISM.md](SERIALIZATION_MECHANISM.md)

---

**Questions?** Open an issue: [https://github.com/mattih11/SeRTial](https://github.com/mattih11/SeRTial)
