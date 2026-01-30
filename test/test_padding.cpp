/// Padding Analysis Tests
#include "test_framework.hpp"
#include <sertial/core/traits.hpp>

using namespace sertial;
using namespace sertial::test;

// ============================================================================
// Test Structs with Known Padding Patterns
// ============================================================================

// No padding: all fields naturally aligned
struct NoPadding {
    int32_t a;
    int32_t b;
    int32_t c;
};
// sizeof = 12, packed = 12

// Has padding: char followed by int
struct HasPadding1 {
    char a;      // 1 byte + 3 padding
    int32_t b;   // 4 bytes
};
// sizeof = 8, packed = 5

// Has trailing padding
struct HasPadding2 {
    int32_t a;   // 4 bytes
    char b;      // 1 byte + 3 trailing padding
};
// sizeof = 8, packed = 5

// Complex padding
struct HasPadding3 {
    char a;      // 1 byte + 7 padding
    double b;    // 8 bytes
    char c;      // 1 byte + 7 trailing padding
};
// sizeof = 24, packed = 10

// Mixed - no padding due to ordering
struct OptimalLayout {
    double a;    // 8 bytes
    int32_t b;   // 4 bytes
    int32_t c;   // 4 bytes
};
// sizeof = 16, packed = 16

// Nested structs
struct Inner {
    int32_t x;
    int32_t y;
};

struct Outer {
    char tag;     // 1 + 3 padding
    Inner inner;  // 8 bytes
};
// sizeof = 12, packed = 9

// ============================================================================
// Individual Test Functions
// ============================================================================

namespace tests {

template<typename T>
bool test_padding_struct(const char* name, std::size_t expected_sizeof, 
                         std::size_t expected_packed, bool expected_has_padding) {
    TEST_SECTION(name);
    
    constexpr auto actual = sizeof(T);
    constexpr auto packed = TypeTraits<T>::packed_size;
    constexpr auto has_padding = TypeTraits<T>::has_padding;
    
    TEST_PRINT("  sizeof:     " << actual << " bytes");
    TEST_PRINT("  packed:     " << packed << " bytes");
    TEST_PRINT("  has_padding: " << (has_padding ? "true" : "false"));
    
    TEST_ASSERT_EQ(actual, expected_sizeof, "sizeof matches expected");
    TEST_ASSERT_EQ(packed, expected_packed, "packed matches expected");
    TEST_ASSERT_EQ(has_padding, expected_has_padding, "has_padding matches expected");
    
    return true;
}

bool no_padding_test() {
    return test_padding_struct<NoPadding>("Test 1: NoPadding struct", 12, 12, false);
}

bool has_padding1_test() {
    return test_padding_struct<HasPadding1>("Test 2: HasPadding1 (char + int)", 8, 5, true);
}

bool has_padding2_test() {
    return test_padding_struct<HasPadding2>("Test 3: HasPadding2 (int + char)", 8, 5, true);
}

bool has_padding3_test() {
    return test_padding_struct<HasPadding3>("Test 4: HasPadding3 (char + double + char)", 24, 10, true);
}

bool optimal_layout_test() {
    return test_padding_struct<OptimalLayout>("Test 5: OptimalLayout (double + 2x int)", 16, 16, false);
}

bool nested_test() {
    return test_padding_struct<Outer>("Test 6: Outer (char + Inner)", 12, 9, true);
}

bool primitives_test() {
    TEST_SECTION("Test 7: Primitives");
    
    TEST_ASSERT(!TypeTraits<int>::has_padding, "int has no padding");
    TEST_ASSERT(!TypeTraits<double>::has_padding, "double has no padding");
    TEST_ASSERT(!TypeTraits<char>::has_padding, "char has no padding");
    
    TEST_PRINT("  int has_padding:    " << TypeTraits<int>::has_padding);
    TEST_PRINT("  double has_padding: " << TypeTraits<double>::has_padding);
    TEST_PRINT("  char has_padding:   " << TypeTraits<char>::has_padding);
    
    return true;
}

bool memcpy_optimization_test() {
    TEST_SECTION("Test 8: can_memcpy_whole optimization flag");
    
    // Structs without padding can potentially use memcpy for whole struct
    // (though this depends on additional factors like nested types)
    TEST_PRINT("  NoPadding can_memcpy:   " << TypeTraits<NoPadding>::can_memcpy_whole);
    TEST_PRINT("  HasPadding1 can_memcpy: " << TypeTraits<HasPadding1>::can_memcpy_whole);
    TEST_PRINT("  int can_memcpy:         " << TypeTraits<int>::can_memcpy_whole);
    
    // Primitives can always be memcpy'd
    TEST_ASSERT(TypeTraits<int>::can_memcpy_whole, "int can memcpy whole");
    
    return true;
}

} // namespace tests

// ============================================================================
// Test Suite
// ============================================================================

struct PaddingTests : TestSuite<PaddingTests> {
    static constexpr const char* name = "SeRTial - Padding Analysis Tests";
    
    static bool run() {
        if (!tests::no_padding_test()) return false;
        if (!tests::has_padding1_test()) return false;
        if (!tests::has_padding2_test()) return false;
        if (!tests::has_padding3_test()) return false;
        if (!tests::optimal_layout_test()) return false;
        if (!tests::nested_test()) return false;
        if (!tests::primitives_test()) return false;
        if (!tests::memcpy_optimization_test()) return false;
        
        TEST_PRINT("");
        TEST_PRINT("All padding tests passed!");
        
        return true;
    }
};

// ============================================================================
// Main
// ============================================================================

int main() {
    return TestRunner::run<PaddingTests>();
}
