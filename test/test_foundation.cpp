/// Test: Foundation - Core containers and type traits
/// Tests fixed_vector, fixed_string, container traits, and type analysis
#include "test_framework.hpp"
#include <sertial/core/concepts.hpp>
#include <sertial/core/traits.hpp>
#include <sertial/containers/fixed_vector.hpp>
#include <sertial/containers/fixed_string.hpp>
#include <sertial/containers/container_traits.hpp>

using namespace sertial;
using namespace sertial::test;

// ============================================================================
// Test Structs
// ============================================================================

struct Point {
    double x, y, z;
};

struct Message {
    uint32_t id;
    fixed_string<64> sender;
    fixed_vector<double, 100> values;
};

// ============================================================================
// Individual Test Functions
// ============================================================================

namespace tests {

bool fixed_vector_test() {
    TEST_SECTION("Test 1: fixed_vector<int, 10>");
    
    fixed_vector<int, 10> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    
    TEST_ASSERT_EQ(vec.size(), 3u, "Size after 3 pushes");
    TEST_ASSERT_EQ(vec.max_size(), 10u, "Max size");
    TEST_ASSERT_EQ(vec[0], 1, "First element");
    TEST_ASSERT_EQ(vec[2], 3, "Last element");
    
    TEST_PRINT("Size: " << vec.size() << "/" << vec.max_size());
    TEST_PRINT("Values: " << vec[0] << " " << vec[1] << " " << vec[2]);
    
    return true;
}

bool fixed_vector_erase_remove_test() {
    TEST_SECTION("Test 1a: fixed_vector erase/remove operations");
    
    // Test erase(iterator)
    fixed_vector<int, 10> vec1 = {1, 2, 3, 4, 5};
    auto it = vec1.erase(vec1.begin() + 2);  // Remove 3
    TEST_ASSERT_EQ(vec1.size(), 4u, "Size after erase");
    TEST_ASSERT_EQ(vec1[0], 1, "Element 0");
    TEST_ASSERT_EQ(vec1[1], 2, "Element 1");
    TEST_ASSERT_EQ(vec1[2], 4, "Element 2 (was 4)");
    TEST_ASSERT_EQ(vec1[3], 5, "Element 3");
    TEST_ASSERT_EQ(*it, 4, "Iterator points to next element");
    
    // Test erase(first, last) - range
    fixed_vector<int, 10> vec2 = {1, 2, 3, 4, 5, 6, 7};
    vec2.erase(vec2.begin() + 2, vec2.begin() + 5);  // Remove 3, 4, 5
    TEST_ASSERT_EQ(vec2.size(), 4u, "Size after range erase");
    TEST_ASSERT_EQ(vec2[0], 1, "First unchanged");
    TEST_ASSERT_EQ(vec2[1], 2, "Second unchanged");
    TEST_ASSERT_EQ(vec2[2], 6, "Third is now 6");
    TEST_ASSERT_EQ(vec2[3], 7, "Fourth is now 7");
    
    // Test remove(value)
    fixed_vector<int, 10> vec3 = {1, 2, 3, 2, 4, 2, 5};
    size_t removed = vec3.remove(2);  // Remove all 2s
    TEST_ASSERT_EQ(removed, 3u, "Removed 3 elements");
    TEST_ASSERT_EQ(vec3.size(), 4u, "Size after remove");
    TEST_ASSERT_EQ(vec3[0], 1, "Element 0");
    TEST_ASSERT_EQ(vec3[1], 3, "Element 1");
    TEST_ASSERT_EQ(vec3[2], 4, "Element 2");
    TEST_ASSERT_EQ(vec3[3], 5, "Element 3");
    
    // Test remove_if(predicate)
    fixed_vector<int, 10> vec4 = {1, 2, 3, 4, 5, 6, 7, 8};
    removed = vec4.remove_if([](int x) { return x % 2 == 0; });  // Remove evens
    TEST_ASSERT_EQ(removed, 4u, "Removed 4 even numbers");
    TEST_ASSERT_EQ(vec4.size(), 4u, "Size after remove_if");
    TEST_ASSERT_EQ(vec4[0], 1, "Odd 1");
    TEST_ASSERT_EQ(vec4[1], 3, "Odd 3");
    TEST_ASSERT_EQ(vec4[2], 5, "Odd 5");
    TEST_ASSERT_EQ(vec4[3], 7, "Odd 7");
    
    // Test with strings (non-trivial types)
    fixed_vector<std::string, 10> vec5 = {"apple", "banana", "cherry", "apple", "date"};
    removed = vec5.remove("apple");
    TEST_ASSERT_EQ(removed, 2u, "Removed 2 apples");
    TEST_ASSERT_EQ(vec5.size(), 3u, "Size after string remove");
    TEST_ASSERT_EQ(vec5[0], "banana", "String 0");
    TEST_ASSERT_EQ(vec5[1], "cherry", "String 1");
    TEST_ASSERT_EQ(vec5[2], "date", "String 2");
    
    TEST_PRINT("Erase/Remove tests:");
    TEST_PRINT("  - Single element erase: OK");
    TEST_PRINT("  - Range erase: OK");
    TEST_PRINT("  - Remove by value: OK");
    TEST_PRINT("  - Remove by predicate: OK");
    TEST_PRINT("  - Non-trivial types: OK");
    
    return true;
}

bool fixed_string_test() {
    TEST_SECTION("Test 2: fixed_string<32>");
    
    fixed_string<32> str = "Hello, SeRTial!";
    
    TEST_ASSERT_EQ(str.size(), 15u, "Initial size");
    TEST_ASSERT_EQ(str.max_size(), 32u, "Max size");
    TEST_ASSERT(!str.empty(), "Not empty");
    
    str.append(" :)");
    TEST_ASSERT_EQ(str.size(), 18u, "Size after append");
    
    TEST_PRINT("String: \"" << str.c_str() << "\"");
    TEST_PRINT("Size: " << str.size() << "/" << str.max_size());
    
    return true;
}

bool container_traits_test() {
    TEST_SECTION("Test 3: Container Traits");
    
    // Use typedefs to avoid comma issues in macros
    using FixedVecInt10 = fixed_vector<int, 10>;
    using FixedVecDouble100 = fixed_vector<double, 100>;
    using FixedStr32 = fixed_string<32>;
    
    TEST_ASSERT(is_fixed_capacity_v<FixedVecInt10>, "fixed_vector is fixed capacity");
    TEST_ASSERT(!is_fixed_capacity_v<std::vector<int>>, "std::vector is not fixed capacity");
    TEST_ASSERT(is_fixed_capacity_v<FixedStr32>, "fixed_string is fixed capacity");
    
    TEST_PRINT("is_fixed_capacity<fixed_vector<int, 10>>: " << is_fixed_capacity_v<FixedVecInt10>);
    TEST_PRINT("is_fixed_capacity<std::vector<int>>: " << is_fixed_capacity_v<std::vector<int>>);
    TEST_PRINT("is_fixed_capacity<fixed_string<32>>: " << is_fixed_capacity_v<FixedStr32>);
    
    return true;
}

bool type_traits_test() {
    TEST_SECTION("Test 4: TypeTraits");
    
    // Use typedefs to avoid comma issues in macros
    using FixedVecDouble100 = fixed_vector<double, 100>;
    
    // int
    TEST_ASSERT_EQ((int)TypeTraits<int>::category, 0, "int is Static");
    TEST_ASSERT_EQ(TypeTraits<int>::packed_size, 4u, "int packed size");
    TEST_ASSERT(TypeTraits<int>::can_memcpy_whole, "int can memcpy");
    
    TEST_PRINT("int:");
    TEST_PRINT("  category: " << (int)TypeTraits<int>::category << " (0=Static, 1=Dynamic, 2=Trailing)");
    TEST_PRINT("  packed_size: " << TypeTraits<int>::packed_size);
    TEST_PRINT("  can_memcpy_whole: " << TypeTraits<int>::can_memcpy_whole);
    
    // Point
    TEST_ASSERT_EQ(sizeof(Point), 24u, "Point sizeof");
    TEST_ASSERT_EQ(TypeTraits<Point>::packed_size, 24u, "Point packed size");
    TEST_ASSERT(!TypeTraits<Point>::has_padding, "Point has no padding");
    
    TEST_PRINT("Point:");
    TEST_PRINT("  sizeof: " << sizeof(Point));
    TEST_PRINT("  packed_size: " << TypeTraits<Point>::packed_size);
    TEST_PRINT("  has_padding: " << TypeTraits<Point>::has_padding);
    
    // fixed_vector
    TEST_ASSERT_EQ((int)TypeTraits<FixedVecDouble100>::category, 1, "fixed_vector is Dynamic");
    
    TEST_PRINT("fixed_vector<double, 100>:");
    TEST_PRINT("  category: " << (int)TypeTraits<FixedVecDouble100>::category);
    TEST_PRINT("  is_fixed_capacity: " << is_fixed_capacity_v<FixedVecDouble100>);
    
    // std::string
    TEST_ASSERT_EQ((int)TypeTraits<std::string>::category, 1, "string is Dynamic");
    
    TEST_PRINT("std::string:");
    TEST_PRINT("  category: " << (int)TypeTraits<std::string>::category);
    
    return true;
}

bool message_struct_test() {
    TEST_SECTION("Test 5: Message struct");
    
    Message msg;
    msg.id = 42;
    msg.sender = "Alice";
    msg.values.push_back(3.14);
    msg.values.push_back(2.71);
    msg.values.push_back(1.41);
    
    TEST_ASSERT_EQ(msg.id, 42u, "Message ID");
    TEST_ASSERT_EQ(msg.values.size(), 3u, "Values count");
    
    TEST_PRINT("Message ID: " << msg.id);
    TEST_PRINT("Sender: " << msg.sender.c_str());
    TEST_PRINT("Values count: " << msg.values.size());
    TEST_PRINT("Message struct size: " << sizeof(Message) << " bytes");
    TEST_PRINT("  - Stack allocated (zero heap allocations!)");
    
    return true;
}

} // namespace tests

// ============================================================================
// Test Suite
// ============================================================================

struct FoundationTests : TestSuite<FoundationTests> {
    static constexpr const char* name = "SeRTial - Foundation Tests";
    
    static bool run() {
        if (!tests::fixed_vector_test()) return false;
        if (!tests::fixed_vector_erase_remove_test()) return false;
        if (!tests::fixed_string_test()) return false;
        if (!tests::container_traits_test()) return false;
        if (!tests::type_traits_test()) return false;
        if (!tests::message_struct_test()) return false;
        
        TEST_PRINT("");
        TEST_PRINT("Foundation Tests Complete");
        TEST_PRINT("  [OK] Core concepts and traits");
        TEST_PRINT("  [OK] Fixed-capacity containers");
        TEST_PRINT("  [OK] Container type detection");
        
        return true;
    }
};

// ============================================================================
// Main
// ============================================================================

int main() {
    return TestRunner::run<FoundationTests>();
}
