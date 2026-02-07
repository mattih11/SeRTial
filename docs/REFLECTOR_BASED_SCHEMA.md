# Reflector-Based Schema Export: Zero-Boilerplate Introspection

**Navigation**: [Home](../README.md) | [User Guide](USER_GUIDE.md) | [Container Guide](CONTAINER_GUIDE.md) | [Examples](EXAMPLES.md) | [Schema Viewer](SCHEMA_VIEWER.md)

**Technical References**: [Serialization Mechanism](SERIALIZATION_MECHANISM.md) | [Size Calculations](SIZE_CALCULATIONS.md) | [Template Patterns](TEMPLATE_PATTERNS.md) | **Reflector Schema**

**Developer Guides**: [Adding Containers](ADDING_CONTAINERS.md) | [Container Internals](CONTAINER_INTERNALS.md)

---

## Overview

SeRTial achieves **zero-boilerplate introspection** by leveraging reflect-cpp's `rfl::Reflector` mechanism to expose compile-time metadata directly in JSON schemas. This eliminates the need for manual schema generation code while providing complete access to all StructLayout metadata.

## The Problem We Solved

### Before: Manual Schema Export (v4.0)

```cpp
// Manual export - repetitive, error-prone
struct TypeSchema {
    std::string name;
    std::size_t sizeof_bytes;
    std::size_t base_packed_size;
    // ... 20+ fields ...
};

template<typename T>
TypeSchema export_schema() {
    TypeSchema schema;
    schema.name = get_type_name<T>();  // Manual extraction
    schema.sizeof_bytes = sizeof(T);    // Manual extraction
    schema.field_names = extract_names<T>();  // Custom code
    schema.field_types = extract_types<T>();  // Custom code
    // ... 20+ manual assignments ...
    return schema;
}
```

**Problems:**
- Manual field extraction logic (complex template metaprogramming)
- Duplicated effort: metadata exists at compile-time but must be manually extracted
- Synchronization burden: Adding new StructLayout metadata requires updating export code
- Parsing overhead: JSON output requires runtime parsing to augment

### After: Reflector-Based Export (v5.1)

```cpp
// Zero boilerplate - automatic reflection
template<typename T>
std::string export_schema() {
    // That's it! One line - everything else is automatic
    return rfl::json::to_schema<StructLayout<T>>();
}
```

**Benefits:**
- **Zero manual extraction**: reflect-cpp introspects the ReflType struct automatically
- **No parsing needed**: Metadata is part of the schema structure from the start
- **Compile-time validation**: Type mismatches caught at compile time
- **Future-proof**: Adding new metadata to StructLayout is a simple struct field addition

## Architecture

### The Genius of rfl::Reflector

The key insight: **Teach reflect-cpp how to "see" StructLayout's compile-time metadata by providing a runtime representation.**

```cpp
namespace rfl {

template<typename T>
struct Reflector<sertial::StructLayout<T>> {
    // ReflType: Runtime struct mirroring compile-time metadata
    struct ReflType {
        std::string name;
        std::size_t sizeof_bytes;
        std::size_t base_packed_size;
        std::size_t max_packed_size;
        bool has_variable_fields;
        std::size_t field_count;
        
        std::vector<std::string> field_names;   // From reflection
        std::vector<std::string> field_types;   // From reflection
        std::vector<std::size_t> field_sizes;   // From StructLayout
        std::vector<std::size_t> field_offsets; // From StructLayout
        std::vector<std::size_t> field_alignments;
        std::vector<bool> field_is_variable;
        std::vector<std::size_t> element_sizes;
        std::vector<std::size_t> capacities;
        
        std::size_t fixed_block_count;
        std::size_t padding_block_count;
        std::size_t dynamic_block_count;
        std::size_t runtime_offset_block_count;
        std::size_t total_blocks;
        
        std::string type_schema;  // Nested schema of T itself
    };
    
    // Convert StructLayout's constexpr data → runtime struct
    static ReflType from(const sertial::StructLayout<T>&) {
        ReflType result;
        // Populate from StructLayout's constexpr members
        result.base_packed_size = StructLayout<T>::base_packed_size;
        result.field_sizes = std::vector(
            StructLayout<T>::field_sizes.begin(),
            StructLayout<T>::field_sizes.end()
        );
        // ... etc for all fields ...
        return result;
    }
};

} // namespace rfl
```

### How It Works

1. **User calls**: `export_schema<MyMessage>()`
2. **Internally calls**: `rfl::json::to_schema<StructLayout<MyMessage>>()`
3. **reflect-cpp sees**: "I have a Reflector for StructLayout<T>!"
4. **reflect-cpp generates**: JSON schema for `Reflector<StructLayout<T>>::ReflType`
5. **Result**: Complete JSON schema with all metadata - **no manual code**

### The Magic Flow

```
User Type (MyMessage)
    ↓
StructLayout<MyMessage>  [compile-time analysis]
    ↓                    [constexpr arrays/values]
    ↓
rfl::Reflector<StructLayout<MyMessage>>
    ↓                    [converts constexpr → runtime]
    ↓
ReflType struct         [plain C++ struct]
    ↓                    [reflect-cpp introspects]
    ↓
rfl::json::to_schema<...>()
    ↓
JSON Schema             [complete metadata, zero boilerplate!]
```

## Container Reflectors: Teaching reflect-cpp About Our Types

### The Container Problem

```cpp
struct PointCloud {
    Header header;
    fixed_vector<Point3D, 256> points;  // reflect-cpp doesn't know this type
};
```

Without container reflectors, `rfl::json::to_schema<PointCloud>()` fails because reflect-cpp doesn't know how to handle `fixed_vector`.

### The Solution: Container Reflectors

```cpp
namespace rfl {

// fixed_vector → std::vector for schema purposes
template<typename T, std::size_t N>
struct Reflector<sertial::fixed_vector<T, N>> {
    using ReflType = std::vector<T>;
    
    static ReflType from(const sertial::fixed_vector<T, N>& v) {
        return std::vector<T>(v.begin(), v.end());
    }
    
    static sertial::fixed_vector<T, N> to(const ReflType& vec) {
        sertial::fixed_vector<T, N> result;
        for (const auto& elem : vec) result.push_back(elem);
        return result;
    }
};

// fixed_string → std::string
template<std::size_t N>
struct Reflector<sertial::fixed_string<N>> {
    using ReflType = std::string;
    static ReflType from(const sertial::fixed_string<N>& s) {
        return std::string(s.c_str());
    }
    static sertial::fixed_string<N> to(const ReflType& str) {
        return sertial::fixed_string<N>(str.c_str());
    }
};

// RingBuffer → std::vector
template<typename T, std::size_t N>
struct Reflector<sertial::RingBuffer<T, N>> {
    using ReflType = std::vector<T>;
    static ReflType from(const sertial::RingBuffer<T, N>& rb) {
        std::vector<T> result;
        for (std::size_t i = 0; i < rb.size(); ++i) {
            result.push_back(rb[i]);
        }
        return result;
    }
    // ... to() implementation ...
};

} // namespace rfl
```

**Result**: reflect-cpp now sees our containers as `std::vector`/`std::string` in schemas, which it knows how to represent as JSON Schema types (`"type": "array"`, `"type": "string"`).

## Benefits of This Approach

### 1. Zero Code Duplication

**Before**: Separate code for serialization, schema export, documentation
**After**: Single `StructLayout<T>` constexpr analysis → everything else automatic

### 2. Compile-Time Guarantees

```cpp
// If StructLayout gains a new field, reflector MUST be updated
static_assert(rfl::internal::num_fields<ReflType> == 20,
              "ReflType field count mismatch!");

// If StructLayout renames a member, this fails at compile time
static_assert(requires { Layout::base_packed_size; });
```

### 3. No Runtime Parsing

**Old approach**:
1. Generate basic JSON schema
2. Parse JSON at runtime
3. Inject additional metadata
4. Re-serialize to JSON

**New approach**:
1. Call `rfl::json::to_schema<StructLayout<T>>()`
2. Done - metadata is part of the schema structure

### 4. Future-Proof Extensibility

Adding new metadata is trivial:

```diff
// In StructLayout<T>
+ static constexpr std::size_t alignment_requirement = alignof(T);

// In Reflector<StructLayout<T>>::ReflType
+ std::size_t alignment_requirement;

// In Reflector<StructLayout<T>>::from()
+ result.alignment_requirement = alignof(T);

// Update compile-time checks
- static constexpr std::size_t EXPECTED_FIELD_COUNT = 20;
+ static constexpr std::size_t EXPECTED_FIELD_COUNT = 21;
```

Compile-time assertions catch any mistakes.

### 5. Clean Separation of Concerns

- **StructLayout<T>**: Compile-time analysis (what metadata exists)
- **Reflector<StructLayout<T>>**: Bridge to runtime (how to export)
- **reflect-cpp**: Schema generation (how to format as JSON)

## Schema Output Example

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$ref": "#/$defs/sertial__StructLayout_PointCloud_",
  "$defs": {
    "sertial__StructLayout_PointCloud_": {
      "type": "object",
      "properties": {
        "name": {"type": "string"},
        "sizeof_bytes": {"type": "integer"},
        "base_packed_size": {"type": "integer"},
        "max_packed_size": {"type": "integer"},
        "has_variable_fields": {"type": "boolean"},
        "field_count": {"type": "integer"},
        "field_names": {
          "type": "array",
          "items": {"type": "string"}
        },
        "field_types": {
          "type": "array",
          "items": {"type": "string"}
        },
        "field_sizes": {
          "type": "array",
          "items": {"type": "integer"}
        },
        "capacities": {
          "type": "array",
          "items": {"type": "integer"}
        },
        "type_schema": {"type": "string"}
      }
    }
  }
}
```

**Everything needed for visualization** - field names, types, sizes, offsets, capacities, blocks - **automatically generated from compile-time metadata**.

## Maintenance Safeguards

### Compile-Time Field Count Check

```cpp
static_assert(
    rfl::internal::num_fields<ReflType> == EXPECTED_FIELD_COUNT,
    "ReflType field count mismatch!"
);
```

**Trigger**: Adding/removing fields from ReflType without updating the constant.

### StructLayout Member Existence Checks

```cpp
static_assert(requires { Layout::base_packed_size; });
static_assert(requires { Layout::field_sizes; });
// ... for all StructLayout members ...
```

**Trigger**: Renaming/removing StructLayout members - reflector won't compile.

### Field Name Documentation

```cpp
static constexpr const char* EXPECTED_FIELDS[] = {
    "name", "sizeof_bytes", "base_packed_size", // ...
};
static_assert(
    sizeof(EXPECTED_FIELDS) / sizeof(EXPECTED_FIELDS[0]) == EXPECTED_FIELD_COUNT
);
```

**Purpose**: Documents what fields should exist, enforces array matches count.

### Maintenance Checklist Comment

```cpp
// MAINTENANCE NOTE: When adding fields to StructLayout, update:
//     1. ReflType struct (add corresponding runtime field)
//     2. EXPECTED_FIELDS array (add field name)
//     3. EXPECTED_FIELD_COUNT constant (increment)
//     4. from() function (populate the new field)
//     5. static_assert checks (verify new member exists)
```

## Comparison: Manual vs. Reflector-Based

| Aspect | Manual Export (v4.0) | Reflector-Based (v5.1) |
|--------|----------------------|------------------------|
| **Code Lines** | ~300 lines | ~150 lines |
| **Boilerplate** | High (custom extraction logic) | Zero (automatic introspection) |
| **Type Safety** | Runtime errors possible | Compile-time guaranteed |
| **Extensibility** | Update 3+ functions | Add 1 struct field |
| **Performance** | Runtime parsing overhead | Direct schema generation |
| **Maintenance** | Manual synchronization | Compile-time assertions |
| **Field Names** | Manual string arrays | Automatic from reflection |
| **Field Types** | Manual type_name extraction | Automatic from reflection |

## Why This Matters

This is a **fundamental breakthrough** in C++ introspection:

1. **Compile-time metadata becomes runtime-accessible without code duplication**
2. **Type-safe introspection without macros or code generation**
3. **Zero-cost abstraction: complexity hidden, API remains simple**
4. **Future changes are compiler-enforced, not documentation-enforced**

### The Core Innovation

> **Teach a reflection library how to reflect your compile-time metadata, and you get automatic schema generation for free.**

This pattern is applicable beyond serialization - any C++ library with compile-time metadata can use this technique to expose it via JSON, YAML, or any other format supported by reflect-cpp.

## Files Involved

### Core Implementation

- **`include/sertial/core/layout/struct_layout_reflector.hpp`**
  - `rfl::Reflector<StructLayout<T>>` specialization
  - ReflType struct definition
  - Compile-time safeguards

- **`include/sertial/containers/reflectors.hpp`**
  - Container reflectors (fixed_vector, fixed_string, RingBuffer)
  - Maps custom types → std types for schema

- **`include/sertial/integration/schema_export.hpp`**
  - `export_schema<T>()` - single-line implementation
  - Just calls `rfl::json::to_schema<StructLayout<T>>()`

### Usage

```cpp
#include <sertial/integration/schema_generator.hpp>

// That's it - no manual schema code needed
auto json = export_schema<MyMessage>();
```

## Future Enhancements

Potential extensions using the same pattern:

1. **Binary schema formats** (protobuf, msgpack descriptors)
2. **Code generation** (Python/Rust/Go bindings from C++ schema)
3. **Runtime query API** (SQL-like queries on message structure)
4. **Documentation generation** (Markdown/HTML from schema)

All achievable by adding new Reflectors or using existing schema with different writers.

## Conclusion

The reflector-based approach represents a **paradigm shift** in how C++ libraries can provide introspection:

- **Before**: Manual extraction, runtime parsing, synchronization burden
- **After**: Automatic reflection, compile-time validation, zero boilerplate

This is the power of **metaprogramming done right**: complexity hidden behind a simple interface, with the compiler enforcing correctness every step of the way.
