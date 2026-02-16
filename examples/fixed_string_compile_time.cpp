/// Compile-Time fixed_string Examples
///
/// Demonstrates compile-time construction and manipulation of fixed_string
/// using string literals, constexpr, and string_view conversions.
///
/// Key Features Demonstrated:
/// 1. Class Template Argument Deduction (CTAD) - auto size inference
/// 2. Non-Type Template Parameters (NTTP) - string literals as template args
/// 3. User-defined literals (_fs suffix)
/// 4. rfl::internal::StringLiteral integration (reflect-cpp) - FULL constexpr support
/// 5. Constexpr operations and compile-time validation
/// 6. Zero-cost string_view integration
///
/// Build:
///   cmake --build build --target fixed_string_compile_time
/// Run:
///   ./build/fixed_string_compile_time

#include <sertial/containers/fixed_string.hpp>
#include <rfl.hpp>
#include <iostream>
#include <string_view>

using namespace sertial;

// ============================================================================
// Compile-Time Construction Examples
// ============================================================================

// Example 1: Using array constructor with template deduction
constexpr fixed_string<7> name_from_literal{"SeRTial"};

// Example 2: Auto-deduced size using CTAD (Class Template Argument Deduction)
constexpr fixed_string auto_deduced{"C++20"};  // Deduces fixed_string<5>

// Example 3: Using fixed_string_literal helper
constexpr auto literal_helper = fixed_string_literal("RealTime");  // fixed_string<8>

// Example 4: Using make_fixed with StringLiteral (C++20 NTTP)
constexpr auto nttp_string = make_fixed<"Embedded">();  // fixed_string<8>

// Example 5: Using rfl::internal::StringLiteral (reflect-cpp integration)
constexpr rfl::internal::StringLiteral rfl_lit{"field_name"};
constexpr auto rfl_literal = make_fixed(rfl_lit);  // fixed_string<10>

// Example 6: Direct construction from rfl::internal::StringLiteral
constexpr rfl::internal::StringLiteral sensor_lit{"sensor_id"};
constexpr fixed_string<9> sensor_id{sensor_lit};

// Example 7: Constexpr string_view from rfl::internal::StringLiteral
constexpr std::string_view rfl_as_view = rfl_lit.string_view();

// Example 8: Constexpr string_view conversion from fixed_string
constexpr std::string_view view_from_fixed = name_from_literal;

// Example 9: Constexpr comparison in function
constexpr bool check_strings_equal() {
    fixed_string<8> a{"Test"};
    fixed_string<8> b{"Test"};
    return a == b;
}
constexpr bool strings_are_equal = check_strings_equal();

// Example 10: Constexpr size query
constexpr std::size_t name_length = name_from_literal.size();

// ============================================================================
// Runtime Demonstrations
// ============================================================================

void demonstrate_compile_time_strings() {
    std::cout << "=== Compile-Time fixed_string Examples ===\n\n";
    
    // All these values were computed at compile time!
    std::cout << "1. Name from literal: " << name_from_literal << "\n";
    std::cout << "   Size: " << name_from_literal.size() << "\n";
    std::cout << "   Capacity: " << name_from_literal.capacity() << "\n\n";
    
    std::cout << "2. Auto-deduced (CTAD): " << auto_deduced << "\n";
    std::cout << "   Type: fixed_string<" << auto_deduced.max_size() << ">\n";
    std::cout << "   Size: " << auto_deduced.size() << "\n\n";
    
    std::cout << "3. Literal helper: " << literal_helper << "\n";
    std::cout << "   Type: fixed_string<" << literal_helper.max_size() << ">\n\n";
    
    std::cout << "4. NTTP (make_fixed): " << nttp_string << "\n";
    std::cout << "   Type: fixed_string<" << nttp_string.max_size() << ">\n\n";
    
    std::cout << "5. rfl::internal::StringLiteral (make_fixed): " << rfl_literal << "\n";
    std::cout << "   Type: fixed_string<" << rfl_literal.max_size() << ">\n\n";
    
    std::cout << "6. rfl::internal::StringLiteral (direct): " << sensor_id << "\n";
    std::cout << "   Type: fixed_string<" << sensor_id.max_size() << ">\n\n";
    
    std::cout << "7. As string_view: " << view_from_fixed << "\n";
    std::cout << "   View size: " << view_from_fixed.size() << "\n\n";
    
    std::cout << "8. Compile-time comparison result: " 
              << (strings_are_equal ? "true" : "false") << "\n\n";
    
    std::cout << "9. Compile-time length: " << name_length << "\n\n";
    
    // Demonstrate user-defined literal
    using namespace sertial::literals;
    auto with_literal = "TestData"_fs;
    std::cout << "10. User-defined literal: " << with_literal << "\n";
    std::cout << "   Type: fixed_string<" << with_literal.max_size() << ">\n\n";
}

// ============================================================================
// Integration with string_view
// ============================================================================

constexpr std::string_view extract_view(const fixed_string<32>& str) {
    return str;  // Implicit conversion
}

void demonstrate_string_view_integration() {
    std::cout << "=== string_view Integration ===\n\n";
    
    fixed_string<32> message{"Real-Time Safe"};
    std::string_view view = message;
    
    std::cout << "Original fixed_string: " << message << "\n";
    std::cout << "As string_view: " << view << "\n";
    std::cout << "View size: " << view.size() << "\n";
    std::cout << "starts_with('Real'): " << message.starts_with("Real") << "\n";
    std::cout << "ends_with('Safe'): " << message.ends_with("Safe") << "\n";
    std::cout << "contains('Time'): " << message.contains("Time") << "\n\n";
}

// ============================================================================
// Compile-Time String Manipulation
// ============================================================================

void demonstrate_constexpr_operations() {
    std::cout << "=== Constexpr Operations ===\n\n";
    
    // Compile-time string construction
    fixed_string<32> prefix{"LOG: "};
    fixed_string<32> suffix{"[INFO]"};
    
    std::cout << "Prefix: " << prefix << "\n";
    std::cout << "Suffix: " << suffix << "\n";
    
    // Runtime concatenation (using constexpr-enabled methods)
    fixed_string<64> combined;
    combined.append(std::string_view(prefix));
    combined.append("Message ");
    combined.append(std::string_view(suffix));
    std::cout << "Combined: " << combined << "\n\n";
}

// ============================================================================
// Use Case: Type Names
// ============================================================================

void demonstrate_type_names() {
    std::cout << "=== Type Names with fixed_string ===\n\n";
    
    fixed_string<64> int_name{"Integer"};
    fixed_string<64> float_name{"Float"};
    fixed_string<64> double_name{"Double"};
    
    std::cout << "int type: " << int_name << "\n";
    std::cout << "float type: " << float_name << "\n";
    std::cout << "double type: " << double_name << "\n\n";
}

// ============================================================================
// Use Case: reflect-cpp Integration
// ============================================================================

void demonstrate_rfl_integration() {
    std::cout << "=== reflect-cpp Integration ===\n\n";
    
    // Using rfl::internal::StringLiteral for field names
    constexpr rfl::internal::StringLiteral temp_lit{"temperature"};
    constexpr rfl::internal::StringLiteral humid_lit{"humidity"};
    constexpr rfl::internal::StringLiteral press_lit{"pressure"};
    
    constexpr auto field1 = make_fixed(temp_lit);
    constexpr auto field2 = make_fixed(humid_lit);
    constexpr auto field3 = make_fixed(press_lit);
    
    std::cout << "Field names from rfl::internal::StringLiteral:\n";
    std::cout << "  " << field1 << " (size: " << field1.size() << ")\n";
    std::cout << "  " << field2 << " (size: " << field2.size() << ")\n";
    std::cout << "  " << field3 << " (size: " << field3.size() << ")\n\n";
    
    // Example: Using with rfl::Field for runtime introspection
    struct SensorData {
        float temperature;
        float humidity;
        float pressure;
    };
    
    // Field names can be extracted from rfl::Field at compile time
    std::cout << "Demonstrates compile-time field name extraction\n";
    std::cout << "compatible with reflect-cpp's rfl::Field<> pattern\n\n";
}

// ============================================================================
// Static Assertions (Compile-Time Validation)
// ============================================================================

// Verify compile-time properties
static_assert(name_from_literal.size() == 7, "Size should be 7");
static_assert(name_from_literal.capacity() == 7, "Capacity should be 7");
static_assert(!name_from_literal.empty(), "Should not be empty");
static_assert(strings_are_equal == true, "Strings should be equal");
static_assert(name_length == 7, "Length should be 7");

// Verify auto-deduction
static_assert(auto_deduced.size() == 5, "Auto-deduced size should be 5");
static_assert(literal_helper.size() == 8, "Literal helper size should be 8");
static_assert(nttp_string.size() == 8, "NTTP string size should be 8");
static_assert(rfl_literal.size() == 10, "rfl::internal::StringLiteral size should be 10");
static_assert(sensor_id.size() == 9, "rfl::internal::StringLiteral direct size should be 9");

// ============================================================================
// Comprehensive Compile-Time Validation Tests
// ============================================================================

// 1. rfl::internal::StringLiteral compile-time properties
static_assert(rfl_lit.length == 10, "rfl_lit length should be 10");
static_assert(sensor_lit.length == 9, "sensor_lit length should be 9");

// 2. fixed_string sizes and capacities from rfl
static_assert(rfl_literal.size() == 10, "rfl_literal size validation");
static_assert(rfl_literal.capacity() == 10, "rfl_literal capacity should be 10");
static_assert(sensor_id.size() == 9, "sensor_id size validation");
static_assert(sensor_id.capacity() == 9, "sensor_id capacity should be 9");

// 3. string_view sizes at compile time
static_assert(rfl_as_view.size() == 10, "rfl_as_view size should be 10");
static_assert(view_from_fixed.size() == 7, "view_from_fixed size should be 7");

// 4. Cross-validation: rfl vs fixed_string
static_assert(rfl_literal.size() == rfl_lit.length, "rfl_literal should match rfl_lit length");
static_assert(sensor_id.size() == sensor_lit.length, "sensor_id should match sensor_lit length");
static_assert(rfl_as_view.size() == rfl_lit.length, "rfl_as_view should match rfl_lit length");

// 5. Emptiness checks
static_assert(!rfl_literal.empty(), "rfl_literal should not be empty");
static_assert(!sensor_id.empty(), "sensor_id should not be empty");

// 6. Equality comparisons (compile-time string content validation)
static_assert(rfl_literal == rfl_as_view, "rfl_literal should equal rfl_as_view");
static_assert(name_from_literal == view_from_fixed, "name_from_literal should equal view_from_fixed");


// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "SeRTial - Compile-Time fixed_string Examples\n";
    std::cout << std::string(50, '=') << "\n\n";
    
    std::cout << "COMPILE-TIME VALIDATION: All static_assert tests passed!\n";
    std::cout << "  - rfl::internal::StringLiteral properties verified\n";
    std::cout << "  - fixed_string sizes and capacities validated\n";
    std::cout << "  - string_view conversions confirmed\n";
    std::cout << "  - Cross-validation between rfl and fixed_string successful\n";
    std::cout << "  - Compile-time equality comparisons verified\n\n";
    
    demonstrate_compile_time_strings();
    demonstrate_string_view_integration();
    demonstrate_constexpr_operations();
    demonstrate_type_names();
    demonstrate_rfl_integration();
    
    std::cout << "All compile-time assertions passed!\n";
    std::cout << "All values were computed at compile time where possible.\n";
    
    return 0;
}
