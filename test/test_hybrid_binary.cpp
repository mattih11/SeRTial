/// Test: Hybrid Binary - Unified serialization with fixed and variable fields
#include "test_framework.hpp"
#include "../include/sertial/io/unified_binary.hpp"
#include "../include/sertial/containers/fixed_vector.hpp"
#include "../include/sertial/containers/fixed_string.hpp"

using namespace sertial;
using namespace sertial::test;

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

bool hybrid_memory_map_analysis() {
    TEST_SECTION("Pure fixed-size struct analysis");
    
    using HMM = HybridMemoryMap<PureFixed>;
    
    TEST_ASSERT(!HMM::has_variable_fields, "Should have no variable fields");
    TEST_ASSERT_EQ(HMM::variable_field_count, 0u, "Variable field count should be 0");
    TEST_ASSERT(HMM::copy_region_count > 0, "Should have copy regions");
    
    TEST_PRINT("PureFixed: " << HMM::copy_region_count << " copy regions, base size = " << HMM::base_packed_size << " bytes");
    
    return true;
}

bool variable_field_analysis() {
    TEST_SECTION("Struct with variable field analysis");
    
    using HMM = HybridMemoryMap<WithVariable>;
    
    TEST_ASSERT(HMM::has_variable_fields, "Should have variable fields");
    TEST_ASSERT_EQ(HMM::variable_field_count, 1u, "Should have 1 variable field");
    
    TEST_PRINT("WithVariable: " << HMM::copy_region_count << " copy regions, "
                << HMM::variable_field_count << " variable fields, base size = " << HMM::base_packed_size << " bytes");
    
    // Test runtime size calculation
    WithVariable obj1{42, {}, 12345};
    obj1.values.push_back(1);
    obj1.values.push_back(2);
    obj1.values.push_back(3);
    
    std::size_t size1 = HMM::calculate_packed_size(obj1);
    // base(4) + length(4) + 3*uint16(6) + timestamp(8) = 22
    std::size_t expected1 = HMM::base_packed_size + sizeof(uint32_t) + 3 * sizeof(uint16_t) + 8;
    TEST_ASSERT_EQ(size1, expected1, "Size should include length prefix + 3 uint16_t elements");
    
    WithVariable obj2{99, {}, 67890};
    std::size_t size2 = HMM::calculate_packed_size(obj2);
    // base(4) + length(4) + timestamp(8) = 16
    std::size_t expected2 = HMM::base_packed_size + sizeof(uint32_t) + 8;  // Length prefix + timestamp
    TEST_ASSERT_EQ(size2, expected2, "Empty vector should be base size + length prefix");
    
    return true;
}

bool multi_variable_analysis() {
    TEST_SECTION("Struct with multiple variable fields");
    
    using HMM = HybridMemoryMap<MultiVariable>;
    
    TEST_ASSERT(HMM::has_variable_fields, "Should have variable fields");
    TEST_ASSERT_EQ(HMM::variable_field_count, 2u, "Should have 2 variable fields");
    
    TEST_PRINT("MultiVariable: " << HMM::copy_region_count << " copy regions, "
                << HMM::variable_field_count << " variable fields, base size = " << HMM::base_packed_size << " bytes");
    
    MultiVariable obj{"test", 5, {}, 0xAB};
    obj.name = "Hello";
    obj.data.push_back(1.0f);
    obj.data.push_back(2.0f);
    
    std::size_t size = HMM::calculate_packed_size(obj);
    // base + name(4+5) + count(4) + data(4+8) + flags(2) = 0 + 9 + 4 + 12 + 2 = 27
    std::size_t expected = HMM::base_packed_size + 
                          sizeof(uint32_t) + 5 * sizeof(char) +  // name with length prefix
                          4 +  // count (runtime offset block)
                          sizeof(uint32_t) + 2 * sizeof(float) +  // data with length prefix
                          2;  // flags (runtime offset block)
    TEST_ASSERT_EQ(size, expected, "Size should include length prefixes + name and data elements");
    
    return true;
}

bool pure_fixed_serialization() {
    TEST_SECTION("Pure fixed serialization round-trip");
    
    PureFixed original{0x12345678, 0xABCD, 0xFEDCBA9876543210, 3.14159f};
    
    auto buffer = serialize_unified(original);
    auto restored = deserialize_unified<PureFixed>(buffer);
    
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
    auto buffer = serialize_unified(original);
    
    // Check size
    using HMM = HybridMemoryMap<WithVariable>;
    std::size_t expected_size = HMM::calculate_packed_size(original);
    TEST_ASSERT_EQ(buffer.size(), expected_size, "Buffer size should match calculated size");
    
    // Deserialize
    auto restored = deserialize_unified<WithVariable>(buffer);
    
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
    auto buffer = serialize_unified(original);
    
    // Deserialize
    auto restored = deserialize_unified<MultiVariable>(buffer);
    
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

bool copy_region_optimization() {
    TEST_SECTION("Copy region optimization");
    
    using HMM = HybridMemoryMap<PureFixed>;
    
    TEST_PRINT("Copy regions for PureFixed:");
    for (std::size_t i = 0; i < HMM::copy_region_count; ++i) {
        const auto& region = HMM::copy_regions[i];
        TEST_PRINT("  Region " << i << ": src=" << region.src_offset 
                    << " dst=" << region.dst_offset << " size=" << region.size);
    }
    
    TEST_ASSERT(HMM::copy_region_count <= 4, "Should have few copy regions");
    
    return true;
}

} // namespace tests

// ============================================================================
// Test Suite
// ============================================================================

struct HybridBinaryTests : TestSuite<HybridBinaryTests> {
    static constexpr const char* name = "Hybrid Memory Map & Binary Serialization";
    
    static bool run() {
        bool success = true;
        success &= tests::hybrid_memory_map_analysis();
        success &= tests::variable_field_analysis();
        success &= tests::multi_variable_analysis();
        success &= tests::pure_fixed_serialization();
        success &= tests::variable_field_serialization();
        success &= tests::multi_variable_serialization();
        success &= tests::copy_region_optimization();
        return success;
    }
};

int main() {
    return TestRunner::run<HybridBinaryTests>();
}
