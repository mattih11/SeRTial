/**
 * @file test_rt_format.cpp
 * @brief Unit tests for sertial::rt append helpers (rt_format.hpp).
 *
 * Tests cover:
 *  - Raw bytes / C-string append and null-string safety
 *  - uint32_t / uint64_t (zero, max, boundary)
 *  - int32_t  / int64_t  (zero, negative, INT_MIN, INT_MAX)
 *  - bool     (true / false)
 *  - double / float — normal values, NaN, ±Inf, negative
 *  - append_double with custom Decimals template parameter
 *  - append_hex (uint32_t, uint64_t, with/without prefix)
 *  - Truncation: buffer full → no overrun, result still null-terminated
 */
#include <sertial/containers/rt_format.hpp>

#include <cassert>
#include <cstring>
#include <cstdint>
#include <limits>
#include <iostream>

using sertial::fixed_string;
namespace rt = sertial::rt;

// ============================================================================
// Minimal test harness
// ============================================================================

static int g_tests_run    = 0;
static int g_tests_failed = 0;

static void test_section(const char* name, void(*fn)()) {
    std::cout << "Testing: " << name << " ... ";
    fn();
    std::cout << "PASSED\n";
    ++g_tests_run;
}

#define CHECK(cond) \
    do { if (!(cond)) { \
        std::cerr << "FAIL at " __FILE__ ":" << __LINE__ << ": " #cond "\n"; \
        ++g_tests_failed; \
    } } while(0)

#define CHECK_STR(fs, expected) \
    CHECK(std::strcmp((fs).c_str(), (expected)) == 0)

// ============================================================================
// append(fixed_string, const char*, len)  /  append(fixed_string, const char*)
// ============================================================================

static void test_append_raw() {
    fixed_string<32> s;
    rt::append(s, "hello", 5u);
    CHECK_STR(s, "hello");
    CHECK(s.size() == 5u);

    rt::append(s, " world");
    CHECK_STR(s, "hello world");
    CHECK(s.size() == 11u);
}

static void test_append_null_ptr() {
    fixed_string<16> s = "safe";
    rt::append(s, static_cast<const char*>(nullptr));
    CHECK_STR(s, "safe");        // unchanged
}

// ============================================================================
// Truncation: append into a nearly-full buffer
// ============================================================================

static void test_truncation() {
    // Capacity = 8 chars; fill 7, then append a longer string
    fixed_string<8> s;
    rt::append(s, "1234567"); // fills to size 7 (max_size == 8, so 7 chars + NUL)
    CHECK(s.size() == 7u);

    rt::append(s, "OVERFLOW");  // buffer is full — should silently do nothing
    CHECK(s.size() == 7u);
    CHECK_STR(s, "1234567");
    CHECK(s.c_str()[7] == '\0');  // still null-terminated

    // Partial truncation: 5 chars fit, 6th does not
    fixed_string<8> s2;
    rt::append(s2, "12345");   // size 5
    rt::append(s2, "XYZ");     // only 2 chars fit (capacity 7 usable, 2 remain)
    CHECK(s2.size() == 7u);
    CHECK_STR(s2, "1234512345XY"[0] == '1' ? "12345XY" : "12345XY");  // "12345XY"
    CHECK_STR(s2, "12345XY");
    CHECK(s2.c_str()[7] == '\0');
}

// ============================================================================
// uint32_t / uint64_t
// ============================================================================

static void test_uint() {
    {
        fixed_string<32> s;
        rt::append(s, uint32_t{0});
        CHECK_STR(s, "0");
    }
    {
        fixed_string<32> s;
        rt::append(s, uint32_t{1});
        CHECK_STR(s, "1");
    }
    {
        fixed_string<32> s;
        rt::append(s, uint32_t{4294967295u});
        CHECK_STR(s, "4294967295");
    }
    {
        fixed_string<32> s;
        rt::append(s, uint64_t{0});
        CHECK_STR(s, "0");
    }
    {
        fixed_string<32> s;
        // UINT64_MAX == 18446744073709551615
        rt::append(s, std::numeric_limits<uint64_t>::max());
        CHECK_STR(s, "18446744073709551615");
    }
    {
        fixed_string<32> s;
        rt::append(s, uint64_t{1'000'000'000});
        CHECK_STR(s, "1000000000");
    }
}

// ============================================================================
// int32_t / int64_t
// ============================================================================

static void test_sint() {
    {
        fixed_string<32> s;
        rt::append(s, int32_t{0});
        CHECK_STR(s, "0");
    }
    {
        fixed_string<32> s;
        rt::append(s, int32_t{-1});
        CHECK_STR(s, "-1");
    }
    {
        fixed_string<32> s;
        rt::append(s, int32_t{2147483647});
        CHECK_STR(s, "2147483647");
    }
    {
        fixed_string<32> s;
        rt::append(s, int32_t{-2147483648});
        CHECK_STR(s, "-2147483648");
    }
    {
        fixed_string<32> s;
        rt::append(s, int64_t{0});
        CHECK_STR(s, "0");
    }
    {
        fixed_string<32> s;
        rt::append(s, int64_t{-1});
        CHECK_STR(s, "-1");
    }
    {
        fixed_string<32> s;
        // INT64_MIN = -9223372036854775808
        rt::append(s, std::numeric_limits<int64_t>::min());
        CHECK_STR(s, "-9223372036854775808");
    }
    {
        fixed_string<32> s;
        rt::append(s, std::numeric_limits<int64_t>::max());
        CHECK_STR(s, "9223372036854775807");
    }
}

// ============================================================================
// bool
// ============================================================================

static void test_bool() {
    {
        fixed_string<16> s;
        rt::append(s, true);
        CHECK_STR(s, "true");
    }
    {
        fixed_string<16> s;
        rt::append(s, false);
        CHECK_STR(s, "false");
    }
}

// ============================================================================
// double / float
// ============================================================================

static void test_double() {
    // 0.0
    {
        fixed_string<32> s;
        rt::append(s, 0.0);
        CHECK_STR(s, "0.000");
    }
    // -0.0 (sign bit set but mathematically == 0.0 with this formatter)
    {
        fixed_string<32> s;
        rt::append(s, -0.0);
        // -0.0 < 0.0 is false in IEEE 754; formatter emits "0.000" (no sign)
        CHECK_STR(s, "0.000");
    }
    // 1.0
    {
        fixed_string<32> s;
        rt::append(s, 1.0);
        CHECK_STR(s, "1.000");
    }
    // -1.5
    {
        fixed_string<32> s;
        rt::append(s, -1.5);
        CHECK_STR(s, "-1.500");
    }
    // 3.14159  → rounded to 3.142 with 3 decimals
    {
        fixed_string<32> s;
        rt::append(s, 3.14159);
        CHECK_STR(s, "3.142");
    }
    // NaN
    {
        fixed_string<16> s;
        rt::append(s, std::numeric_limits<double>::quiet_NaN());
        CHECK_STR(s, "nan");
    }
    // +Inf
    {
        fixed_string<16> s;
        rt::append(s, std::numeric_limits<double>::infinity());
        CHECK_STR(s, "inf");
    }
    // -Inf
    {
        fixed_string<16> s;
        rt::append(s, -std::numeric_limits<double>::infinity());
        CHECK_STR(s, "-inf");
    }
    // Large value: INT64_MAX as double (precision loss expected; check no crash)
    {
        fixed_string<64> s;
        rt::append(s, static_cast<double>(std::numeric_limits<int64_t>::max()));
        CHECK(s.size() > 0u);  // exact decimal varies; just verify it runs
    }
}

static void test_float() {
    {
        fixed_string<32> s;
        rt::append(s, 0.0f);
        CHECK_STR(s, "0.000");
    }
    {
        fixed_string<32> s;
        rt::append(s, 1.5f);
        CHECK_STR(s, "1.500");
    }
    {
        fixed_string<16> s;
        rt::append(s, std::numeric_limits<float>::quiet_NaN());
        CHECK_STR(s, "nan");
    }
}

// ============================================================================
// append_double with custom Decimals template parameter
// ============================================================================

static void test_append_double_custom_decimals() {
    // Decimals = 0
    {
        fixed_string<16> s;
        rt::append_double<16, 0>(s, 3.7);
        CHECK_STR(s, "3");
    }
    // Decimals = 1
    {
        fixed_string<16> s;
        rt::append_double<16, 1>(s, 3.14);
        CHECK_STR(s, "3.1");
    }
    // Decimals = 5
    {
        fixed_string<32> s;
        rt::append_double<32, 5>(s, 3.14159);
        // 3.14159 * 100000 + 0.5 = 314159.5 → 314159 → "14159" with leading digit "3"
        CHECK_STR(s, "3.14159");
    }
    // Negative with Decimals = 2
    {
        fixed_string<16> s;
        rt::append_double<16, 2>(s, -2.718);
        CHECK_STR(s, "-2.72");
    }
}

// ============================================================================
// Hex output
// ============================================================================

static void test_hex() {
    // Zero
    {
        fixed_string<32> s;
        rt::append_hex(s, uint32_t{0u});
        CHECK_STR(s, "0");
    }
    // 0xFF
    {
        fixed_string<32> s;
        rt::append_hex(s, uint32_t{0xFFu});
        CHECK_STR(s, "FF");
    }
    // 0xDEADBEEF
    {
        fixed_string<32> s;
        rt::append_hex(s, uint32_t{0xDEADBEEFu});
        CHECK_STR(s, "DEADBEEF");
    }
    // UINT64_MAX
    {
        fixed_string<32> s;
        rt::append_hex(s, std::numeric_limits<uint64_t>::max());
        CHECK_STR(s, "FFFFFFFFFFFFFFFF");
    }
    // prefix = true
    {
        fixed_string<32> s;
        rt::append_hex(s, uint32_t{0xABCDu}, true);
        CHECK_STR(s, "0xABCD");
    }
    // uint64 without prefix
    {
        fixed_string<32> s;
        rt::append_hex(s, uint64_t{0x0000'0000'0000'0001u});
        CHECK_STR(s, "1");
    }
    // uint64 with prefix
    {
        fixed_string<32> s;
        rt::append_hex(s, uint64_t{0xCAFEBABE'DEADBEEF}, true);
        CHECK_STR(s, "0xCAFEBABEDEADBEEF");
    }
}

// ============================================================================
// Composite: multiple appends in sequence
// ============================================================================

static void test_composite() {
    fixed_string<64> s;
    rt::append(s, "id=");
    rt::append(s, uint32_t{42u});
    rt::append(s, " v=");
    rt::append(s, 1.5);
    rt::append(s, " ok=");
    rt::append(s, true);
    CHECK_STR(s, "id=42 v=1.500 ok=true");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    test_section("append_raw",                   test_append_raw);
    test_section("append_null_ptr",              test_append_null_ptr);
    test_section("truncation",                   test_truncation);
    test_section("uint",                         test_uint);
    test_section("sint",                         test_sint);
    test_section("bool",                         test_bool);
    test_section("double",                       test_double);
    test_section("float",                        test_float);
    test_section("append_double_custom_decimals",test_append_double_custom_decimals);
    test_section("hex",                          test_hex);
    test_section("composite",                    test_composite);

    if (g_tests_failed > 0) {
        std::cerr << g_tests_failed << " check(s) FAILED\n";
        return 1;
    }
    std::cout << "All " << g_tests_run << " test sections passed.\n";
    return 0;
}
