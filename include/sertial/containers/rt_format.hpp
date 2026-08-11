/**
 * @file containers/rt_format.hpp
 * @brief OOB-safe RT-append helpers for sertial::fixed_string.
 *
 * Free functions in namespace sertial::rt that append formatted values into a
 * fixed_string buffer without any heap allocation, exceptions, or glibc
 * floating-point formatting.  Safe to call from EVL out-of-band (OOB) context.
 *
 * All functions are noexcept and use only memcpy, stack-local char arrays and
 * integer arithmetic.  The buffer is silently truncated when full; the string
 * remains null-terminated at all times.
 *
 * Usage:
 * @code
 * sertial::fixed_string<64> msg;
 * sertial::rt::append(msg, "value=");
 * sertial::rt::append(msg, static_cast<int32_t>(-42));
 * sertial::rt::append(msg, ' ');   // use push_back for single chars
 * sertial::rt::append_double<64, 4>(msg, 3.14159);
 * // msg == "value=-42 3.1416"
 * @endcode
 *
 * @see sertial::fixed_string — RT-append is the preferred way to build strings
 *      in OOB / no-exception contexts.
 *
 * @realtime OOB-safe on EVL platform: no throw, no heap, no glibc float.
 * @note All conversion to integer uses a 20-byte stack buffer.
 * @note Float conversion uses fixed-point arithmetic; no modf/frexp/snprintf.
 */
#pragma once

#include "fixed_string.hpp"
#include <cstdint>
#include <cstring>

namespace sertial::rt {

// ============================================================================
// Core: bounded memcpy — never throws, never allocates, always null-terminates
// ============================================================================

/**
 * @brief Append up to @p len bytes from @p src into @p dst, silently truncating
 *        when the buffer is full.
 *
 * @realtime OOB-safe: memcpy only.
 */
template<std::size_t N>
void append(fixed_string<N>& dst, const char* src, std::size_t len) noexcept {
    // Reserve 1 byte for null terminator: usable capacity = N - 1
    const std::size_t avail = (N > dst.size() + 1u) ? (N - 1u - dst.size()) : 0u;
    const std::size_t copy  = len < avail ? len : avail;
    if (copy == 0u) return;
    std::memcpy(dst.data_unsafe() + dst.size(), src, copy);
    dst.set_size_unsafe(dst.size() + copy);
}

/**
 * @brief Append a null-terminated C-string into @p dst, silently truncating
 *        when the buffer is full.
 *
 * @realtime OOB-safe: no strlen equivalent calls glibc; manual scan instead.
 */
template<std::size_t N>
void append(fixed_string<N>& dst, const char* cstr) noexcept {
    if (!cstr) return;
    std::size_t len = 0u;
    while (cstr[len]) ++len;
    append(dst, cstr, len);
}

// ============================================================================
// Unsigned integers — reverse-digit trick, 20-byte stack buffer
// ============================================================================

/**
 * @brief Append the decimal representation of a uint64_t.
 * @realtime OOB-safe.
 */
template<std::size_t N>
void append(fixed_string<N>& dst, uint64_t v) noexcept {
    char tmp[20];
    int  i = 20;
    do {
        tmp[--i] = static_cast<char>('0' + v % 10u);
        v /= 10u;
    } while (v);
    append(dst, tmp + i, static_cast<std::size_t>(20 - i));
}

/**
 * @brief Append the decimal representation of a uint32_t.
 * @realtime OOB-safe.
 */
template<std::size_t N>
void append(fixed_string<N>& dst, uint32_t v) noexcept {
    append(dst, static_cast<uint64_t>(v));
}

// ============================================================================
// Signed integers
// ============================================================================

/**
 * @brief Append the decimal representation of an int64_t (handles INT64_MIN safely).
 * @realtime OOB-safe.
 */
template<std::size_t N>
void append(fixed_string<N>& dst, int64_t v) noexcept {
    if (v < 0) {
        append(dst, "-", 1u);
        // INT64_MIN negation would overflow; cast to unsigned first
        append(dst, static_cast<uint64_t>(-static_cast<uint64_t>(v)));
    } else {
        append(dst, static_cast<uint64_t>(v));
    }
}

/**
 * @brief Append the decimal representation of an int32_t.
 * @realtime OOB-safe.
 */
template<std::size_t N>
void append(fixed_string<N>& dst, int32_t v) noexcept {
    append(dst, static_cast<int64_t>(v));
}

// ============================================================================
// Boolean
// ============================================================================

/**
 * @brief Append "true" or "false".
 * @realtime OOB-safe.
 */
template<std::size_t N>
void append(fixed_string<N>& dst, bool v) noexcept {
    if (v) append(dst, "true",  4u);
    else   append(dst, "false", 5u);
}

// ============================================================================
// Floating point — fixed-point, no glibc, OOB-safe
//
// Renders as: [-]<integer>.<Decimals digits>
//   e.g. append_double<64, 3>(dst, 3.14159) → "3.142"
//
// Special values:
//   NaN  → "nan"
//   +Inf → "inf"
//   -Inf → "-inf"
//
// NaN detection:  v != v  (bit comparison, no glibc)
// Inf detection:  magnitude exceeds DBL_MAX  (no std::isinf)
// Split:          integer cast + subtract (no modf/frexp)
// Rounding:       add 0.5 * ulp at last digit position
// ============================================================================

namespace detail {

/// Compile-time power-of-10 table for fractional-digit scaling (0..9 digits).
inline constexpr uint64_t pow10_table[10] = {
    1u,
    10u,
    100u,
    1'000u,
    10'000u,
    100'000u,
    1'000'000u,
    10'000'000u,
    100'000'000u,
    1'000'000'000u
};

/// OOB-safe NaN check: exploits the IEEE 754 property that NaN != NaN.
inline bool rt_isnan(double v) noexcept { return v != v; }

/// OOB-safe infinity check: any finite double satisfies |v| <= DBL_MAX.
/// 1.7976931348623157e+308 == DBL_MAX.
inline bool rt_isinf(double v) noexcept {
    return !rt_isnan(v) &&
           (v > 1.7976931348623157e+308 || v < -1.7976931348623157e+308);
}

} // namespace detail

/**
 * @brief Append a double in fixed-point notation with @p Decimals decimal places.
 *
 * @tparam N    Capacity of the destination fixed_string.
 * @tparam Decimals  Number of fractional digits to render (0..9).
 *
 * Uses only integer arithmetic after splitting the value into its integer and
 * fractional parts.  No glibc floating-point functions are called.
 *
 * @realtime OOB-safe: no throw, no heap, no glibc float formatting.
 */
template<std::size_t N, int Decimals = 3>
void append_double(fixed_string<N>& dst, double v) noexcept {
    static_assert(Decimals >= 0 && Decimals <= 9,
                  "sertial::rt::append_double: Decimals must be in [0, 9]");

    if (detail::rt_isnan(v)) {
        append(dst, "nan", 3u);
        return;
    }
    if (detail::rt_isinf(v)) {
        if (v < 0.0) append(dst, "-inf", 4u);
        else         append(dst, "inf",  3u);
        return;
    }

    if (v < 0.0) {
        append(dst, "-", 1u);
        v = -v;
    }

    // Integer part
    const uint64_t int_part = static_cast<uint64_t>(v);
    append(dst, int_part);

    if constexpr (Decimals > 0) {
        append(dst, ".", 1u);

        const uint64_t scale = detail::pow10_table[Decimals];
        const double   frac  = v - static_cast<double>(int_part);

        // Round to nearest at the Decimals-th fractional place
        uint64_t frac_int =
            static_cast<uint64_t>(frac * static_cast<double>(scale) + 0.5);

        // Guard against rounding overflow: 0.9995 with Decimals=3 → frac_int==1000
        if (frac_int >= scale) frac_int = scale - 1u;

        // Write exactly Decimals digits, with leading zeros
        char tmp[10];
        for (int i = Decimals - 1; i >= 0; --i) {
            tmp[i] = static_cast<char>('0' + frac_int % 10u);
            frac_int /= 10u;
        }
        append(dst, tmp, static_cast<std::size_t>(Decimals));
    }
}

/**
 * @brief Append a double with 3 decimal places.
 *
 * Delegates to append_double<N, 3>.  Use append_double<N, D> directly for
 * a different number of fractional digits.
 *
 * @realtime OOB-safe.
 */
template<std::size_t N>
void append(fixed_string<N>& dst, double v) noexcept {
    append_double<N, 3>(dst, v);
}

/**
 * @brief Append a float with 3 decimal places (via static_cast to double).
 * @realtime OOB-safe.
 */
template<std::size_t N>
void append(fixed_string<N>& dst, float v) noexcept {
    append_double<N, 3>(dst, static_cast<double>(v));
}

// ============================================================================
// Hex output
// ============================================================================

/**
 * @brief Append a uint64_t in uppercase hexadecimal, optionally prefixed "0x".
 *
 * Leading zeros are suppressed (e.g. 255 → "FF", not "00000000000000FF").
 *
 * @param prefix  When true, prepends "0x" before the hex digits.
 * @realtime OOB-safe.
 */
template<std::size_t N>
void append_hex(fixed_string<N>& dst, uint64_t v, bool prefix = false) noexcept {
    static constexpr char kHex[] = "0123456789ABCDEF";
    if (prefix) append(dst, "0x", 2u);
    char tmp[16];
    int  i = 16;
    do {
        tmp[--i] = kHex[v & 0xFu];
        v >>= 4u;
    } while (v);
    append(dst, tmp + i, static_cast<std::size_t>(16 - i));
}

/**
 * @brief Append a uint32_t in uppercase hexadecimal, optionally prefixed "0x".
 * @realtime OOB-safe.
 */
template<std::size_t N>
void append_hex(fixed_string<N>& dst, uint32_t v, bool prefix = false) noexcept {
    append_hex(dst, static_cast<uint64_t>(v), prefix);
}

} // namespace sertial::rt
