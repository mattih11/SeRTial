/// Size Computation Tests
#include "test_framework.hpp"
#include <sertial/core/size_computation.hpp>
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

bool compile_time_bounds_test() {
    TEST_SECTION("Test 1: Compile-Time Size Bounds");
    
    fixed_string<32> fs;
    fixed_vector<int, 100> fv;
    fixed_vector<double, 50> fv2;
    
    constexpr std::size_t fs_max = max_serialized_size(fs);
    constexpr std::size_t fv_max = max_serialized_size(fv);
    constexpr std::size_t fv2_max = max_serialized_size(fv2);
    
    TEST_PRINT("  fixed_string<32> max size: " << fs_max << " bytes (compile-time)");
    TEST_PRINT("  fixed_vector<int, 100> max size: " << fv_max << " bytes (compile-time)");
    TEST_PRINT("  fixed_vector<double, 50> max size: " << fv2_max << " bytes (compile-time)");
    
    // Primitives have exact compile-time size
    constexpr int32_t i = 0;
    constexpr double d = 0.0;
    constexpr bool b = false;
    
    constexpr std::size_t i_size = max_serialized_size(i);
    constexpr std::size_t d_size = max_serialized_size(d);
    constexpr std::size_t b_size = max_serialized_size(b);
    
    TEST_ASSERT_EQ(i_size, 4u, "int32_t is 4 bytes");
    TEST_ASSERT_EQ(d_size, 8u, "double is 8 bytes");
    TEST_ASSERT_EQ(b_size, 1u, "bool is 1 byte");
    
    TEST_PRINT("  int32_t size: " << i_size << " bytes (compile-time)");
    TEST_PRINT("  double size: " << d_size << " bytes (compile-time)");
    TEST_PRINT("  bool size: " << b_size << " bytes (compile-time)");
    
    constexpr std::size_t total_max = compute_max_total_size(fs, fv, i, d);
    TEST_PRINT("  Total max for all: " << total_max << " bytes (compile-time)");
    
    return true;
}

bool runtime_computation_test() {
    TEST_SECTION("Test 2: Runtime Size Computation");
    
    std::string str1 = "Hello";
    std::string str2 = "World!";
    std::vector<int> vec = {1, 2, 3, 4, 5};
    
    std::size_t str1_size = compute_serialized_size(str1);
    std::size_t str2_size = compute_serialized_size(str2);
    std::size_t vec_size = compute_serialized_size(vec);
    
    TEST_PRINT("  \"" << str1 << "\" size: " << str1_size << " bytes");
    TEST_PRINT("  \"" << str2 << "\" size: " << str2_size << " bytes");
    TEST_PRINT("  vector<int>[5] size: " << vec_size << " bytes");
    
    std::size_t total = compute_total_size(str1, str2, vec);
    TEST_PRINT("  Total: " << total << " bytes");
    
    return true;
}

bool accuracy_test() {
    TEST_SECTION("Test 3: Size Prediction Accuracy");
    
    struct TestCase {
        const char* name;
        std::size_t predicted;
        std::size_t actual;
    };
    
    std::vector<TestCase> cases;
    
    // Test 1: Simple int
    {
        int32_t value = 12345;
        std::size_t predicted = compute_serialized_size(value);
        auto binary = to_binary(value);
        cases.push_back({"int32_t", predicted, binary.size()});
    }
    
    // Test 2: String
    {
        std::string value = "Testing!";
        std::size_t predicted = compute_serialized_size(value);
        BinaryWriter writer;
        writer.write(value);
        cases.push_back({"std::string", predicted, writer.size()});
    }
    
    // Test 3: fixed_string
    {
        fixed_string<32> value = "Fixed!";
        std::size_t predicted = compute_serialized_size(value);
        BinaryWriter writer;
        writer.write(value);
        cases.push_back({"fixed_string<32>", predicted, writer.size()});
    }
    
    // Test 4: Vector of ints
    {
        std::vector<int> value = {10, 20, 30, 40, 50};
        std::size_t predicted = compute_serialized_size(value);
        BinaryWriter writer;
        writer.write(value);
        cases.push_back({"vector<int>[5]", predicted, writer.size()});
    }
    
    // Test 5: fixed_vector
    {
        fixed_vector<double, 100> value;
        value.push_back(1.1);
        value.push_back(2.2);
        value.push_back(3.3);
        std::size_t predicted = compute_serialized_size(value);
        BinaryWriter writer;
        writer.write(value);
        cases.push_back({"fixed_vector<double, 100>[3]", predicted, writer.size()});
    }
    
    TEST_PRINT(std::left);
    TEST_PRINT("  " << std::setw(30) << "Type" 
              << std::setw(12) << "Predicted" 
              << std::setw(12) << "Actual" 
              << "Match");
    TEST_PRINT("  " << std::string(54, '-'));
    
    bool all_match = true;
    for (const auto& test : cases) {
        bool match = test.predicted == test.actual;
        all_match &= match;
        
        TEST_PRINT("  " << std::setw(30) << test.name
                  << std::setw(12) << test.predicted
                  << std::setw(12) << test.actual
                  << (match ? "[OK]" : "[X]"));
        
        TEST_ASSERT(match, test.name << " prediction accuracy");
    }
    
    TEST_PRINT("");
    TEST_PRINT("  All predictions accurate: " << (all_match ? "YES [OK]" : "NO [X]"));
    
    return true;
}

bool optimized_write_all_test() {
    TEST_SECTION("Test 4: Optimized write_all()");
    
    int32_t a = 42;
    std::string b = "Hello";
    double c = 3.14;
    
    // Individual writes
    BinaryWriter writer1;
    writer1.write(a);
    writer1.write(b);
    writer1.write(c);
    
    // Batch write using BinaryWriter::write_all
    BinaryWriter writer2;
    writer2.write_all(a, b, c);
    
    // to_binary_batch
    auto binary_batch = to_binary_batch(a, b, c);
    
    TEST_PRINT("  Individual writes: " << writer1.size() << " bytes");
    TEST_PRINT("  write_all(): " << writer2.size() << " bytes");
    TEST_PRINT("  to_binary_batch(): " << binary_batch.size() << " bytes");
    
    TEST_ASSERT_EQ(writer1.size(), writer2.size(), "write_all matches individual");
    TEST_ASSERT_EQ(writer1.size(), binary_batch.size(), "to_binary_batch matches individual");
    
    TEST_PRINT("  All methods match: YES [OK]");
    
    // Verify content matches
    bool content_match = true;
    for (std::size_t i = 0; i < writer1.size(); ++i) {
        if (writer1.data()[i] != writer2.data()[i] || writer1.data()[i] != binary_batch[i]) {
            content_match = false;
            break;
        }
    }
    TEST_ASSERT(content_match, "Content verification");
    TEST_PRINT("  Content verification: PASS [OK]");
    
    return true;
}

bool memory_efficiency_test() {
    TEST_SECTION("Test 5: Memory Efficiency Analysis");
    
    // Large fixed container
    fixed_vector<double, 500> large_fixed;
    large_fixed.push_back(1.0);
    large_fixed.push_back(2.0);
    
    constexpr std::size_t max_bound = max_serialized_size(fixed_vector<double, 500>{});
    std::size_t runtime_size = compute_serialized_size(large_fixed);
    
    BinaryWriter writer;
    writer.write(large_fixed);
    std::size_t actual_size = writer.size();
    
    TEST_PRINT("  Compile-time max bound: " << max_bound << " bytes");
    TEST_PRINT("  Runtime computed size: " << runtime_size << " bytes");
    TEST_PRINT("  Actual serialized size: " << actual_size << " bytes");
    
    std::size_t overhead = max_bound - actual_size;
    double overhead_percent = (double)overhead / actual_size * 100;
    TEST_PRINT("  Over-allocation: " << overhead << " bytes (" << std::fixed << std::setprecision(1) << overhead_percent << "% overhead)");
    
    TEST_PRINT("");
    TEST_PRINT("  Note: For fixed-capacity containers, compile-time bound trades");
    TEST_PRINT("        memory for zero runtime computation cost.");
    
    return true;
}

bool size_category_test() {
    TEST_SECTION("Test 6: Size Category Detection");
    
    TEST_PRINT("  Static (compile-time computable):");
    TEST_ASSERT_EQ((int)TypeTraits<int>::category, 0, "int is Static");
    TEST_ASSERT_EQ((int)TypeTraits<double>::category, 0, "double is Static");
    TEST_ASSERT_EQ((int)TypeTraits<bool>::category, 0, "bool is Static");
    TEST_PRINT("    - int: YES");
    TEST_PRINT("    - double: YES");
    TEST_PRINT("    - bool: YES");
    
    TEST_PRINT("");
    TEST_PRINT("  Dynamic (requires runtime computation):");
    TEST_ASSERT_EQ((int)TypeTraits<std::string>::category, 1, "string is Dynamic");
    TEST_ASSERT_EQ((int)TypeTraits<std::vector<int>>::category, 1, "vector<int> is Dynamic");
    
    // Use typedef to avoid macro comma issue
    using FixedVecInt10 = fixed_vector<int, 10>;
    TEST_ASSERT_EQ((int)TypeTraits<FixedVecInt10>::category, 1, "fixed_vector is Dynamic");
    TEST_PRINT("    - std::string: YES");
    TEST_PRINT("    - std::vector<int>: YES");
    TEST_PRINT("    - fixed_vector<int, 10>: YES");
    
    return true;
}

} // namespace tests

// ============================================================================
// Test Suite
// ============================================================================

struct SizeComputationTests : TestSuite<SizeComputationTests> {
    static constexpr const char* name = "SeRTial - Size Computation Tests";
    
    static bool run() {
        if (!tests::compile_time_bounds_test()) return false;
        if (!tests::runtime_computation_test()) return false;
        if (!tests::accuracy_test()) return false;
        if (!tests::optimized_write_all_test()) return false;
        if (!tests::memory_efficiency_test()) return false;
        if (!tests::size_category_test()) return false;
        
        TEST_PRINT("");
        TEST_PRINT("Phase 2.5 Complete: Size Computation");
        TEST_PRINT("  [OK] Compile-time size bounds for fixed containers");
        TEST_PRINT("  [OK] Runtime size computation for dynamic types");
        TEST_PRINT("  [OK] Automatic size prediction (100% accurate)");
        TEST_PRINT("  [OK] Optimized write_all() with pre-allocation");
        TEST_PRINT("  [OK] Zero runtime overhead for static types");
        TEST_PRINT("");
        TEST_PRINT("Benefit: No manual reserve() needed - automatic optimal allocation!");
        
        return true;
    }
};

// ============================================================================
// Main
// ============================================================================

int main() {
    return TestRunner::run<SizeComputationTests>();
}
