# Template Metaprogramming Patterns in SeRTial

**Navigation**: [Home](../README.md) | [User Guide](USER_GUIDE.md) | [Container Guide](CONTAINER_GUIDE.md) | [Examples](EXAMPLES.md) | [Schema Viewer](SCHEMA_VIEWER.md)

**Technical References**: [Serialization Mechanism](SERIALIZATION_MECHANISM.md) | [Size Calculations](SIZE_CALCULATIONS.md) | **Template Patterns** | [Reflector Schema](REFLECTOR_BASED_SCHEMA.md)

**Developer Guides**: [Adding Containers](ADDING_CONTAINERS.md) | [Container Internals](CONTAINER_INTERNALS.md)

---

## Overview

SeRTial makes extensive use of C++20 template metaprogramming to achieve **compile-time type analysis**, **zero runtime overhead**, and **type-safe serialization**. This document captures the patterns, techniques, and lessons learned during development.

## Core Philosophy

**If it can be computed at compile time, it SHALL be computed at compile time.**

- Type traits → compile-time analysis
- Buffer sizes → compile-time constants
- Dispatch logic → compile-time branching (`if constexpr`)
- Validation → `static_assert` (fail at compile time, not runtime)

## C++20 Concepts

### Pattern: Interface Requirements as Concepts

**Use concepts to define container requirements** instead of duck-typing or SFINAE:

```cpp
/// @brief Concept: Serializable fixed-capacity container
template<typename T>
concept SerializableContainer = requires(const T& c, T& mut_c) {
    // Type requirements
    typename T::value_type;
    
    // Compile-time constant requirements
    { T::max_size_v } -> std::convertible_to<std::size_t>;
    
    // Method requirements
    { c.size() } -> std::same_as<std::size_t>;
    { c.data() } -> std::convertible_to<const typename T::value_type*>;
    
    // Mutable interface (for deserialization)
    { mut_c.data_unsafe() } -> std::convertible_to<typename T::value_type*>;
    { mut_c.set_size_unsafe(std::size_t{}) } -> std::same_as<void>;
} && 
// Negative requirement: reject nested containers
!requires { typename T::value_type::max_size_v; };
```

**Benefits:**
- **Clear error messages**: Compiler shows exactly which requirement failed
- **Self-documenting**: Concept defines the interface contract
- **Compile-time validation**: Errors at instantiation, not deep in call stack
- **Composable**: Can combine concepts with `&&`, `||`

**Usage:**
```cpp
// Function constraint
template<SerializableContainer T>
auto serialize(const T& container);

// Trait constraint
template<SerializableContainer T>
struct container_metadata { /* ... */ };

// Static assertion
static_assert(SerializableContainer<fixed_vector<int, 10>>);
```

### Pattern: Negative Requirements

**Reject types that satisfy unwanted properties**:

```cpp
template<typename T>
concept NotNestedContainer = 
    SerializableContainer<T> && 
    !requires { typename T::value_type::max_size_v; };
```

**Use case**: Prevent nested containers (e.g., `fixed_vector<fixed_vector<int, 5>, 10>`).

**Caveat**: Negative requirements may not work perfectly on all compilers. Consider additional static assertions:

```cpp
template<SerializableContainer T>
struct container_metadata {
    // Explicit compile-time check
    static_assert(!requires { typename T::value_type::max_size_v; },
                  "Nested containers not supported");
    // ...
};
```

## SFINAE and Template Specialization

### Anti-Pattern: Ternary Conditional in Variable Template

**WRONG** - Both branches are instantiated, even if condition is false:

```cpp
template<typename T>
inline constexpr std::size_t capacity_v = 
    SerializableContainer<T> ? container_max_size_v<T> : 0;
    
// Problem: For T = int (not a container)
// Compiler evaluates: SerializableContainer<int> ? container_max_size_v<int> : 0
//                                                   ^^^^^^^^^^^^^^^^^^^^^^^^^
//                                                   ERROR: no specialization for int
```

**Why it fails**: C++ eagerly instantiates both branches of the ternary operator before selecting one. If `container_max_size_v<int>` has no specialization, compilation fails even though the false branch would be chosen.

### Correct Pattern: Template Specialization

**CORRECT** - SFINAE-friendly, only instantiates matching specialization:

```cpp
// Base template (fallback for non-containers)
template<typename T>
struct capacity_impl {
    static constexpr std::size_t value = 0;
};

// Specialized template (only for SerializableContainer types)
template<SerializableContainer T>
struct capacity_impl<T> {
    static constexpr std::size_t value = container_max_size_v<T>;
};

// Convenience alias
template<typename T>
inline constexpr std::size_t capacity_v = capacity_impl<T>::value;
```

**Why it works**: Template specialization uses **SFINAE (Substitution Failure Is Not An Error)**. When instantiating `capacity_v<int>`:
1. Tries specialized template `capacity_impl<int>` (requires `SerializableContainer<int>`)
2. Substitution fails (int is not a SerializableContainer)
3. Falls back to base template → `value = 0` (SUCCESS)
4. Never instantiates `container_max_size_v<int>` (avoids error)

### Pattern: Layered Specialization

For multiple categories of types:

```cpp
// Layer 0: Base (default)
template<typename T>
struct trait { static constexpr bool value = false; };

// Layer 1: Concept-based specialization
template<SerializableContainer T>
struct trait<T> { static constexpr bool value = true; };

// Layer 2: Specific type specialization
template<>
struct trait<std::vector<int>> { static constexpr bool value = true; };

// Layer 3: Template parameter specialization
template<typename T, std::size_t N>
struct trait<std::array<T, N>> { static constexpr bool value = true; };
```

**Resolution order**: Most specific → least specific (Layer 3 → Layer 0)

## Compile-Time Branching

### Pattern: `if constexpr` for Zero-Cost Dispatch

**Use `if constexpr` for compile-time branching** (no runtime cost):

```cpp
template<typename T>
std::size_t calculate_size(const T& obj) {
    if constexpr (SerializableContainer<T>) {
        // Branch 1: Container (length prefix + data)
        return sizeof(uint32_t) + obj.size() * sizeof(typename T::value_type);
    } else if constexpr (std::is_arithmetic_v<T>) {
        // Branch 2: Primitive type
        return sizeof(T);
    } else {
        // Branch 3: Struct (recurse via reflection)
        return calculate_struct_size(obj);
    }
}
```

**Benefits:**
- **Zero runtime cost**: Dead branches eliminated at compile time
- **Type-safe**: Each branch only compiles if condition is true
- **Readable**: Clear intent (unlike SFINAE tricks)

**Contrast with runtime `if`**:
```cpp
// Runtime branching (slow, requires runtime type info)
if (is_container(obj)) {  // Virtual call or RTTI
    return calculate_container_size(obj);
} else {
    return calculate_primitive_size(obj);
}
```

## Variadic Templates

### Pattern: Field Iteration via Parameter Pack

**Iterate over struct fields using variadic templates**:

```cpp
template<typename... Fields>
constexpr auto analyze_fields(rfl::NamedTuple<Fields...>*) {
    return std::array<FieldInfo, sizeof...(Fields)>{
        analyze_single_field<Fields>()...  // Pack expansion
    };
}

template<typename Field>
constexpr FieldInfo analyze_single_field() {
    using FieldType = typename Field::Type;
    return FieldInfo{
        .name = Field::name(),
        .size = sizeof(FieldType),
        .is_container = SerializableContainer<FieldType>
    };
}
```

**Usage with reflect-cpp**:
```cpp
struct Player {
    uint32_t id;
    fixed_vector<float, 10> data;
    uint64_t timestamp;
};

// reflect-cpp provides: rfl::NamedTuple<Field<"id", uint32_t>, Field<"data", ...>, ...>
using Fields = rfl::named_tuple_t<Player>;
constexpr auto info = analyze_fields(static_cast<Fields*>(nullptr));
```

### Pattern: Fold Expressions for Aggregation

**Compute aggregate values from parameter pack**:

```cpp
// Sum of all field sizes
template<typename... Fields>
constexpr std::size_t total_size() {
    return (sizeof(typename Fields::Type) + ...);  // Fold expression
}

// Check if any field is a container
template<typename... Fields>
constexpr bool has_containers() {
    return (SerializableContainer<typename Fields::Type> || ...);
}

// Count variable-length fields
template<typename... Fields>
constexpr std::size_t count_variable_fields() {
    return ((SerializableContainer<typename Fields::Type> ? 1 : 0) + ...);
}
```

**Fold expression syntax:**
- `(... op pack)` - Left fold: `((pack[0] op pack[1]) op pack[2]) ...`
- `(pack op ...)` - Right fold: `pack[0] op (pack[1] op (pack[2] ...))`
- `(init op ... op pack)` - Binary fold with initial value

## Type Traits

### Pattern: Trait Template + Variable Template

**Standard pattern for reusable type properties**:

```cpp
// 1. Primary template (default case)
template<typename T>
struct is_container_impl : std::false_type {};

// 2. Specializations for specific types
template<typename T, std::size_t N>
struct is_container_impl<fixed_vector<T, N>> : std::true_type {};

template<std::size_t N>
struct is_container_impl<fixed_string<N>> : std::true_type {};

// 3. Convenience variable template (preferred usage)
template<typename T>
inline constexpr bool is_container_v = is_container_impl<T>::value;

// 4. Usage
static_assert(is_container_v<fixed_vector<int, 10>>);
static_assert(!is_container_v<int>);
```

**Naming conventions:**
- Trait struct: `trait_name` or `trait_name_impl`
- Variable template: `trait_name_v` (preferred for users)
- Type alias: `trait_name_t` (for extracting types)

### Pattern: Concept-Based Trait

**Modern approach using concepts** (replaces manual specializations):

```cpp
// Old way: Manual specialization for each type
template<typename T> struct is_container : std::false_type {};
template<typename T, std::size_t N> 
struct is_container<fixed_vector<T, N>> : std::true_type {};
template<std::size_t N> 
struct is_container<fixed_string<N>> : std::true_type {};
// ... repeat for every container type

// New way: Single concept check
template<typename T>
inline constexpr bool is_container_v = SerializableContainer<T>;
```

**Benefits:**
- **No specializations needed**: Concept applies to all matching types
- **Automatic**: New types work immediately if they satisfy the concept
- **Clear errors**: Compiler shows which requirement failed

## Constexpr Functions

### Pattern: Compile-Time Size Computation

**Compute buffer sizes at compile time**:

```cpp
template<typename T>
consteval std::size_t max_serialized_size() {
    if constexpr (std::is_arithmetic_v<T>) {
        return sizeof(T);
    } else if constexpr (SerializableContainer<T>) {
        return sizeof(uint32_t) +  // Length prefix
               T::max_size_v * sizeof(typename T::value_type);
    } else {
        // Struct: sum of field max sizes
        return compute_struct_max_size<T>();
    }
}

// Usage: Compile-time stack buffer allocation
template<typename T>
auto serialize(const T& obj) {
    constexpr std::size_t max_size = max_serialized_size<T>();
    static_buffer<max_size> buffer;  // Stack-allocated
    // ... serialize into buffer ...
    return buffer;
}
```

**`constexpr` vs `consteval`:**
- `constexpr`: CAN be evaluated at compile time (may also run at runtime)
- `consteval`: MUST be evaluated at compile time (C++20)

**Use `consteval` when:**
- Result is used in constant expressions (e.g., template parameters, array sizes)
- Want to guarantee compile-time evaluation
- Prevent accidental runtime overhead

## Static Assertions

### Pattern: Compile-Time Validation

**Validate assumptions at compile time**:

```cpp
// Concept satisfaction
static_assert(SerializableContainer<fixed_vector<int, 10>>,
              "fixed_vector must satisfy SerializableContainer");

// Nested container rejection
static_assert(!SerializableContainer<fixed_vector<fixed_vector<int, 5>, 10>>,
              "Nested containers must be rejected");

// Size calculations
static_assert(max_serialized_size<Player>() < 10'000,
              "Player too large - check for unbounded types");

// Trait consistency
static_assert(is_container_v<T> == SerializableContainer<T>,
              "Trait and concept must agree");
```

**Benefits:**
- **Fail fast**: Errors at compilation, not deployment
- **Documentation**: Assertions serve as executable specifications
- **Zero runtime cost**: Validation is free (compile-time only)

### Pattern: Debug-Only Assertions

**Check expensive properties only in debug builds**:

```cpp
#ifndef NDEBUG
// Expensive check: all fields are trivially copyable
template<typename T>
constexpr bool all_fields_trivial() {
    // ... complex reflection-based check ...
}

static_assert(all_fields_trivial<Message>(),
              "All fields must be trivially copyable");
#endif
```

## Common Pitfalls

### Pitfall 1: Forward Declarations and Concepts

**Problem**: Evaluating concepts before types are fully defined:

```cpp
// container_registration.hpp
template<typename T> class fixed_vector;  // Forward declaration

template<typename T>
concept SerializableContainer = requires(T c) {
    { c.size() } -> std::same_as<std::size_t>;  // ERROR: incomplete type
};

static_assert(SerializableContainer<fixed_vector<int, 10>>);  // Fails!
```

**Solution**: Move static assertions to a file that includes full definitions:

```cpp
// container_traits.hpp
#include "fixed_vector.hpp"  // Full definition

// Now the assertion works:
static_assert(SerializableContainer<fixed_vector<int, 10>>);  // SUCCESS
```

### Pitfall 2: Ternary Conditionals with Templates

**Problem**: Both branches instantiated (see SFINAE section above)

```cpp
// WRONG
template<typename T>
constexpr auto get_size() {
    return is_container_v<T> ? T::max_size_v : sizeof(T);
}
```

**Solution**: Use `if constexpr` or template specialization:

```cpp
// CORRECT
template<typename T>
constexpr auto get_size() {
    if constexpr (is_container_v<T>) {
        return T::max_size_v;
    } else {
        return sizeof(T);
    }
}
```

### Pitfall 3: Concept Overload Resolution

**Problem**: Multiple concepts overlap, causing ambiguity:

```cpp
template<typename T>
concept NumericType = std::is_arithmetic_v<T>;

template<typename T>
concept IntegralType = std::is_integral_v<T>;

// Ambiguous for int (satisfies both concepts)
void process(NumericType auto x);
void process(IntegralType auto y);

process(42);  // Ambiguous!
```

**Solution**: Make concepts mutually exclusive or add subsumption:

```cpp
// Option 1: Mutual exclusion
template<typename T>
concept FloatingType = std::is_floating_point_v<T>;

template<typename T>
concept IntegralType = std::is_integral_v<T>;

// Option 2: Subsumption (more specific concept)
template<typename T>
concept SignedIntegral = std::is_integral_v<T> && std::is_signed_v<T>;
```

## Best Practices

### 1. Prefer Concepts Over SFINAE

```cpp
// Old way: SFINAE
template<typename T, std::enable_if_t<is_container_v<T>, int> = 0>
void serialize(const T& obj);

// New way: Concepts
template<SerializableContainer T>
void serialize(const T& obj);
```

### 2. Use `constexpr` Liberally

```cpp
// Make everything constexpr that can be
constexpr std::size_t compute_size() { return 42; }
constexpr bool is_valid() { return true; }

// Enables compile-time usage
static_assert(compute_size() == 42);
std::array<int, compute_size()> arr;
```

### 3. Document Concept Requirements

```cpp
/// @brief Serializable container concept
/// 
/// Requires:
/// - typename value_type - element type
/// - static constexpr size_t max_size_v - compile-time capacity
/// - size_t size() const - runtime element count
/// - const T* data() const - pointer to elements
/// - T* data_unsafe() - mutable access (deserialization)
/// - void set_size_unsafe(size_t) - direct size setter (deserialization)
/// 
/// Rejects:
/// - Nested containers (value_type cannot be a container)
template<typename T>
concept SerializableContainer = /* ... */;
```

### 4. Static Assert Everything

```cpp
// Validate compile-time assumptions
static_assert(sizeof(uint32_t) == 4);
static_assert(std::endian::native == std::endian::little);  // If required
static_assert(!has_padding<Header>());

// Verify concept logic
static_assert(SerializableContainer<fixed_vector<int, 10>>);
static_assert(!SerializableContainer<int>);
```

### 5. Use Type Aliases for Clarity

```cpp
// Hard to read
using Callback = std::function<void(const fixed_vector<fixed_string<256>, 10>&)>;

// Clearer
using Name = fixed_string<256>;
using NameList = fixed_vector<Name, 10>;
using Callback = std::function<void(const NameList&)>;
```

## Summary

**Key Takeaways:**

1. **C++20 Concepts** provide clear, self-documenting type constraints
2. **Template Specialization** (not ternary conditionals) for SFINAE-friendly traits
3. **`if constexpr`** for zero-cost compile-time branching
4. **`constexpr`/`consteval`** move computation from runtime to compile time
5. **Static Assertions** catch errors early (compile time, not deployment)
6. **Variadic Templates** enable generic field iteration and analysis

**Resources:**

- [C++20 Concepts (cppreference)](https://en.cppreference.com/w/cpp/language/constraints)
- [Template Metaprogramming (Modern C++ Design)](https://www.boost.org/doc/libs/1_84_0/libs/mp11/doc/html/index.html)
- [reflect-cpp Documentation](https://github.com/getml/reflect-cpp)

**SeRTial-Specific Patterns:**

- `containers/container_registration.hpp` - Concept-based container detection
- `core/traits/hybrid_memory_map.hpp` - Compile-time layout analysis
- `io/unified_binary.hpp` - Template-based serialization dispatch
