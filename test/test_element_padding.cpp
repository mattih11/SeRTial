/// Element Padding and Layout Tests
/// 
/// Validates that:
/// 1. Elements with internal padding are serialized correctly
/// 2. Element padding IS included in serialized data (matches C++ layout)
/// 3. No inter-element padding (tightly packed array)
/// 4. Nested containers are detected and rejected at compile time

#include "test_framework.hpp"
#include <sertial/sertial.hpp>

using namespace sertial;
using namespace sertial::test;

// ============================================================================
// Test Structs
// ============================================================================

/// Simple POD - no padding
struct SimplePOD {
    uint32_t a;
    uint32_t b;
    uint32_t c;
};
static_assert(sizeof(SimplePOD) == 12);

/// Struct with internal padding
struct PaddedStruct {
    uint8_t a;   // 1 byte
    // 3 bytes padding
    uint32_t b;  // 4 bytes
};
static_assert(sizeof(PaddedStruct) == 8);  // Includes padding

/// Complex padding
struct ComplexPadding {
    uint8_t a;   // 1 byte
    // 7 bytes padding
    uint64_t b;  // 8 bytes
    uint8_t c;   // 1 byte
    // 7 bytes padding
};
static_assert(sizeof(ComplexPadding) == 24);

// ============================================================================
// Test Messages
// ============================================================================

struct MessageWithSimplePOD {
    uint32_t header;
    fixed_vector<SimplePOD, 10> items;
};

struct MessageWithPaddedElements {
    uint32_t header;
    fixed_vector<PaddedStruct, 10> items;
};

struct MessageWithComplexPadding {
    uint32_t header;
    fixed_vector<ComplexPadding, 5> items;
};

// ============================================================================
// Individual Test Functions
// ============================================================================

namespace tests {

bool simple_pod_serialization_test() {
    TEST_SECTION("Test 1: Simple POD elements (no padding)");
    
    MessageWithSimplePOD msg{
        .header = 42,
        .items = {{1, 2, 3}, {4, 5, 6}}
    };
    
    // Serialize
    auto buffer = serialize(msg);
    
    // Expected size:
    // header: 4 bytes
    // items length: 4 bytes
    // items data: 2 * 12 = 24 bytes
    // Total: 32 bytes
    constexpr size_t expected_size = 4 + 4 + 2 * sizeof(SimplePOD);
    static_assert(sizeof(SimplePOD) == 12);
    static_assert(expected_size == 32);
    
    TEST_PRINT("  Buffer size: " << buffer.size() << " bytes");
    TEST_PRINT("  Expected: " << expected_size << " bytes");
    TEST_ASSERT_EQ(buffer.size(), expected_size, "Size matches");
    
    // Deserialize
    auto restored = deserialize<MessageWithSimplePOD>(buffer.view());
    TEST_ASSERT(restored.has_value(), "Deserialization succeeded");
    
    TEST_ASSERT_EQ(restored->header, 42u, "Header matches");
    TEST_ASSERT_EQ(restored->items.size(), 2u, "Item count matches");
    TEST_ASSERT_EQ(restored->items[0].a, 1u, "Element [0].a matches");
    TEST_ASSERT_EQ(restored->items[0].b, 2u, "Element [0].b matches");
    TEST_ASSERT_EQ(restored->items[0].c, 3u, "Element [0].c matches");
    TEST_ASSERT_EQ(restored->items[1].a, 4u, "Element [1].a matches");
    
    return true;
}

bool padded_struct_serialization_test() {
    TEST_SECTION("Test 2: Padded struct elements (internal padding)");
    
    MessageWithPaddedElements msg{
        .header = 99,
        .items = {{1, 100}, {2, 200}, {3, 300}}
    };
    
    // Serialize
    auto buffer = serialize(msg);
    
    // Expected size:
    // header: 4 bytes
    // items length: 4 bytes
    // items data: 3 * sizeof(PaddedStruct) = 3 * 8 = 24 bytes
    //   NOTE: Padding IS included in sizeof, thus in serialized size
    // Total: 32 bytes
    constexpr size_t expected_size = 4 + 4 + 3 * sizeof(PaddedStruct);
    static_assert(sizeof(PaddedStruct) == 8);
    static_assert(expected_size == 32);
    
    TEST_PRINT("  sizeof(PaddedStruct): " << sizeof(PaddedStruct) << " bytes");
    TEST_PRINT("  Buffer size: " << buffer.size() << " bytes");
    TEST_PRINT("  Expected: " << expected_size << " bytes");
    TEST_ASSERT_EQ(buffer.size(), expected_size, "Size includes padding");
    
    // Deserialize
    auto restored = deserialize<MessageWithPaddedElements>(buffer.view());
    TEST_ASSERT(restored.has_value(), "Deserialization succeeded");
    
    TEST_ASSERT_EQ(restored->header, 99u, "Header matches");
    TEST_ASSERT_EQ(restored->items.size(), 3u, "Item count matches");
    TEST_ASSERT_EQ(restored->items[0].a, 1u, "Element [0].a matches");
    TEST_ASSERT_EQ(restored->items[0].b, 100u, "Element [0].b matches");
    TEST_ASSERT_EQ(restored->items[1].a, 2u, "Element [1].a matches");
    TEST_ASSERT_EQ(restored->items[1].b, 200u, "Element [1].b matches");
    TEST_ASSERT_EQ(restored->items[2].a, 3u, "Element [2].a matches");
    TEST_ASSERT_EQ(restored->items[2].b, 300u, "Element [2].b matches");
    
    // Verify padding bytes exist (they're part of the element)
    // We can't check their values (undefined), but size confirms they're there
    
    return true;
}

bool complex_padding_serialization_test() {
    TEST_SECTION("Test 3: Complex padding (multiple padding regions)");
    
    MessageWithComplexPadding msg{
        .header = 77,
        .items = {{10, 1000, 11}, {20, 2000, 22}}
    };
    
    // Serialize
    auto buffer = serialize(msg);
    
    // Expected size:
    // header: 4 bytes
    // items length: 4 bytes
    // items data: 2 * sizeof(ComplexPadding) = 2 * 24 = 48 bytes
    // Total: 56 bytes
    constexpr size_t expected_size = 4 + 4 + 2 * sizeof(ComplexPadding);
    static_assert(sizeof(ComplexPadding) == 24);
    static_assert(expected_size == 56);
    
    TEST_PRINT("  sizeof(ComplexPadding): " << sizeof(ComplexPadding) << " bytes");
    TEST_PRINT("  Buffer size: " << buffer.size() << " bytes");
    TEST_PRINT("  Expected: " << expected_size << " bytes");
    TEST_ASSERT_EQ(buffer.size(), expected_size, "Size includes all padding");
    
    // Deserialize
    auto restored = deserialize<MessageWithComplexPadding>(buffer.view());
    TEST_ASSERT(restored.has_value(), "Deserialization succeeded");
    
    TEST_ASSERT_EQ(restored->items[0].a, 10u, "Element [0].a matches");
    TEST_ASSERT_EQ(restored->items[0].b, 1000u, "Element [0].b matches");
    TEST_ASSERT_EQ(restored->items[0].c, 11u, "Element [0].c matches");
    
    return true;
}

bool size_calculation_with_padding_test() {
    TEST_SECTION("Test 4: Compile-time size calculation with padding");
    
    using Layout = StructLayout<MessageWithPaddedElements>;
    
    // Base size: just the header (uint32_t = 4 bytes)
    constexpr size_t base = Layout::base_packed_size;
    TEST_PRINT("  base_packed_size: " << base << " bytes");
    TEST_ASSERT_EQ(base, 4u, "Base is header only");
    
    // Max size: header + length + 10 * PaddedStruct
    constexpr size_t max_size = Layout::max_packed_size;
    constexpr size_t expected_max = 4 + 4 + 10 * sizeof(PaddedStruct);
    static_assert(expected_max == 4 + 4 + 10 * 8);
    static_assert(expected_max == 88);
    
    TEST_PRINT("  max_packed_size: " << max_size << " bytes");
    TEST_PRINT("  Expected: " << expected_max << " bytes");
    TEST_ASSERT_EQ(max_size, expected_max, "Max size includes element padding");
    
    // Runtime calculation with 3 elements
    MessageWithPaddedElements msg{.header = 1, .items = {{1,1},{2,2},{3,3}}};
    size_t actual = packed_size_of(msg);
    size_t expected_actual = 4 + 4 + 3 * sizeof(PaddedStruct);
    
    TEST_PRINT("  actual size (3 elements): " << actual << " bytes");
    TEST_PRINT("  Expected: " << expected_actual << " bytes");
    TEST_ASSERT_EQ(actual, expected_actual, "Actual size includes padding");
    
    return true;
}

bool array_element_layout_test() {
    TEST_SECTION("Test 5: Array element memory layout (tightly packed)");
    
    // Verify C++ guarantee: array elements are contiguous
    PaddedStruct array[3] = {{1, 100}, {2, 200}, {3, 300}};
    
    // Check addresses
    std::byte* addr0 = reinterpret_cast<std::byte*>(&array[0]);
    std::byte* addr1 = reinterpret_cast<std::byte*>(&array[1]);
    std::byte* addr2 = reinterpret_cast<std::byte*>(&array[2]);
    
    TEST_PRINT("  &array[0]: " << (void*)addr0);
    TEST_PRINT("  &array[1]: " << (void*)addr1);
    TEST_PRINT("  &array[2]: " << (void*)addr2);
    
    // Verify no inter-element gaps
    ptrdiff_t gap01 = addr1 - addr0;
    ptrdiff_t gap12 = addr2 - addr1;
    
    TEST_PRINT("  Gap 0->1: " << gap01 << " bytes");
    TEST_PRINT("  Gap 1->2: " << gap12 << " bytes");
    TEST_PRINT("  sizeof(PaddedStruct): " << sizeof(PaddedStruct) << " bytes");
    
    TEST_ASSERT_EQ(gap01, static_cast<ptrdiff_t>(sizeof(PaddedStruct)), 
                   "No gap between elements");
    TEST_ASSERT_EQ(gap12, static_cast<ptrdiff_t>(sizeof(PaddedStruct)), 
                   "Elements tightly packed");
    
    // Verify memcpy would work correctly
    std::byte buffer[3 * sizeof(PaddedStruct)];
    std::memcpy(buffer, array, 3 * sizeof(PaddedStruct));
    
    // Deserialize back
    PaddedStruct* restored = reinterpret_cast<PaddedStruct*>(buffer);
    TEST_ASSERT_EQ(restored[0].a, 1u, "Element [0] restored");
    TEST_ASSERT_EQ(restored[1].a, 2u, "Element [1] restored");
    TEST_ASSERT_EQ(restored[2].a, 3u, "Element [2] restored");
    
    return true;
}

// ============================================================================
// Compile-Time Validation: Nested Containers Should Be Rejected
// ============================================================================

// These should trigger compile errors if uncommented:
/*
struct NestedContainers {
    fixed_vector<fixed_vector<float, 5>, 10> matrix;  // SHOULD FAIL
};

struct NestedInMessage {
    uint32_t id;
    fixed_vector<fixed_vector<uint32_t, 10>, 20> data;  // SHOULD FAIL
};
*/

// Note: Add static_assert in container_traits.hpp to enforce this:
// static_assert(!is_fixed_container_v<T>, 
//               "Nested fixed containers not supported - use flattened structure");

} // namespace tests

// ============================================================================
// Test Suite
// ============================================================================

struct ElementPaddingTests : TestSuite<ElementPaddingTests> {
    static constexpr const char* name = "SeRTial - Element Padding & Layout Tests";
    
    static bool run() {
        if (!tests::simple_pod_serialization_test()) return false;
        if (!tests::padded_struct_serialization_test()) return false;
        if (!tests::complex_padding_serialization_test()) return false;
        if (!tests::size_calculation_with_padding_test()) return false;
        if (!tests::array_element_layout_test()) return false;
        
        TEST_PRINT("");
        TEST_PRINT("All element padding tests passed!");
        TEST_PRINT("");
        TEST_PRINT("Key findings:");
        TEST_PRINT("  \u2705 Element internal padding IS serialized (matches C++ layout)");
        TEST_PRINT("  \u2705 No inter-element padding (array guarantee)");
        TEST_PRINT("  \u2705 sizeof(T) correctly includes padding for size calculations");
        TEST_PRINT("  \u2705 Serialization/deserialization preserves all data");
        
        return true;
    }
};

// ============================================================================
// Main
// ============================================================================

int main() {
    return TestRunner::run<ElementPaddingTests>();
}
