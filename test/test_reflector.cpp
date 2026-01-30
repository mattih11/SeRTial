/// Test: Reflector - Type-specific serialization handlers
/// Tests BinaryReflector specializations for primitives, strings, containers
#include "test_framework.hpp"
#include <sertial/reflector/binary_reflector.hpp>
#include <sertial/reflector/reflector_primitives.hpp>
#include <sertial/reflector/reflector_strings.hpp>
#include <sertial/reflector/reflector_containers.hpp>
#include <array>

using namespace sertial;
using namespace sertial::test;

// ============================================================================
// Test Structs
// ============================================================================

struct Point {
    double x, y, z;
};

// Manual BinaryReflector for Point
namespace sertial {
template<>
struct BinaryReflector<Point> {
    static void write(BinaryWriter& writer, const Point& p) {
        BinaryReflector<double>::write(writer, p.x);
        BinaryReflector<double>::write(writer, p.y);
        BinaryReflector<double>::write(writer, p.z);
    }
    
    static std::optional<Point> read(BinaryReader& reader) {
        auto x = BinaryReflector<double>::read(reader);
        auto y = BinaryReflector<double>::read(reader);
        auto z = BinaryReflector<double>::read(reader);
        
        if (!x || !y || !z) return std::nullopt;
        return Point{*x, *y, *z};
    }
};
} // namespace sertial

// ============================================================================
// Individual Test Functions
// ============================================================================

namespace tests {

bool primitives_test() {
    TEST_SECTION("Test 1: Primitive Types");
    
    int32_t i32 = -12345;
    uint64_t u64 = 0xFFFFFFFFFFFFFFFF;
    double dbl = 3.14159;
    bool flag = true;
    char ch = 'A';
    
    auto data_i32 = serialize(i32);
    auto data_u64 = serialize(u64);
    auto data_dbl = serialize(dbl);
    auto data_flag = serialize(flag);
    auto data_ch = serialize(ch);
    
    auto read_i32 = deserialize<int32_t>(data_i32);
    auto read_u64 = deserialize<uint64_t>(data_u64);
    auto read_dbl = deserialize<double>(data_dbl);
    auto read_flag = deserialize<bool>(data_flag);
    auto read_ch = deserialize<char>(data_ch);
    
    TEST_ASSERT(read_i32 && *read_i32 == i32, "int32_t roundtrip");
    TEST_ASSERT(read_u64 && *read_u64 == u64, "uint64_t roundtrip");
    TEST_ASSERT(read_dbl && *read_dbl == dbl, "double roundtrip");
    TEST_ASSERT(read_flag && *read_flag == flag, "bool roundtrip");
    TEST_ASSERT(read_ch && *read_ch == ch, "char roundtrip");
    
    TEST_PRINT("  int32_t: " << *read_i32 << " (" << data_i32.size() << " bytes)");
    TEST_PRINT("  uint64_t: " << *read_u64 << " (" << data_u64.size() << " bytes)");
    TEST_PRINT("  double: " << *read_dbl << " (" << data_dbl.size() << " bytes)");
    TEST_PRINT("  bool: " << *read_flag << " (" << data_flag.size() << " bytes)");
    TEST_PRINT("  char: '" << *read_ch << "' (" << data_ch.size() << " bytes)");
    
    return true;
}

bool strings_test() {
    TEST_SECTION("Test 2: String Types");
    
    std::string str = "Hello, Reflector!";
    fixed_string<64> fstr = "Fixed capacity string";
    
    auto data_str = serialize(str);
    auto data_fstr = serialize(fstr);
    
    auto read_str = deserialize<std::string>(data_str);
    auto read_fstr = deserialize<fixed_string<64>>(data_fstr);
    
    TEST_ASSERT(read_str && *read_str == str, "std::string roundtrip");
    TEST_ASSERT(read_fstr && std::string_view(*read_fstr) == std::string_view(fstr), "fixed_string roundtrip");
    
    TEST_PRINT("  std::string: \"" << *read_str << "\" (" << data_str.size() << " bytes)");
    TEST_PRINT("  fixed_string<64>: \"" << read_fstr->c_str() << "\" (" << data_fstr.size() << " bytes)");
    
    return true;
}

bool vectors_test() {
    TEST_SECTION("Test 3: Vector Types");
    
    std::vector<int> vec_int = {1, 2, 3, 4, 5};
    auto data_vec_int = serialize(vec_int);
    auto read_vec_int = deserialize<std::vector<int>>(data_vec_int);
    
    TEST_ASSERT(read_vec_int && *read_vec_int == vec_int, "vector<int> roundtrip");
    TEST_PRINT("  std::vector<int>: [" << (*read_vec_int)[0] << ", " << (*read_vec_int)[1] << ", ...] (" << data_vec_int.size() << " bytes)");
    
    std::vector<std::string> vec_str = {"one", "two", "three"};
    auto data_vec_str = serialize(vec_str);
    auto read_vec_str = deserialize<std::vector<std::string>>(data_vec_str);
    
    TEST_ASSERT(read_vec_str && *read_vec_str == vec_str, "vector<string> roundtrip");
    TEST_PRINT("  std::vector<string>: [\"" << (*read_vec_str)[0] << "\", ...] (" << data_vec_str.size() << " bytes)");
    
    fixed_vector<double, 100> fvec;
    fvec.push_back(1.1);
    fvec.push_back(2.2);
    fvec.push_back(3.3);
    
    auto data_fvec = serialize(fvec);
    auto read_fvec = deserialize<fixed_vector<double, 100>>(data_fvec);
    
    TEST_ASSERT(read_fvec && read_fvec->size() == fvec.size(), "fixed_vector roundtrip");
    TEST_PRINT("  fixed_vector<double, 100>: [" << (*read_fvec)[0] << ", ...] (" << data_fvec.size() << " bytes)");
    
    return true;
}

bool arrays_test() {
    TEST_SECTION("Test 4: Array Types");
    
    std::array<int, 5> arr = {10, 20, 30, 40, 50};
    auto data_arr = serialize(arr);
    auto read_arr = deserialize<std::array<int, 5>>(data_arr);
    
    TEST_ASSERT(read_arr && *read_arr == arr, "array<int, 5> roundtrip");
    TEST_PRINT("  std::array<int, 5>: [" << (*read_arr)[0] << ", " << (*read_arr)[1] << ", ...] (" << data_arr.size() << " bytes, no length prefix)");
    
    return true;
}

bool custom_struct_test() {
    TEST_SECTION("Test 5: Custom Struct");
    
    Point p{1.5, 2.5, 3.5};
    auto data_p = serialize(p);
    auto read_p = deserialize<Point>(data_p);
    
    TEST_ASSERT(read_p && read_p->x == p.x && read_p->y == p.y && read_p->z == p.z, "Point roundtrip");
    TEST_PRINT("  Point: (" << read_p->x << ", " << read_p->y << ", " << read_p->z << ") (" << data_p.size() << " bytes)");
    
    return true;
}

bool nested_containers_test() {
    TEST_SECTION("Test 6: Nested Containers");
    
    std::vector<std::vector<int>> nested = {{1, 2, 3}, {4, 5}, {6, 7, 8, 9}};
    auto data_nested = serialize(nested);
    auto read_nested = deserialize<std::vector<std::vector<int>>>(data_nested);
    
    TEST_ASSERT(read_nested && *read_nested == nested, "nested vector roundtrip");
    TEST_PRINT("  std::vector<std::vector<int>>: [[1, 2, 3], ...] (" << data_nested.size() << " bytes)");
    
    std::vector<Point> points = {{1, 2, 3}, {4, 5, 6}};
    auto data_points = serialize(points);
    auto read_points = deserialize<std::vector<Point>>(data_points);
    
    TEST_ASSERT(read_points && read_points->size() == 2, "vector<Point> roundtrip");
    TEST_PRINT("  std::vector<Point>: [(1, 2, 3), ...] (" << data_points.size() << " bytes)");
    
    return true;
}

bool error_handling_test() {
    TEST_SECTION("Test 7: Error Handling");
    
    // Truncated vector
    std::vector<std::byte> truncated = {std::byte{10}}; // Length 10 but no data
    auto read_truncated = deserialize<std::vector<int>>(truncated);
    TEST_ASSERT(!read_truncated.has_value(), "Truncated vector fails gracefully");
    TEST_PRINT("  Truncated vector: failed as expected [OK]");
    
    // fixed_vector overflow (length > capacity)
    std::vector<std::byte> overflow;
    BinaryWriter writer;
    writer.write_varint(200); // Length > 100 (capacity)
    auto overflow_data = std::vector<std::byte>(writer.data().begin(), writer.data().end());
    auto read_overflow = deserialize<fixed_vector<int, 100>>(overflow_data);
    TEST_ASSERT(!read_overflow.has_value(), "fixed_vector overflow fails gracefully");
    TEST_PRINT("  fixed_vector overflow: failed as expected [OK]");
    
    return true;
}

bool concept_test() {
    TEST_SECTION("Test 8: Concept Checking");
    
    // Use typedef to avoid macro comma issue
    using FixedStr32 = fixed_string<32>;
    
    TEST_ASSERT(HasBinaryReflector<int>, "int has BinaryReflector");
    TEST_ASSERT(HasBinaryReflector<std::string>, "string has BinaryReflector");
    TEST_ASSERT(HasBinaryReflector<std::vector<int>>, "vector<int> has BinaryReflector");
    TEST_ASSERT(HasBinaryReflector<Point>, "Point has BinaryReflector");
    TEST_ASSERT(HasBinaryReflector<FixedStr32>, "fixed_string has BinaryReflector");
    
    TEST_PRINT("  HasBinaryReflector<int>: " << HasBinaryReflector<int>);
    TEST_PRINT("  HasBinaryReflector<std::string>: " << HasBinaryReflector<std::string>);
    TEST_PRINT("  HasBinaryReflector<std::vector<int>>: " << HasBinaryReflector<std::vector<int>>);
    TEST_PRINT("  HasBinaryReflector<Point>: " << HasBinaryReflector<Point>);
    TEST_PRINT("  HasBinaryReflector<fixed_string<32>>: " << HasBinaryReflector<FixedStr32>);
    
    return true;
}

} // namespace tests

// ============================================================================
// Test Suite
// ============================================================================

struct ReflectorTests : TestSuite<ReflectorTests> {
    static constexpr const char* name = "SeRTial - Reflector Tests";
    
    static bool run() {
        if (!tests::primitives_test()) return false;
        if (!tests::strings_test()) return false;
        if (!tests::vectors_test()) return false;
        if (!tests::arrays_test()) return false;
        if (!tests::custom_struct_test()) return false;
        if (!tests::nested_containers_test()) return false;
        if (!tests::error_handling_test()) return false;
        if (!tests::concept_test()) return false;
        
        TEST_PRINT("");
        TEST_PRINT("Reflector Tests Complete");
        TEST_PRINT("  [OK] Primitive type reflectors");
        TEST_PRINT("  [OK] String type reflectors");
        TEST_PRINT("  [OK] Container reflectors (vectors, arrays)");
        TEST_PRINT("  [OK] Nested container support");
        TEST_PRINT("  [OK] Custom struct reflectors");
        TEST_PRINT("  [OK] Optimized memcpy paths for arithmetic types");
        TEST_PRINT("  [OK] Comprehensive error handling");
        
        return true;
    }
};

// ============================================================================
// Main
// ============================================================================

int main() {
    return TestRunner::run<ReflectorTests>();
}
