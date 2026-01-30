#pragma once

/// Test Framework for SeRTial
/// 
/// Provides a simple, templated test runner that can be used across all test files.
/// 
/// Usage:
///   struct MyTests : TestSuite<MyTests> {
///       static constexpr const char* name = "My Test Suite";
///       static bool run() {
///           TEST_ASSERT(1 + 1 == 2, "Basic math");
///           TEST_ASSERT_EQ(compute(), expected, "Computation");
///           return true;
///       }
///   };
///   
///   int main() { return TestRunner::run<MyTests>(); }

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <functional>
#include <cassert>

namespace sertial::test {

// ============================================================================
// Test Macros
// ============================================================================

// Use variadic macros to handle commas in template arguments
#define TEST_ASSERT(cond, ...) \
    do { \
        if (!(cond)) { \
            std::cout << "    FAIL: " << __VA_ARGS__ << " (line " << __LINE__ << ")\n"; \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(actual, expected, ...) \
    do { \
        if ((actual) != (expected)) { \
            std::cout << "    FAIL: " << __VA_ARGS__ << "\n"; \
            std::cout << "      Expected: " << (expected) << "\n"; \
            std::cout << "      Actual:   " << (actual) << "\n"; \
            return false; \
        } \
    } while(0)

#define TEST_SECTION(name) \
    std::cout << "\n" << name << "\n" << std::string(strlen(name), '-') << "\n"

// Wrap in do-while to handle expressions with commas
#define TEST_PRINT(...) do { std::cout << "  " << __VA_ARGS__ << "\n"; } while(0)

// ============================================================================
// Test Suite Base
// ============================================================================

/// Base class for test suites (CRTP pattern)
template<typename Derived>
struct TestSuite {
    /// Run the test suite, return true if all tests pass
    static bool execute() {
        std::cout << "\n" << std::string(50, '=') << "\n";
        std::cout << Derived::name << "\n";
        std::cout << std::string(50, '=') << "\n";
        
        bool result = Derived::run();
        
        std::cout << "\n" << std::string(50, '-') << "\n";
        if (result) {
            std::cout << "[OK] All tests passed!\n";
        } else {
            std::cout << "[X] Some tests failed!\n";
        }
        std::cout << std::string(50, '=') << "\n";
        
        return result;
    }
};

// ============================================================================
// Test Runner
// ============================================================================

/// Runs one or more test suites
struct TestRunner {
    /// Run a single test suite
    template<typename Suite>
    static int run() {
        return Suite::execute() ? 0 : 1;
    }
    
    /// Run multiple test suites
    template<typename... Suites>
    static int run_all() {
        bool all_passed = true;
        ((all_passed &= Suites::execute()), ...);
        return all_passed ? 0 : 1;
    }
};

// ============================================================================
// Type-Parameterized Test Helpers
// ============================================================================

/// Test a roundtrip serialization for a type
template<typename T, typename Serializer, typename Deserializer>
bool test_roundtrip(const T& original, Serializer serialize_fn, Deserializer deserialize_fn, 
                    const char* type_name = nullptr) {
    auto data = serialize_fn(original);
    auto restored = deserialize_fn(data);
    
    if (!restored.has_value()) {
        if (type_name) std::cout << "    FAIL: Deserialization failed for " << type_name << "\n";
        return false;
    }
    
    // For simple comparison, serialize again and compare bytes
    auto data2 = serialize_fn(*restored);
    if (data.size() != data2.size()) {
        if (type_name) std::cout << "    FAIL: Size mismatch for " << type_name << "\n";
        return false;
    }
    
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] != data2[i]) {
            if (type_name) std::cout << "    FAIL: Data mismatch at byte " << i << " for " << type_name << "\n";
            return false;
        }
    }
    
    return true;
}

/// Print hex dump of data
inline void print_hex(const std::byte* data, size_t size, size_t max_bytes = 32) {
    for (size_t i = 0; i < std::min(size, max_bytes); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(data[i]) << " ";
    }
    if (size > max_bytes) std::cout << "...";
    std::cout << std::dec << std::setfill(' ');
}

template<typename Container>
void print_hex(const Container& data, size_t max_bytes = 32) {
    print_hex(reinterpret_cast<const std::byte*>(data.data()), data.size(), max_bytes);
}

} // namespace sertial::test
