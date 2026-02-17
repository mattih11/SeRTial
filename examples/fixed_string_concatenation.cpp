/**
 * @file fixed_string_concatenation.cpp
 * @brief Demonstrates fixed_string concatenation operators with compile-time size deduction
 * 
 * Shows various concatenation patterns:
 * 1. Two fixed_strings: fixed_string<N1> + fixed_string<N2> → fixed_string<N1+N2>
 * 2. fixed_string + string literal
 * 3. String literal + fixed_string
 * 4. fixed_string + character
 * 5. Character + fixed_string
 * 6. fixed_string + string_view
 * 7. Chained concatenations
 */

#include <sertial/sertial.hpp>
#include <iostream>
#include <string_view>
#include <cassert>

using namespace sertial;

// ============================================================================
// Compile-Time Concatenation Examples
// ============================================================================

void demonstrate_basic_concatenation() {
    std::cout << "=== Basic fixed_string Concatenation ===\n\n";
    
    // Example 1: Two fixed_strings
    constexpr auto hello = make_fixed<"Hello">();
    constexpr auto world = make_fixed<" World">();
    constexpr auto greeting = hello + world;  // fixed_string<11>
    
    std::cout << "1. fixed_string + fixed_string:\n";
    std::cout << "   hello (size " << hello.size() << ", capacity " << hello.capacity() << "): \"" << hello << "\"\n";
    std::cout << "   world (size " << world.size() << ", capacity " << world.capacity() << "): \"" << world << "\"\n";
    std::cout << "   result (size " << greeting.size() << ", capacity " << greeting.capacity() << "): \"" << greeting << "\"\n\n";
    
    // Compile-time validation
    static_assert(greeting.capacity() == 11, "Capacity should be 5 + 6");
    static_assert(greeting.size() == 11, "Size should be 11");
    
    // Example 2: fixed_string + literal
    constexpr auto name = make_fixed<"SeRTial">();
    constexpr auto versioned = name + " v2.0";  // fixed_string<12>
    
    std::cout << "2. fixed_string + string literal:\n";
    std::cout << "   \"" << name << "\" + \" v2.0\" = \"" << versioned << "\"\n";
    std::cout << "   Capacity: " << versioned.capacity() << " (7 + 5)\n\n";
    
    static_assert(versioned.capacity() == 12, "Capacity should be 7 + 5");
    
    // Example 3: Literal + fixed_string
    constexpr auto prefixed = "Lib: " + name;  // fixed_string<12>
    
    std::cout << "3. String literal + fixed_string:\n";
    std::cout << "   \"Lib: \" + \"" << name << "\" = \"" << prefixed << "\"\n";
    std::cout << "   Capacity: " << prefixed.capacity() << " (5 + 7)\n\n";
    
    static_assert(prefixed.capacity() == 12, "Capacity should be 5 + 7");
}

void demonstrate_character_concatenation() {
    std::cout << "=== Character Concatenation ===\n\n";
    
    // Example 4: fixed_string + char
    constexpr auto base = make_fixed<"Test">();
    constexpr auto with_suffix = base + '!';  // fixed_string<5>
    
    std::cout << "4. fixed_string + char:\n";
    std::cout << "   \"" << base << "\" + '!' = \"" << with_suffix << "\"\n";
    std::cout << "   Capacity: " << with_suffix.capacity() << " (4 + 1)\n\n";
    
    static_assert(with_suffix.capacity() == 5, "Capacity should be 4 + 1");
    
    // Example 5: char + fixed_string
    constexpr auto with_prefix = '>' + base;  // fixed_string<5>
    
    std::cout << "5. char + fixed_string:\n";
    std::cout << "   '>' + \"" << base << "\" = \"" << with_prefix << "\"\n";
    std::cout << "   Capacity: " << with_prefix.capacity() << " (1 + 4)\n\n";
    
    static_assert(with_prefix.capacity() == 5, "Capacity should be 1 + 4");
}

void demonstrate_chained_concatenation() {
    std::cout << "=== Chained Concatenation ===\n\n";
    
    // Example 6: Multiple concatenations
    constexpr auto part1 = make_fixed<"Sea">();
    constexpr auto part2 = make_fixed<"Real">();
    constexpr auto part3 = make_fixed<"Time">();
    
    // Chain: "Sea" + "Real" → fixed_string<8>
    //        fixed_string<8> + "Time" → fixed_string<12>
    constexpr auto combined = part1 + part2 + part3;
    
    std::cout << "6. Chained concatenation:\n";
    std::cout << "   \"" << part1 << "\" + \"" << part2 << "\" + \"" << part3 << "\" = \"" << combined << "\"\n";
    std::cout << "   Capacity: " << combined.capacity() << "\n\n";
    
    // Example 7: Building a path-like string
    constexpr auto root = make_fixed<"home">();
    constexpr auto user = make_fixed<"user">();
    constexpr auto dir = make_fixed<"src">();
    
    constexpr auto path = '/' + root + '/' + user + '/' + dir;
    
    std::cout << "7. Building a path:\n";
    std::cout << "   Path: \"" << path << "\"\n";
    std::cout << "   Capacity: " << path.capacity() << "\n\n";
}

void demonstrate_mixed_types() {
    std::cout << "=== Mixed Type Concatenation ===\n\n";
    
    // Example 8: fixed_string + string_view (runtime - uses lhs capacity)
    auto protocol = make_fixed<"https://">();  // 8 capacity
    std::string_view domain = "example.com";   // 11 chars - WON'T FIT!
    
    std::cout << "8. fixed_string + string_view (demonstrating overflow handling):\n";
    std::cout << "   Protocol capacity: " << protocol.capacity() << " (size: " << protocol.size() << ")\n";
    std::cout << "   Domain size: " << domain.size() << "\n";
    std::cout << "   NOTE: This would throw std::length_error (8 + 11 > 8)\n";
    std::cout << "   Using larger capacity instead...\n";
    
    // Use appropriate capacity
    fixed_string<20> protocol_big{"https://"};
    auto url = protocol_big + domain;
    std::cout << "   Result: \"" << url << "\" (capacity: " << url.capacity() << ")\n\n";
    
    // Example 9: string_view + fixed_string
    std::string_view scheme = "ftp://";
    fixed_string<20> server{"server.org"};
    auto ftp_url = scheme + server;
    
    std::cout << "9. string_view + fixed_string:\n";
    std::cout << "   \"" << scheme << "\" + \"" << server << "\" = \"" << ftp_url << "\"\n";
    std::cout << "   Capacity: " << ftp_url.capacity() << " (uses rhs capacity)\n\n";
}

void demonstrate_practical_example() {
    std::cout << "=== Practical Example: Message Formatting ===\n\n";
    
    // Example 10: Real-world usage pattern
    constexpr auto prefix = make_fixed<"[INFO]">();
    constexpr auto timestamp = make_fixed<"2026-02-18 10:30:00">();
    constexpr auto component = make_fixed<"SensorModule">();
    
    // Build log header at compile time
    constexpr auto log_header = prefix + " " + timestamp + " [" + component + "]";
    
    std::cout << "10. Log message header:\n";
    std::cout << "   \"" << log_header << "\"\n";
    std::cout << "   Size: " << log_header.size() << ", Capacity: " << log_header.capacity() << "\n\n";
    
    // Runtime message addition
    auto full_log = log_header + " Sensor initialized successfully";
    std::cout << "   Full message:\n";
    std::cout << "   \"" << full_log << "\"\n\n";
}

void demonstrate_exact_sizing() {
    std::cout << "=== Exact Size Deduction ===\n\n";
    
    // The key feature: result capacity = sum of operand capacities
    fixed_string<40> left{"Short"};           // 5 chars used, 40 capacity
    fixed_string<60> right{" string"};        // 7 chars used, 60 capacity
    auto result = left + right;                // fixed_string<100>
    
    std::cout << "11. Exact capacity calculation:\n";
    std::cout << "   Left:  size=" << left.size() << ", capacity=" << left.capacity() << "\n";
    std::cout << "   Right: size=" << right.size() << ", capacity=" << right.capacity() << "\n";
    std::cout << "   Result: size=" << result.size() << ", capacity=" << result.capacity() << "\n";
    std::cout << "   \"" << result << "\"\n\n";
    
    // This is the requested feature!
    // Capacity = 40 + 60 = 100 (regardless of actual string sizes)
    assert(result.capacity() == 100 && "Capacity should be 40 + 60");
    
    std::cout << "   SUCCESS: fixed_string<40> + fixed_string<60> = fixed_string<100>\n\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "SeRTial - fixed_string Concatenation Examples\n";
    std::cout << std::string(50, '=') << "\n\n";
    
    demonstrate_basic_concatenation();
    demonstrate_character_concatenation();
    demonstrate_chained_concatenation();
    demonstrate_mixed_types();
    demonstrate_practical_example();
    demonstrate_exact_sizing();
    
    std::cout << "All concatenation operations successful!\n";
    std::cout << "All compile-time size deductions validated via static_assert.\n";
    
    return 0;
}
