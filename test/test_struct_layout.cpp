/// Test: StructLayout - Unified serialization with fixed and variable fields
#include "test_framework.hpp"
#include "../include/sertial/io/unified_binary.hpp"
#include "../include/sertial/core/layout/block_types.hpp"
#include "../include/sertial/containers/fixed_vector.hpp"
#include "../include/sertial/containers/fixed_string.hpp"

using namespace sertial;
using namespace sertial::test;
using namespace sertial::detail;  // For BlockType

// ============================================================================
// Test Structures
// ============================================================================

struct PureFixed {
    uint32_t a;
    uint16_t b;
    uint64_t c;
    float d;
};

struct WithVariable {
    uint32_t id;
    fixed_vector<uint16_t, 10> values;
    uint64_t timestamp;
};

struct MultiVariable {
    fixed_string<32> name;
    uint32_t count;
    fixed_vector<float, 20> data;
    uint16_t flags;
};

// ============================================================================
// Test Functions
// ============================================================================

namespace tests {

bool struct_layout_analysis() {
    TEST_SECTION("Pure fixed-size struct analysis");
    
    using Layout = StructLayout<PureFixed>;
    
    TEST_ASSERT(!Layout::has_variable_fields, "Should have no variable fields");
    TEST_ASSERT_EQ(Layout::base_packed_size, Layout::max_packed_size, "Fixed struct: base == max");
    TEST_ASSERT(Layout::execution_order.size() > 0, "Should have execution blocks");
    
    TEST_PRINT("PureFixed: " << Layout::execution_order.size() << " execution blocks, base size = " << Layout::base_packed_size << " bytes");
    
    return true;
}

bool variable_field_analysis() {
    TEST_SECTION("Struct with variable field analysis");
    
    using Layout = StructLayout<WithVariable>;
    
    TEST_ASSERT(Layout::has_variable_fields, "Should have variable fields");
    TEST_ASSERT(Layout::max_packed_size > Layout::base_packed_size, "Variable struct: max > base");
    
    TEST_PRINT("WithVariable: " << Layout::execution_order.size() << " execution blocks, "
                << "base size = " << Layout::base_packed_size << " bytes, "
                << "max size = " << Layout::max_packed_size << " bytes");
    
    // Test runtime size calculation
    WithVariable obj1{42, {}, 12345};
    obj1.values.push_back(1);
    obj1.values.push_back(2);
    obj1.values.push_back(3);
    
    std::size_t size1 = packed_size_of(obj1);
    // id(4) + length(4) + 3*uint16(6) + timestamp(8) = 22
    std::size_t expected1 = 4 + sizeof(uint32_t) + 3 * sizeof(uint16_t) + 8;
    TEST_ASSERT_EQ(size1, expected1, "Size should include length prefix + 3 uint16_t elements");
    
    WithVariable obj2{99, {}, 67890};
    std::size_t size2 = packed_size_of(obj2);
    // id(4) + length(4) + timestamp(8) = 16
    std::size_t expected2 = 4 + sizeof(uint32_t) + 8;  // Length prefix + timestamp
    TEST_ASSERT_EQ(size2, expected2, "Empty vector should be base size + length prefix");
    
    return true;
}

bool multi_variable_analysis() {
    TEST_SECTION("Struct with multiple variable fields");
    
    using Layout = StructLayout<MultiVariable>;
    
    TEST_ASSERT(Layout::has_variable_fields, "Should have variable fields");
    
    // Count dynamic blocks
    std::size_t dynamic_count = 0;
    for (const auto& block : Layout::execution_order) {
        if (block.type == BlockType::Dynamic) {
            dynamic_count++;
        }
    }
    TEST_ASSERT_EQ(dynamic_count, 2u, "Should have 2 dynamic blocks");
    
    TEST_PRINT("MultiVariable: " << Layout::execution_order.size() << " execution blocks, "
                << dynamic_count << " dynamic blocks, base size = " << Layout::base_packed_size << " bytes");
    
    MultiVariable obj{"test", 5, {}, 0xAB};
    obj.name = "Hello";
    obj.data.push_back(1.0f);
    obj.data.push_back(2.0f);
    
    std::size_t size = packed_size_of(obj);
    // name(4+5) + count(4) + data(4+8) + flags(2) = 9 + 4 + 12 + 2 = 27
    std::size_t expected = sizeof(uint32_t) + 5 * sizeof(char) +  // name with length prefix
                          4 +  // count (runtime offset block)
                          sizeof(uint32_t) + 2 * sizeof(float) +  // data with length prefix
                          2;  // flags (runtime offset block)
    TEST_ASSERT_EQ(size, expected, "Size should include length prefixes + name and data elements");
    
    return true;
}

bool pure_fixed_serialization() {
    TEST_SECTION("Pure fixed serialization round-trip");
    
    PureFixed original{0x12345678, 0xABCD, 0xFEDCBA9876543210, 3.14159f};
    
    auto buffer = serialize(original);
    auto restored_opt = deserialize<PureFixed>(buffer.view());
    
    TEST_ASSERT(restored_opt.has_value(), "Deserialization should succeed");
    auto& restored = *restored_opt;
    
    TEST_ASSERT_EQ(restored.a, original.a, "Field a should match");
    TEST_ASSERT_EQ(restored.b, original.b, "Field b should match");
    TEST_ASSERT_EQ(restored.c, original.c, "Field c should match");
    TEST_ASSERT(std::abs(restored.d - original.d) < 0.0001f, "Field d should match");
    
    return true;
}

bool variable_field_serialization() {
    TEST_SECTION("Variable field serialization");
    
    WithVariable original{42, {}, 12345};
    original.values.push_back(100);
    original.values.push_back(200);
    original.values.push_back(300);
    
    // Serialize
    auto buffer = serialize(original);
    
    // Check size
    std::size_t expected_size = packed_size_of(original);
    TEST_ASSERT_EQ(buffer.size(), expected_size, "Buffer size should match calculated size");
    
    // Deserialize
    auto restored_opt = deserialize<WithVariable>(buffer.view());
    
    TEST_ASSERT(restored_opt.has_value(), "Deserialization should succeed");
    auto& restored = *restored_opt;
    
    TEST_ASSERT_EQ(restored.id, original.id, "Field id should match");
    TEST_ASSERT_EQ(restored.timestamp, original.timestamp, "Field timestamp should match");
    TEST_ASSERT_EQ(restored.values.size(), original.values.size(), "Vector size should match");
    
    for (std::size_t i = 0; i < original.values.size(); ++i) {
        TEST_ASSERT_EQ(restored.values[i], original.values[i], "Vector element should match");
    }
    
    TEST_PRINT("Serialized " << buffer.size() << " bytes, round-trip successful");
    
    return true;
}

bool multi_variable_serialization() {
    TEST_SECTION("Multiple variable fields serialization");
    
    MultiVariable original{"Hello", 5, {}, 0xABCD};
    original.data.push_back(1.5f);
    original.data.push_back(2.5f);
    original.data.push_back(3.5f);
    
    // Serialize
    auto buffer = serialize(original);
    
    // Deserialize
    auto restored_opt = deserialize<MultiVariable>(buffer.view());
    
    TEST_ASSERT(restored_opt.has_value(), "Deserialization should succeed");
    auto& restored = *restored_opt;
    
    TEST_ASSERT_EQ(restored.count, original.count, "Field count should match");
    TEST_ASSERT_EQ(restored.flags, original.flags, "Field flags should match");
    
    // Check string
    TEST_ASSERT_EQ(restored.name.size(), original.name.size(), "String size should match");
    TEST_ASSERT_EQ(std::string(restored.name.data()), std::string(original.name.data()), "String content should match");
    
    // Check vector
    TEST_ASSERT_EQ(restored.data.size(), original.data.size(), "Vector size should match");
    for (std::size_t i = 0; i < original.data.size(); ++i) {
        TEST_ASSERT(std::abs(restored.data[i] - original.data[i]) < 0.0001f, "Vector element should match");
    }
    
    TEST_PRINT("Multi-variable round-trip successful");
    
    return true;
}

bool execution_block_analysis() {
    TEST_SECTION("Execution block analysis");
    
    using Layout = StructLayout<PureFixed>;
    
    TEST_PRINT("Execution blocks for PureFixed:");
    for (std::size_t i = 0; i < Layout::execution_order.size(); ++i) {
        const auto& block = Layout::execution_order[i];
        std::string type_str;
        switch(block.type) {
            case BlockType::Fixed: type_str = "Fixed"; break;
            case BlockType::Padding: type_str = "Padding"; break;
            case BlockType::Dynamic: type_str = "Dynamic"; break;
            case BlockType::RuntimeOffset: type_str = "RuntimeOffset"; break;
        }
        TEST_PRINT("  Block " << i << ": type=" << type_str << " index=" << block.index);
    }
    
    TEST_ASSERT(Layout::execution_order.size() > 0, "Should have at least one block");
    
    // Verify struct layout properties
    TEST_PRINT("\nLayout properties:");
    TEST_PRINT("  sizeof(PureFixed): " << sizeof(PureFixed));
    TEST_PRINT("  base_packed_size: " << Layout::base_packed_size);
    TEST_PRINT("  max_packed_size: " << Layout::max_packed_size);
    TEST_PRINT("  num_fields: " << Layout::num_fields);
    TEST_PRINT("  has_variable_fields: " << (Layout::has_variable_fields ? "yes" : "no"));
    
    return true;
}

} // namespace tests

// ============================================================================
// Test Suite
// ============================================================================

struct StructLayoutTests : TestSuite<StructLayoutTests> {
    static constexpr const char* name = "StructLayout & Binary Serialization";
    
    static bool run() {
        bool success = true;
        success &= tests::struct_layout_analysis();
        success &= tests::variable_field_analysis();
        success &= tests::multi_variable_analysis();
        success &= tests::pure_fixed_serialization();
        success &= tests::variable_field_serialization();
        success &= tests::multi_variable_serialization();
        success &= tests::execution_block_analysis();
        return success;
    }
};

int main() {
    return TestRunner::run<StructLayoutTests>();
}
