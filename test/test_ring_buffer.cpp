#include <sertial/containers/ring_buffer.hpp>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include <iostream>

using namespace sertial;

// Test helper
template<typename T>
void test_section(const char* name, T&& func) {
    std::cout << "Testing: " << name << "... ";
    func();
    std::cout << "PASSED\n";
}

// ============================================================================
// Basic Operations Tests
// ============================================================================

void test_basic_construction() {
    RingBuffer<int, 5> buf;
    assert(buf.empty());
    assert(!buf.full());
    assert(buf.size() == 0);
    assert(buf.capacity() == 5);
}

void test_push_back_and_size() {
    RingBuffer<int, 5> buf;
    
    buf.push_back(1);
    assert(buf.size() == 1);
    assert(!buf.empty());
    assert(!buf.full());
    
    buf.push_back(2);
    assert(buf.size() == 2);
    
    buf.push_back(3);
    buf.push_back(4);
    buf.push_back(5);
    assert(buf.size() == 5);
    assert(buf.full());
}

void test_front_back_access() {
    RingBuffer<int, 5> buf;
    
    buf.push_back(1);
    buf.push_back(2);
    
    assert(buf.front() == 1);
    assert(buf.back() == 2);
    
    buf.push_back(3);
    assert(buf.front() == 1);
    assert(buf.back() == 3);
}

void test_clear() {
    RingBuffer<int, 5> buf;
    
    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(3);
    assert(buf.size() == 3);
    
    buf.clear();
    assert(buf.empty());
    assert(buf.size() == 0);
}

// ============================================================================
// Wrap-Around Tests
// ============================================================================

void test_overwrite_oldest() {
    RingBuffer<int, 3> buf;
    
    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(3);
    assert(buf.size() == 3);
    assert(buf.front() == 1);
    assert(buf.back() == 3);
    
    // Overwrite oldest (1)
    buf.push_back(4);
    assert(buf.size() == 3);
    assert(buf.front() == 2);  // 1 was overwritten
    assert(buf.back() == 4);
    
    // Overwrite next oldest (2)
    buf.push_back(5);
    assert(buf.size() == 3);
    assert(buf.front() == 3);  // 2 was overwritten
    assert(buf.back() == 5);
}

void test_continuous_overwrite() {
    RingBuffer<int, 3> buf;
    
    // Fill buffer
    for (int i = 0; i < 3; ++i) {
        buf.push_back(i);
    }
    
    // Continue pushing - should keep overwriting
    for (int i = 3; i < 10; ++i) {
        buf.push_back(i);
        assert(buf.size() == 3);
        assert(buf.front() == i - 2);
        assert(buf.back() == i);
    }
}

// ============================================================================
// Indexing Tests
// ============================================================================

void test_operator_bracket() {
    RingBuffer<int, 5> buf;
    
    for (int i = 0; i < 5; ++i) {
        buf.push_back(i * 10);
    }
    
    // Check all elements via operator[]
    for (size_t i = 0; i < 5; ++i) {
        assert(buf[i] == static_cast<int>(i * 10));
    }
}

void test_indexing_after_wrap() {
    RingBuffer<int, 3> buf;
    
    // Fill: [1, 2, 3]
    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(3);
    
    // Overwrite: [4, 2, 3] (logically: 2, 3, 4)
    buf.push_back(4);
    assert(buf[0] == 2);
    assert(buf[1] == 3);
    assert(buf[2] == 4);
    
    // Overwrite: [4, 5, 3] (logically: 3, 4, 5)
    buf.push_back(5);
    assert(buf[0] == 3);
    assert(buf[1] == 4);
    assert(buf[2] == 5);
}

void test_at_bounds_checking() {
    RingBuffer<int, 5> buf;
    
    buf.push_back(1);
    buf.push_back(2);
    
    assert(buf.at(0) == 1);
    assert(buf.at(1) == 2);
    
    bool caught = false;
    try {
        buf.at(2);  // Out of range
    } catch (const std::out_of_range&) {
        caught = true;
    }
    assert(caught);
}

// ============================================================================
// Pop Operations Tests
// ============================================================================

void test_pop_front() {
    RingBuffer<int, 5> buf;
    
    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(3);
    
    buf.pop_front();
    assert(buf.size() == 2);
    assert(buf.front() == 2);
    
    buf.pop_front();
    assert(buf.size() == 1);
    assert(buf.front() == 3);
    assert(buf.back() == 3);
}

void test_pop_back() {
    RingBuffer<int, 5> buf;
    
    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(3);
    
    buf.pop_back();
    assert(buf.size() == 2);
    assert(buf.back() == 2);
    
    buf.pop_back();
    assert(buf.size() == 1);
    assert(buf.front() == 1);
    assert(buf.back() == 1);
}

void test_pop_and_push() {
    RingBuffer<int, 3> buf;
    
    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(3);
    
    buf.pop_front();
    buf.push_back(4);
    
    assert(buf.size() == 3);
    assert(buf[0] == 2);
    assert(buf[1] == 3);
    assert(buf[2] == 4);
}

// ============================================================================
// Iterator Tests
// ============================================================================

void test_iterator_traversal() {
    RingBuffer<int, 5> buf;
    
    for (int i = 0; i < 5; ++i) {
        buf.push_back(i);
    }
    
    // Forward iteration
    int expected = 0;
    for (auto it = buf.begin(); it != buf.end(); ++it) {
        assert(*it == expected);
        ++expected;
    }
    assert(expected == 5);
}

void test_iterator_after_wrap() {
    RingBuffer<int, 3> buf;
    
    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(3);
    buf.push_back(4);  // Overwrites 1
    buf.push_back(5);  // Overwrites 2
    
    // Should iterate: 3, 4, 5
    std::vector<int> collected;
    for (const auto& val : buf) {
        collected.push_back(val);
    }
    
    assert(collected.size() == 3);
    assert(collected[0] == 3);
    assert(collected[1] == 4);
    assert(collected[2] == 5);
}

void test_const_iterator() {
    RingBuffer<int, 5> buf;
    
    for (int i = 0; i < 5; ++i) {
        buf.push_back(i);
    }
    
    const auto& cbuf = buf;
    
    int expected = 0;
    for (auto it = cbuf.begin(); it != cbuf.end(); ++it) {
        assert(*it == expected);
        ++expected;
    }
}

void test_iterator_empty_buffer() {
    RingBuffer<int, 5> buf;
    
    assert(buf.begin() == buf.end());
    
    int count = 0;
    for (auto it = buf.begin(); it != buf.end(); ++it) {
        ++count;
    }
    assert(count == 0);
}

void test_range_based_for() {
    RingBuffer<int, 5> buf;
    
    for (int i = 1; i <= 5; ++i) {
        buf.push_back(i * 10);
    }
    
    int sum = 0;
    for (const auto& val : buf) {
        sum += val;
    }
    assert(sum == 150);  // 10 + 20 + 30 + 40 + 50
}

// ============================================================================
// Non-Trivial Type Tests
// ============================================================================

void test_string_type() {
    RingBuffer<std::string, 3> buf;
    
    buf.push_back("hello");
    buf.push_back("world");
    
    assert(buf.front() == "hello");
    assert(buf.back() == "world");
    assert(buf.size() == 2);
    
    buf.push_back("!");
    buf.push_back("test");  // Overwrites "hello"
    
    assert(buf.front() == "world");
    assert(buf[0] == "world");
    assert(buf[1] == "!");
    assert(buf[2] == "test");
}

void test_move_semantics() {
    RingBuffer<std::string, 3> buf;
    
    std::string s1 = "moveable";
    buf.push_back(std::move(s1));
    
    assert(buf.front() == "moveable");
    assert(s1.empty());  // Moved from
}

void test_emplace_back() {
    struct Point {
        int x{}, y{};
        Point() = default;
        Point(int x_, int y_) : x(x_), y(y_) {}
    };
    
    RingBuffer<Point, 3> buf;
    
    buf.emplace_back(1, 2);
    buf.emplace_back(3, 4);
    
    assert(buf.front().x == 1);
    assert(buf.front().y == 2);
    assert(buf.back().x == 3);
    assert(buf.back().y == 4);
}

void test_emplace_back_in_place() {
    struct LargeData {
        int id{0};
        double values[10]{};
        uint64_t timestamp{0};
    };
    
    RingBuffer<LargeData, 5> buf;
    
    // Test 1: Zero-copy write to empty buffer
    auto& slot1 = buf.emplace_back_in_place();
    slot1.id = 42;
    slot1.values[0] = 1.5;
    slot1.values[9] = 9.5;
    slot1.timestamp = 1000;
    
    assert(buf.size() == 1);
    assert(buf.front().id == 42);
    assert(buf.front().values[0] == 1.5);
    assert(buf.front().values[9] == 9.5);
    assert(buf.front().timestamp == 1000);
    
    // Test 2: Multiple zero-copy writes
    auto& slot2 = buf.emplace_back_in_place();
    slot2.id = 43;
    slot2.timestamp = 2000;
    
    auto& slot3 = buf.emplace_back_in_place();
    slot3.id = 44;
    slot3.timestamp = 3000;
    
    assert(buf.size() == 3);
    assert(buf[0].id == 42);
    assert(buf[1].id == 43);
    assert(buf[2].id == 44);
    
    // Test 3: Zero-copy write with overwrite
    buf.push_back(LargeData{});
    buf.push_back(LargeData{});
    assert(buf.size() == 5);
    assert(buf.full());
    
    auto& slot4 = buf.emplace_back_in_place();  // Overwrites oldest
    slot4.id = 100;
    slot4.timestamp = 9999;
    
    assert(buf.size() == 5);  // Still full
    assert(buf.front().id == 43);  // 42 was overwritten
    assert(buf.back().id == 100);
    assert(buf.back().timestamp == 9999);
    
    // Test 4: Verify we can modify returned reference
    auto& slot5 = buf.emplace_back_in_place();
    slot5.id = 200;
    slot5.id += 50;  // Modify in place
    assert(buf.back().id == 250);
}

// ============================================================================
// Constexpr Tests (C++20)
// ============================================================================

constexpr bool test_constexpr_operations() {
    RingBuffer<int, 5> buf;
    
    buf.push_back(42);
    if (buf.front() != 42) return false;
    if (buf.size() != 1) return false;
    
    buf.push_back(7);
    if (buf.back() != 7) return false;
    if (buf.size() != 2) return false;
    
    buf.clear();
    if (!buf.empty()) return false;
    
    return true;
}

static_assert(test_constexpr_operations(), "Constexpr operations failed");

// ============================================================================
// Real-world Use Case Test (CommRaT-style)
// ============================================================================

void test_commrat_use_case() {
    struct TimsMessage {
        uint64_t timestamp;
        uint32_t seq_number;
        int32_t data;
    };
    
    RingBuffer<TimsMessage, 100> buffer;
    
    // Producer: Add messages with increasing timestamps
    for (uint64_t t = 1000; t <= 1010; ++t) {
        TimsMessage msg{t, static_cast<uint32_t>(t - 1000), static_cast<int32_t>(t * 2)};
        buffer.push_back(msg);
    }
    
    assert(buffer.size() == 11);
    assert(buffer.front().timestamp == 1000);
    assert(buffer.back().timestamp == 1010);
    
    // Consumer: Find message closest to target timestamp
    uint64_t target = 1005;
    
    size_t best_idx = 0;
    uint64_t min_diff = UINT64_MAX;
    
    for (size_t i = 0; i < buffer.size(); ++i) {
        uint64_t ts = buffer[i].timestamp;
        uint64_t diff = (ts >= target) ? (ts - target) : (target - ts);
        
        if (diff < min_diff) {
            min_diff = diff;
            best_idx = i;
        }
    }
    
    assert(buffer[best_idx].timestamp == 1005);
    assert(min_diff == 0);
    
    // Test overwrite behavior with 100 more messages
    for (uint64_t t = 1011; t <= 1110; ++t) {
        TimsMessage msg{t, static_cast<uint32_t>(t - 1000), static_cast<int32_t>(t * 2)};
        buffer.push_back(msg);
    }
    
    assert(buffer.size() == 100);  // Buffer full
    assert(buffer.front().timestamp == 1011);  // Oldest messages overwritten
    assert(buffer.back().timestamp == 1110);
}

// ============================================================================
// Performance Characteristics Test
// ============================================================================

void test_performance_characteristics() {
    // Verify O(1) operations through large number of operations
    RingBuffer<int, 1000> buf;
    
    // Fill buffer
    for (int i = 0; i < 1000; ++i) {
        buf.push_back(i);
    }
    
    // Continuous overwrite
    for (int i = 1000; i < 10000; ++i) {
        buf.push_back(i);
        assert(buf.size() == 1000);
        assert(buf.front() == i - 999);
        assert(buf.back() == i);
    }
    
    // Random access
    for (int i = 0; i < 1000; ++i) {
        assert(buf[i] == 9000 + i);
    }
}

// ============================================================================
// Edge Cases
// ============================================================================

void test_single_element_buffer() {
    RingBuffer<int, 1> buf;
    
    buf.push_back(1);
    assert(buf.full());
    assert(buf.front() == 1);
    assert(buf.back() == 1);
    
    buf.push_back(2);  // Overwrites
    assert(buf.size() == 1);
    assert(buf.front() == 2);
}

void test_modify_through_iterator() {
    RingBuffer<int, 5> buf;
    
    for (int i = 0; i < 5; ++i) {
        buf.push_back(i);
    }
    
    // Modify through iterator
    for (auto it = buf.begin(); it != buf.end(); ++it) {
        *it *= 2;
    }
    
    for (size_t i = 0; i < 5; ++i) {
        assert(buf[i] == static_cast<int>(i * 2));
    }
}

void test_modify_through_reference() {
    RingBuffer<int, 5> buf;
    
    buf.push_back(10);
    buf.push_back(20);
    
    buf.front() = 100;
    buf.back() = 200;
    
    assert(buf.front() == 100);
    assert(buf.back() == 200);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "Running RingBuffer tests...\n\n";
    
    // Basic operations
    test_section("basic_construction", test_basic_construction);
    test_section("push_back_and_size", test_push_back_and_size);
    test_section("front_back_access", test_front_back_access);
    test_section("clear", test_clear);
    
    // Wrap-around
    test_section("overwrite_oldest", test_overwrite_oldest);
    test_section("continuous_overwrite", test_continuous_overwrite);
    
    // Indexing
    test_section("operator_bracket", test_operator_bracket);
    test_section("indexing_after_wrap", test_indexing_after_wrap);
    test_section("at_bounds_checking", test_at_bounds_checking);
    
    // Pop operations
    test_section("pop_front", test_pop_front);
    test_section("pop_back", test_pop_back);
    test_section("pop_and_push", test_pop_and_push);
    
    // Iterators
    test_section("iterator_traversal", test_iterator_traversal);
    test_section("iterator_after_wrap", test_iterator_after_wrap);
    test_section("const_iterator", test_const_iterator);
    test_section("iterator_empty_buffer", test_iterator_empty_buffer);
    test_section("range_based_for", test_range_based_for);
    
    // Non-trivial types
    test_section("string_type", test_string_type);
    test_section("move_semantics", test_move_semantics);
    test_section("emplace_back", test_emplace_back);
    test_section("emplace_back_in_place", test_emplace_back_in_place);
    
    // Real-world use case
    test_section("commrat_use_case", test_commrat_use_case);
    
    // Performance
    test_section("performance_characteristics", test_performance_characteristics);
    
    // Edge cases
    test_section("single_element_buffer", test_single_element_buffer);
    test_section("modify_through_iterator", test_modify_through_iterator);
    test_section("modify_through_reference", test_modify_through_reference);
    
    std::cout << "\n========================================\n";
    std::cout << "All tests passed! ✓\n";
    std::cout << "========================================\n";
    
    return 0;
}
