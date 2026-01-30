/// Test: Binary I/O - Low-level serialization primitives
/// Tests BinaryWriter, BinaryReader, and varint encoding
#include "test_framework.hpp"
#include <sertial/io/varint.hpp>
#include <sertial/io/binary_writer.hpp>
#include <sertial/io/binary_reader.hpp>
#include <sertial/containers/fixed_vector.hpp>
#include <sertial/containers/fixed_string.hpp>

using namespace sertial;
using namespace sertial::test;

// ============================================================================
// Individual Test Functions
// ============================================================================

namespace tests {

bool varint_test() {
    TEST_SECTION("Test 1: Varint Encoding/Decoding");
    
    // Test unsigned varints
    std::vector<uint64_t> test_values = {0, 1, 127, 128, 255, 256, 65535, 65536, 0xFFFFFFFF, 0xFFFFFFFFFFFFFFFF};
    
    for (uint64_t value : test_values) {
        std::byte buffer[10];
        std::size_t encoded_size = encode_varint(value, buffer);
        std::size_t expected_size = varint_size(value);
        
        TEST_ASSERT_EQ(encoded_size, expected_size, "Encoded size matches expected");
        
        auto [decoded, bytes] = decode_varint_unsafe(buffer);
        TEST_ASSERT_EQ(decoded, value, "Decoded value matches");
        TEST_ASSERT_EQ(bytes, encoded_size, "Decoded bytes matches");
        
        TEST_PRINT(value << " -> " << encoded_size << " bytes");
    }
    
    // Test signed varints (zigzag)
    std::vector<int64_t> signed_values = {0, -1, 1, -2, 2, -127, 127, -128, 128, -1000, 1000};
    
    TEST_PRINT("");
    TEST_PRINT("Signed values (zigzag):");
    for (int64_t value : signed_values) {
        std::byte buffer[10];
        std::size_t encoded_size = encode_varint(value, buffer);
        
        auto [decoded, bytes] = decode_varint_signed<int64_t>(std::span{buffer, encoded_size});
        TEST_ASSERT_EQ(decoded, value, "Signed decoded value matches");
        TEST_ASSERT_EQ(bytes, encoded_size, "Signed decoded bytes matches");
        
        TEST_PRINT("  " << value << " -> " << encoded_size << " bytes");
    }
    
    return true;
}

bool primitives_test() {
    TEST_SECTION("Test 2: Primitive Types");
    
    BinaryWriter writer;
    
    writer.write(static_cast<uint8_t>(42));
    writer.write(static_cast<int32_t>(-12345));
    writer.write(3.14159);
    writer.write(true);
    writer.write(false);
    
    TEST_PRINT("Written " << writer.size() << " bytes");
    
    BinaryReader reader(writer.data());
    
    auto u8 = reader.read<uint8_t>();
    auto i32 = reader.read<int32_t>();
    auto dbl = reader.read<double>();
    auto b1 = reader.read_bool();
    auto b2 = reader.read_bool();
    
    TEST_ASSERT(u8.has_value() && *u8 == 42, "uint8_t roundtrip");
    TEST_ASSERT(i32.has_value() && *i32 == -12345, "int32_t roundtrip");
    TEST_ASSERT(dbl.has_value() && *dbl == 3.14159, "double roundtrip");
    TEST_ASSERT(b1.has_value() && *b1 == true, "bool true roundtrip");
    TEST_ASSERT(b2.has_value() && *b2 == false, "bool false roundtrip");
    
    TEST_PRINT("  uint8_t: " << (int)*u8);
    TEST_PRINT("  int32_t: " << *i32);
    TEST_PRINT("  double: " << *dbl);
    TEST_PRINT("  bool: " << *b1 << ", " << *b2);
    TEST_PRINT("  Position: " << reader.position() << "/" << reader.size());
    
    return true;
}

bool strings_test() {
    TEST_SECTION("Test 3: Strings");
    
    BinaryWriter writer;
    
    std::string str1 = "Hello, SeRTial!";
    std::string str2 = "Binary serialization";
    writer.write(str1);
    writer.write(str2);
    
    TEST_PRINT("Written " << writer.size() << " bytes");
    
    BinaryReader reader(writer.data());
    
    auto read1 = reader.read_string();
    auto read2 = reader.read_string_view();
    
    TEST_ASSERT(read1.has_value() && *read1 == str1, "string 1 roundtrip");
    TEST_ASSERT(read2.has_value() && *read2 == str2, "string 2 roundtrip (zero-copy)");
    
    TEST_PRINT("  String 1: \"" << *read1 << "\"");
    TEST_PRINT("  String 2: \"" << *read2 << "\" (zero-copy)");
    TEST_PRINT("  Position: " << reader.position() << "/" << reader.size());
    
    return true;
}

bool fixed_strings_test() {
    TEST_SECTION("Test 4: Fixed Strings");
    
    BinaryWriter writer;
    
    fixed_string<32> fs1 = "Stack allocated!";
    fixed_string<64> fs2 = "No heap allocations";
    
    writer.write(fs1);
    writer.write(fs2);
    
    TEST_PRINT("Written " << writer.size() << " bytes");
    
    BinaryReader reader(writer.data());
    
    auto read1 = reader.read_fixed_string<32>();
    auto read2 = reader.read_fixed_string<64>();
    
    TEST_ASSERT(read1.has_value() && std::string_view(*read1) == std::string_view(fs1), "fixed_string<32> roundtrip");
    TEST_ASSERT(read2.has_value() && std::string_view(*read2) == std::string_view(fs2), "fixed_string<64> roundtrip");
    
    TEST_PRINT("  fixed_string<32>: \"" << read1->c_str() << "\"");
    TEST_PRINT("  fixed_string<64>: \"" << read2->c_str() << "\"");
    TEST_PRINT("  Position: " << reader.position() << "/" << reader.size());
    
    return true;
}

bool vectors_test() {
    TEST_SECTION("Test 5: Dynamic Vectors");
    
    BinaryWriter writer;
    
    std::vector<int> vec1 = {1, 2, 3, 4, 5};
    std::vector<double> vec2 = {3.14, 2.71, 1.41};
    
    writer.write(vec1);
    writer.write(vec2);
    
    TEST_PRINT("Written " << writer.size() << " bytes");
    
    BinaryReader reader(writer.data());
    
    auto read1 = reader.read_vector<int>();
    auto read2 = reader.read_vector<double>();
    
    TEST_ASSERT(read1.has_value() && *read1 == vec1, "vector<int> roundtrip");
    TEST_ASSERT(read2.has_value() && *read2 == vec2, "vector<double> roundtrip");
    
    TEST_PRINT("  vec<int>: " << (*read1)[0] << " " << (*read1)[1] << " " << (*read1)[2] << " " << (*read1)[3] << " " << (*read1)[4]);
    TEST_PRINT("  vec<double>: " << (*read2)[0] << " " << (*read2)[1] << " " << (*read2)[2]);
    TEST_PRINT("  Position: " << reader.position() << "/" << reader.size());
    
    return true;
}

bool fixed_vectors_test() {
    TEST_SECTION("Test 6: Fixed Vectors");
    
    BinaryWriter writer;
    
    fixed_vector<int, 10> fv1;
    fv1.push_back(10);
    fv1.push_back(20);
    fv1.push_back(30);
    
    fixed_vector<double, 100> fv2;
    fv2.push_back(1.1);
    fv2.push_back(2.2);
    
    writer.write(fv1);
    writer.write(fv2);
    
    TEST_PRINT("Written " << writer.size() << " bytes");
    
    BinaryReader reader(writer.data());
    
    auto read1 = reader.read_fixed_vector<int, 10>();
    auto read2 = reader.read_fixed_vector<double, 100>();
    
    TEST_ASSERT(read1.has_value() && read1->size() == 3, "fixed_vector<int> roundtrip");
    TEST_ASSERT(read2.has_value() && read2->size() == 2, "fixed_vector<double> roundtrip");
    
    TEST_PRINT("  fixed_vector<int, 10>: " << (*read1)[0] << " " << (*read1)[1] << " " << (*read1)[2]);
    TEST_PRINT("  fixed_vector<double, 100>: " << (*read2)[0] << " " << (*read2)[1]);
    TEST_PRINT("  Position: " << reader.position() << "/" << reader.size());
    
    return true;
}

bool mixed_data_test() {
    TEST_SECTION("Test 7: Mixed Data Structures");
    
    BinaryWriter writer;
    
    writer.write(static_cast<uint32_t>(12345));
    writer.write(std::string("Username"));
    writer.write(static_cast<int16_t>(200));
    
    std::vector<int> items = {1, 2, 3};  // Use int instead of string for simpler testing
    writer.write(items);
    writer.write(3.14159);
    
    TEST_PRINT("Written " << writer.size() << " bytes");
    
    BinaryReader reader(writer.data());
    
    auto id = reader.read<uint32_t>();
    auto username = reader.read_string();
    auto score = reader.read<int16_t>();
    auto read_items = reader.read_vector<int>();
    auto pi = reader.read<double>();
    
    TEST_ASSERT(id.has_value() && *id == 12345, "ID roundtrip");
    TEST_ASSERT(username.has_value() && *username == "Username", "Username roundtrip");
    TEST_ASSERT(score.has_value() && *score == 200, "Score roundtrip");
    TEST_ASSERT(read_items.has_value() && read_items->size() == 3, "Items roundtrip");
    TEST_ASSERT(pi.has_value() && *pi == 3.14159, "Pi roundtrip");
    
    TEST_PRINT("  ID: " << *id);
    TEST_PRINT("  Username: " << *username);
    TEST_PRINT("  Score: " << *score);
    TEST_PRINT("  Items: " << (*read_items)[0] << " " << (*read_items)[1] << " " << (*read_items)[2]);
    TEST_PRINT("  Pi: " << *pi);
    TEST_PRINT("  Position: " << reader.position() << "/" << reader.size());
    
    return true;
}

bool error_handling_test() {
    TEST_SECTION("Test 8: Error Handling");
    
    // Reading past end
    std::vector<std::byte> small_data = {std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    BinaryReader reader(small_data);
    
    auto val = reader.read<uint32_t>();
    TEST_ASSERT(val.has_value(), "Can read 4 bytes");
    TEST_PRINT("  Read uint32_t: " << *val);
    
    auto past_end = reader.read<uint32_t>();
    TEST_ASSERT(!past_end.has_value(), "Cannot read past end");
    TEST_PRINT("  Read past end: failed (expected)");
    
    // Truncated string
    std::vector<std::byte> bad_string = {std::byte{10}}; // Length 10 but no data
    BinaryReader reader2(bad_string);
    auto str = reader2.read_string();
    TEST_ASSERT(!str.has_value(), "Truncated string fails gracefully");
    TEST_PRINT("  Truncated string: failed (expected)");
    
    return true;
}

} // namespace tests

// ============================================================================
// Test Suite
// ============================================================================

struct BinaryIOTests : TestSuite<BinaryIOTests> {
    static constexpr const char* name = "SeRTial - Binary I/O Tests";
    
    static bool run() {
        if (!tests::varint_test()) return false;
        if (!tests::primitives_test()) return false;
        if (!tests::strings_test()) return false;
        if (!tests::fixed_strings_test()) return false;
        if (!tests::vectors_test()) return false;
        if (!tests::fixed_vectors_test()) return false;
        if (!tests::mixed_data_test()) return false;
        if (!tests::error_handling_test()) return false;
        
        TEST_PRINT("");
        TEST_PRINT("Binary I/O Tests Complete");
        TEST_PRINT("  [OK] Varint encoding/decoding");
        TEST_PRINT("  [OK] BinaryWriter (primitives, strings, containers)");
        TEST_PRINT("  [OK] BinaryReader (safe, zero-copy where possible)");
        TEST_PRINT("  [OK] Fixed-capacity container serialization");
        TEST_PRINT("  [OK] Error handling and bounds checking");
        
        return true;
    }
};

// ============================================================================
// Main
// ============================================================================

int main() {
    return TestRunner::run<BinaryIOTests>();
}
