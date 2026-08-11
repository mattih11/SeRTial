#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <rfl.hpp>

namespace sertial {

/// @brief Fixed-capacity string with zero heap allocation and compile-time construction
/// 
/// A stack-allocated string class with compile-time size deduction and constexpr support.
/// Unlike std::string, fixed_string never allocates heap memory and can be fully constructed
/// at compile time from string literals.
/// 
/// @tparam MaxSize Maximum string length (excluding null terminator)
/// 
/// @par Compile-Time Construction:
/// @code
/// // CTAD (Class Template Argument Deduction)
/// constexpr fixed_string name{"SeRTial"};  // Deduces fixed_string<7>
/// 
/// // Helper functions
/// constexpr auto str1 = fixed_string_literal("Hello");  // fixed_string<5>
/// constexpr auto str2 = make_fixed<"World">();          // fixed_string<5> (C++20 NTTP)
/// 
/// // User-defined literal
/// using namespace sertial::literals;
/// auto str3 = "RealTime"_fs;  // fixed_string<8>
/// 
/// // reflect-cpp integration (C++20 NTTP)
/// constexpr auto str4 = make_fixed<rfl::internal::StringLiteral{"Config"}>();  // fixed_string<6>
/// @endcode
/// 
/// @par Runtime Construction:
/// @code
/// fixed_string<32> msg = "Hello";
/// msg += " World";
/// std::cout << msg;  // "Hello World"
/// @endcode
/// 
/// @par string_view Integration:
/// @code
/// constexpr fixed_string<16> data{"Embedded"};
/// constexpr std::string_view view = data;  // Implicit conversion
/// @endcode
/// 
/// @note All constructors and operations use std::copy_n or manual loops (not memcpy)
///       for full constexpr compatibility in C++20.
/// @note MaxSize does NOT include the null terminator (internal array is MaxSize + 1)
/// @note Capacity is fixed at compile time - attempting to exceed it throws std::length_error
/// 
/// @wireformat Serialized as: [length:4 bytes][characters:length bytes] (no null terminator)
/// @realtime Zero heap allocation, bounded execution time (O(n) where n ≤ MaxSize)
/// @compiletime Size deduction, type safety, constexpr construction fully supported
///
/// @see sertial::rt::append — preferred API for building strings in OOB /
///      no-exception contexts (integers, floats, hex, bool; no glibc, no throw).
template<std::size_t MaxSize>
class fixed_string {
public:
    using size_type = std::size_t;
    using value_type = char;
    using pointer = char*;
    using const_pointer = const char*;
    using reference = char&;
    using const_reference = const char&;
    using iterator = char*;
    using const_iterator = const char*;
    
    static constexpr size_type max_size_v = MaxSize;
    
    // ========================================================================
    // Construction
    // ========================================================================
    
    /// @brief Default constructor - creates empty string
    constexpr fixed_string() noexcept : size_(0) {
        data_[0] = '\0';
    }
    
    /// @brief Construct from C-string (null-terminated)
    /// @param str C-string to copy (must not exceed MaxSize)
    /// @throws std::length_error if string length exceeds MaxSize
    /// @note Fully constexpr - uses manual loop instead of memcpy
    constexpr fixed_string(const char* str) : size_(0) {
        if (str == nullptr) {
            data_[0] = '\0';
            return;
        }
        
        // Use manual loop for constexpr compatibility
        size_type i = 0;
        while (str[i] != '\0' && i < MaxSize) {
            data_[i] = str[i];
            ++i;
        }
        
        if (str[i] != '\0') {
            throw std::length_error("fixed_string: string exceeds max_size");
        }
        
        size_ = i;
        data_[size_] = '\0';
    }
    
    /// @brief Construct from string_view
    /// @param sv String view to copy (must not exceed MaxSize)
    /// @throws std::length_error if string_view size exceeds MaxSize
    /// @note Fully constexpr - uses std::copy_n for compatibility
    constexpr fixed_string(std::string_view sv) : size_(0) {
        if (sv.size() >= MaxSize) {
            throw std::length_error("fixed_string: string_view exceeds max_size");
        }
        
        // Use std::copy_n for constexpr compatibility
        std::copy_n(sv.data(), sv.size(), data_);
        size_ = sv.size();
        data_[size_] = '\0';
    }
    
    /// @brief Construct from std::string
    /// @param str String to copy (must not exceed MaxSize)
    /// @throws std::length_error if string size exceeds MaxSize
    constexpr fixed_string(const std::string& str) : fixed_string(std::string_view(str)) {}
    
    /// @brief Compile-time constructor from string literal with auto size deduction
    /// 
    /// Enables Class Template Argument Deduction (CTAD) for automatic size inference:
    /// @code
    /// constexpr fixed_string name{"SeRTial"};  // Deduces fixed_string<7>
    /// @endcode
    /// 
    /// @tparam N Size of string literal (including null terminator)
    /// @param str String literal (compile-time constant)
    /// @note This is the preferred way to create compile-time fixed_strings
    /// @note MaxSize is deduced as N-1 (excluding null terminator)
    template<std::size_t N>
    constexpr fixed_string(const char (&str)[N]) : size_(0) {
        static_assert(N <= MaxSize + 1, "String literal exceeds fixed_string capacity");
        constexpr size_type len = N - 1;  // Exclude null terminator
        std::copy_n(str, len, data_);
        size_ = len;
        data_[size_] = '\0';
    }
    
    /// @brief Compile-time constructor from rfl::internal::StringLiteral (reflect-cpp integration)
    /// 
    /// Enables integration with reflect-cpp's compile-time string literals:
    /// @code
    /// // Direct construction:
    /// constexpr fixed_string<10> name{rfl::internal::StringLiteral{"field_name"}};
    /// // Via make_fixed helper:
    /// constexpr auto id = make_fixed<rfl::internal::StringLiteral{"sensor_id"}>();
    /// @endcode
    /// 
    /// @tparam N Size of StringLiteral (including null terminator)
    /// @param lit rfl::internal::StringLiteral instance
    template<std::size_t N>
    constexpr fixed_string(const rfl::internal::StringLiteral<N>& lit) : size_(0) {
        // StringLiteral::length excludes null terminator
        constexpr std::size_t len = N - 1;
        static_assert(len <= MaxSize, "rfl::internal::StringLiteral string exceeds fixed_string capacity");
        // Use string_view() which already excludes null terminator
        auto sv = lit.string_view();
        std::copy_n(sv.data(), sv.size(), data_);
        size_ = sv.size();
        data_[size_] = '\0';
    }
    
    /// @brief Construct string filled with repeated character
    /// @param count Number of characters (must not exceed MaxSize)
    /// @param ch Character to repeat
    /// @throws std::length_error if count exceeds MaxSize
    constexpr fixed_string(size_type count, char ch) : size_(0) {
        if (count >= MaxSize) {
            throw std::length_error("fixed_string: count exceeds max_size");
        }
        
        std::fill_n(data_, count, ch);
        size_ = count;
        data_[size_] = '\0';
    }
    
    // ========================================================================
    // Capacity
    // ========================================================================
    
    [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
    
    [[nodiscard]] constexpr size_type length() const noexcept { return size_; }
    
    [[nodiscard]] constexpr size_type max_size() const noexcept { return MaxSize; }
    
    [[nodiscard]] constexpr size_type capacity() const noexcept { return MaxSize; }
    
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
    
    [[nodiscard]] constexpr bool full() const noexcept { return size_ >= MaxSize - 1; }
    
    // ========================================================================
    // Element Access
    // ========================================================================
    
    constexpr reference operator[](size_type index) noexcept {
        assert(index < size_ && "fixed_string: index out of bounds");
        return data_[index];
    }
    
    constexpr const_reference operator[](size_type index) const noexcept {
        assert(index < size_ && "fixed_string: index out of bounds");
        return data_[index];
    }
    
    constexpr reference at(size_type index) {
        if (index >= size_) {
            throw std::out_of_range("fixed_string: index out of bounds");
        }
        return data_[index];
    }
    
    constexpr const_reference at(size_type index) const {
        if (index >= size_) {
            throw std::out_of_range("fixed_string: index out of bounds");
        }
        return data_[index];
    }
    
    constexpr reference front() noexcept {
        assert(!empty() && "fixed_string: front() on empty string");
        return data_[0];
    }
    
    constexpr const_reference front() const noexcept {
        assert(!empty() && "fixed_string: front() on empty string");
        return data_[0];
    }
    
    constexpr reference back() noexcept {
        assert(!empty() && "fixed_string: back() on empty string");
        return data_[size_ - 1];
    }
    
    constexpr const_reference back() const noexcept {
        assert(!empty() && "fixed_string: back() on empty string");
        return data_[size_ - 1];
    }
    
    constexpr const_pointer data() const noexcept { return data_; }
    
    constexpr const_pointer c_str() const noexcept { return data_; }
    
    // ========================================================================
    // Modifiers
    // ========================================================================
    
    constexpr void assign(const char* str) {
        if (str == nullptr) {
            clear();
            return;
        }
        
        // Use manual loop for constexpr compatibility
        size_type i = 0;
        while (str[i] != '\0' && i < MaxSize) {
            data_[i] = str[i];
            ++i;
        }
        
        if (str[i] != '\0') {
            throw std::length_error("fixed_string: assign exceeds max_size");
        }
        
        size_ = i;
        data_[size_] = '\0';
    }
    
    constexpr void assign(std::string_view sv) {
        if (sv.size() >= MaxSize) {
            throw std::length_error("fixed_string: assign exceeds max_size");
        }
        
        std::copy_n(sv.data(), sv.size(), data_);
        size_ = sv.size();
        data_[size_] = '\0';
    }
    
    constexpr void push_back(char ch) {
        if (size_ >= MaxSize) {
            throw std::length_error("fixed_string: push_back on full string");
        }
        data_[size_++] = ch;
        data_[size_] = '\0';
    }
    
    constexpr void pop_back() noexcept {
        assert(!empty() && "fixed_string: pop_back() on empty string");
        --size_;
        data_[size_] = '\0';
    }
    
    constexpr void clear() noexcept {
        size_ = 0;
        data_[0] = '\0';
    }
    
    constexpr void resize(size_type new_size, char ch = '\0') {
        if (new_size >= MaxSize) {
            throw std::length_error("fixed_string: resize exceeds max_size");
        }
        
        if (new_size > size_) {
            std::fill(data_ + size_, data_ + new_size, ch);
        }
        size_ = new_size;
        data_[size_] = '\0';
    }
    
    constexpr fixed_string& append(const char* str) {
        if (str == nullptr) return *this;
        
        // Use manual loop for constexpr compatibility
        size_type i = 0;
        while (str[i] != '\0' && size_ + i < MaxSize) {
            data_[size_ + i] = str[i];
            ++i;
        }
        
        if (str[i] != '\0') {
            throw std::length_error("fixed_string: append exceeds max_size");
        }
        
        size_ += i;
        data_[size_] = '\0';
        return *this;
    }
    
    constexpr fixed_string& append(std::string_view sv) {
        if (size_ + sv.size() > MaxSize) {
            throw std::length_error("fixed_string: append exceeds max_size");
        }
        
        std::copy_n(sv.data(), sv.size(), data_ + size_);
        size_ += sv.size();
        data_[size_] = '\0';
        return *this;
    }
    
    constexpr fixed_string& operator+=(const char* str) {
        return append(str);
    }
    
    constexpr fixed_string& operator+=(std::string_view sv) {
        return append(sv);
    }
    
    constexpr fixed_string& operator+=(char ch) {
        push_back(ch);
        return *this;
    }
    
    // ========================================================================
    // Conversion
    // ========================================================================
    
    /// @brief Implicit conversion to string_view
    /// @return Non-owning view of the string data
    /// @note Constexpr-compatible, zero-cost abstraction
    constexpr operator std::string_view() const noexcept {
        return std::string_view(data_, size_);
    }
    
    [[nodiscard]] std::string to_string() const {
        return std::string(data_, size_);
    }
    
    [[nodiscard]] std::string str() const {
        return to_string();
    }
    
    // ========================================================================
    // Iterators
    // ========================================================================
    
    constexpr iterator begin() noexcept { return data_; }
    constexpr iterator end() noexcept { return data_ + size_; }
    constexpr const_iterator begin() const noexcept { return data_; }
    constexpr const_iterator end() const noexcept { return data_ + size_; }
    constexpr const_iterator cbegin() const noexcept { return data_; }
    constexpr const_iterator cend() const noexcept { return data_ + size_; }
    
    // ========================================================================
    // String Operations
    // ========================================================================
    
    [[nodiscard]] constexpr int compare(std::string_view sv) const noexcept {
        return std::string_view(*this).compare(sv);
    }
    
    [[nodiscard]] constexpr bool starts_with(std::string_view sv) const noexcept {
        return std::string_view(*this).starts_with(sv);
    }
    
    [[nodiscard]] constexpr bool ends_with(std::string_view sv) const noexcept {
        return std::string_view(*this).ends_with(sv);
    }
    
    [[nodiscard]] constexpr bool contains(std::string_view sv) const noexcept {
        return std::string_view(*this).find(sv) != std::string_view::npos;
    }
    
    // ========================================================================
    // Internal API (for BinaryReflector)
    // ========================================================================
    
    /// @brief Unsafe direct size setter (for deserialization)
    /// @warning Caller must ensure data is properly initialized and null-terminated!
    constexpr void set_size_unsafe(size_type new_size) noexcept {
        assert(new_size < MaxSize && "fixed_string: set_size_unsafe exceeds max_size");
        size_ = new_size;
        data_[size_] = '\0';
    }
    
    /// @brief Unsafe direct data access (for deserialization)
    constexpr pointer data_unsafe() noexcept { return data_; }
    
private:
    /// @brief Internal character storage
    /// @note Array size is MaxSize + 1 to accommodate null terminator
    /// @note MaxSize represents usable capacity (excluding null terminator)
    char data_[MaxSize + 1];  // +1 for null terminator
    
    /// @brief Current string length (excluding null terminator)
    size_type size_;
};

// ============================================================================
// Comparison Operators
// ============================================================================

template<std::size_t N>
constexpr bool operator==(const fixed_string<N>& lhs, const fixed_string<N>& rhs) noexcept {
    return std::string_view(lhs) == std::string_view(rhs);
}

template<std::size_t N>
constexpr bool operator==(const fixed_string<N>& lhs, std::string_view rhs) noexcept {
    return std::string_view(lhs) == rhs;
}

template<std::size_t N>
constexpr bool operator==(std::string_view lhs, const fixed_string<N>& rhs) noexcept {
    return lhs == std::string_view(rhs);
}

template<std::size_t N>
constexpr bool operator!=(const fixed_string<N>& lhs, const fixed_string<N>& rhs) noexcept {
    return !(lhs == rhs);
}

template<std::size_t N>
constexpr bool operator!=(const fixed_string<N>& lhs, std::string_view rhs) noexcept {
    return !(lhs == rhs);
}

template<std::size_t N>
constexpr bool operator!=(std::string_view lhs, const fixed_string<N>& rhs) noexcept {
    return !(lhs == rhs);
}

template<std::size_t N>
constexpr bool operator<(const fixed_string<N>& lhs, const fixed_string<N>& rhs) noexcept {
    return std::string_view(lhs) < std::string_view(rhs);
}

template<std::size_t N>
constexpr bool operator<=(const fixed_string<N>& lhs, const fixed_string<N>& rhs) noexcept {
    return std::string_view(lhs) <= std::string_view(rhs);
}

template<std::size_t N>
constexpr bool operator>(const fixed_string<N>& lhs, const fixed_string<N>& rhs) noexcept {
    return std::string_view(lhs) > std::string_view(rhs);
}

template<std::size_t N>
constexpr bool operator>=(const fixed_string<N>& lhs, const fixed_string<N>& rhs) noexcept {
    return std::string_view(lhs) >= std::string_view(rhs);
}

// ============================================================================
// Concatenation Operators
// ============================================================================

/// @brief Concatenate two fixed_strings with compile-time size deduction
/// 
/// Creates a new fixed_string with capacity equal to the sum of both strings.
/// Enables natural concatenation syntax:
/// @code
/// constexpr fixed_string<5> hello{"Hello"};
/// constexpr fixed_string<6> world{" World"};
/// constexpr auto result = hello + world;  // fixed_string<11>
/// @endcode
/// 
/// @tparam N1 Capacity of left string
/// @tparam N2 Capacity of right string
/// @param lhs Left operand
/// @param rhs Right operand
/// @return fixed_string<N1 + N2> containing concatenated result
/// @note Result capacity is sum of capacities, not sum of sizes
/// @note Fully constexpr-compatible
template<std::size_t N1, std::size_t N2>
constexpr auto operator+(const fixed_string<N1>& lhs, const fixed_string<N2>& rhs) {
    fixed_string<N1 + N2> result;
    result.append(std::string_view(lhs));
    result.append(std::string_view(rhs));
    return result;
}

/// @brief Concatenate fixed_string with string_view (runtime only)
/// 
/// @code
/// fixed_string<5> hello{"Hello"};
/// std::string_view world = " World";
/// auto result = hello + world;  // throws if combined size exceeds lhs capacity
/// @endcode
/// 
/// @tparam N Capacity of fixed_string
/// @param lhs fixed_string operand
/// @param rhs string_view operand (size unknown at compile time)
/// @return fixed_string<N> - reuses lhs capacity, throws if overflow
/// @throws std::length_error if lhs.size() + rhs.size() > N
/// @note string_view has no compile-time size, so result uses lhs capacity
template<std::size_t N>
auto operator+(const fixed_string<N>& lhs, std::string_view rhs) {
    fixed_string<N> result{lhs};
    result.append(rhs);  // Will throw if doesn't fit
    return result;
}

/// @brief Concatenate string_view with fixed_string (runtime only)
/// 
/// @tparam N Capacity of fixed_string
/// @param lhs string_view operand (size unknown at compile time)
/// @param rhs fixed_string operand
/// @return fixed_string<N> - reuses rhs capacity, throws if overflow
/// @throws std::length_error if lhs.size() + rhs.size() > N
/// @note string_view has no compile-time size, so result uses rhs capacity
template<std::size_t N>
auto operator+(std::string_view lhs, const fixed_string<N>& rhs) {
    fixed_string<N> result;
    result.append(lhs);
    result.append(std::string_view(rhs));  // Will throw if doesn't fit
    return result;
}

/// @brief Concatenate fixed_string with C-string literal
/// 
/// Deduces the literal size at compile time:
/// @code
/// constexpr fixed_string<5> hello{"Hello"};
/// constexpr auto result = hello + " World";  // fixed_string<12>
/// @endcode
/// 
/// @tparam N1 Capacity of fixed_string
/// @tparam N2 Size of string literal (including null terminator)
/// @param lhs fixed_string operand
/// @param rhs C-string literal
/// @return fixed_string<N1 + N2 - 1>
template<std::size_t N1, std::size_t N2>
constexpr auto operator+(const fixed_string<N1>& lhs, const char (&rhs)[N2]) {
    fixed_string<N1 + N2 - 1> result;
    result.append(std::string_view(lhs));
    result.append(rhs);
    return result;
}

/// @brief Concatenate C-string literal with fixed_string
/// 
/// @tparam N1 Size of string literal (including null terminator)
/// @tparam N2 Capacity of fixed_string
/// @param lhs C-string literal
/// @param rhs fixed_string operand
/// @return fixed_string<N1 - 1 + N2>
template<std::size_t N1, std::size_t N2>
constexpr auto operator+(const char (&lhs)[N1], const fixed_string<N2>& rhs) {
    fixed_string<N1 - 1 + N2> result;
    result.append(lhs);
    result.append(std::string_view(rhs));
    return result;
}

/// @brief Concatenate fixed_string with single character
/// 
/// @tparam N Capacity of fixed_string
/// @param lhs fixed_string operand
/// @param rhs Character to append
/// @return fixed_string<N + 1>
template<std::size_t N>
constexpr auto operator+(const fixed_string<N>& lhs, char rhs) {
    fixed_string<N + 1> result;
    result.append(std::string_view(lhs));
    result.push_back(rhs);
    return result;
}

/// @brief Concatenate character with fixed_string
/// 
/// @tparam N Capacity of fixed_string
/// @param lhs Character to prepend
/// @param rhs fixed_string operand
/// @return fixed_string<1 + N>
template<std::size_t N>
constexpr auto operator+(char lhs, const fixed_string<N>& rhs) {
    fixed_string<1 + N> result;
    result.push_back(lhs);
    result.append(std::string_view(rhs));
    return result;
}

// ============================================================================
// Stream Operators
// ============================================================================

template<std::size_t N>
inline std::ostream& operator<<(std::ostream& os, const fixed_string<N>& str) {
    return os << std::string_view(str);
}

// ============================================================================
// Deduction Guides (C++17 CTAD - Class Template Argument Deduction)
// ============================================================================

/// @brief Deduce MaxSize from string literal
/// @details Enables: constexpr fixed_string name{"Hello"};  // Deduces fixed_string<5>
template<std::size_t N>
fixed_string(const char (&)[N]) -> fixed_string<N - 1>;

// ============================================================================
// Compile-Time String Literal Helpers
// ============================================================================

/// @brief Compile-time string literal wrapper for NTTP (Non-Type Template Parameter)
/// 
/// Allows passing string literals as template parameters in C++20:
/// @code
/// constexpr auto str = make_fixed<"Hello">();  // fixed_string<5>
/// @endcode
/// 
/// @tparam N Size of string literal (including null terminator)
template<std::size_t N>
struct StringLiteral {
    /// @brief Construct from string literal
    constexpr StringLiteral(const char (&str)[N]) {
        std::copy_n(str, N, value);
    }
    
    char value[N];                          ///< String data (including null terminator)
    static constexpr std::size_t size = N - 1;  ///< String length (excluding null terminator)
};

/// @brief Create fixed_string with auto-deduced size using NTTP (C++20)
/// 
/// Uses Non-Type Template Parameters to pass string literals as template arguments:
/// @code
/// constexpr auto name = make_fixed<"SeRTial">();  // fixed_string<7>
/// constexpr auto msg = make_fixed<"Real-Time">();  // fixed_string<9>
/// @endcode
/// 
/// @tparam Lit StringLiteral containing the compile-time string
/// @return fixed_string with automatically deduced size
/// @note Requires C++20 for class types as NTTP
template<StringLiteral Lit>
constexpr auto make_fixed() {
    return fixed_string<Lit.size>(Lit.value);
}

/// @brief Create fixed_string from rfl::internal::StringLiteral with auto-deduced size
/// 
/// Enables seamless integration with reflect-cpp's compile-time string literals:
/// @code
/// rfl::internal::StringLiteral lit{"field_name"};
/// auto name = make_fixed(lit);  // fixed_string<10>
/// @endcode
/// 
/// @tparam N Size of StringLiteral (including null terminator)
/// @param lit rfl::internal::StringLiteral instance
/// @return fixed_string with size automatically deduced from StringLiteral
/// @note Useful for integrating with reflect-cpp's rfl::Field<> and rfl::Literal<> names
template<std::size_t N>
constexpr auto make_fixed(const rfl::internal::StringLiteral<N>& lit) {
    // StringLiteral::length is constexpr and excludes null terminator
    return fixed_string<rfl::internal::StringLiteral<N>::length>(lit);
}

/// @brief Create fixed_string with explicit capacity from string literal
/// 
/// Useful when you want larger capacity than the initial string:
/// @code
/// constexpr auto name = make_fixed_string<64>("Hello");  // fixed_string<64> with "Hello"
/// @endcode
/// 
/// @tparam MaxSize Maximum capacity for the string
/// @tparam N Size of string literal (including null terminator)
/// @param str String literal to initialize with
/// @return fixed_string<MaxSize> initialized with str
/// @note MaxSize must be >= N-1
template<std::size_t MaxSize, std::size_t N>
constexpr fixed_string<MaxSize> make_fixed_string(const char (&str)[N]) {
    static_assert(N <= MaxSize + 1, "String literal exceeds fixed_string capacity");
    return fixed_string<MaxSize>(str);
}

/// @brief Create fixed_string with perfectly-sized capacity from literal
/// 
/// Automatically deduces the exact size needed:
/// @code
/// constexpr auto name = fixed_string_literal("Hello");  // fixed_string<5>
/// @endcode
/// 
/// @tparam N Size of string literal (including null terminator)
/// @param str String literal
/// @return fixed_string<N-1> with exact capacity
/// @note Equivalent to using CTAD: fixed_string{"Hello"}
template<std::size_t N>
constexpr fixed_string<N - 1> fixed_string_literal(const char (&str)[N]) {
    return fixed_string<N - 1>(str);
}

/// @brief User-defined literal suffix for fixed_string (C++20)
/// 
/// Provides the most concise syntax for compile-time string creation:
/// @code
/// using namespace sertial::literals;
/// auto name = "SeRTial"_fs;     // fixed_string<7>
/// auto msg = "Real-Time"_fs;    // fixed_string<9>
/// @endcode
/// 
/// @note Requires `using namespace sertial::literals;`
namespace literals {

/// @brief User-defined literal operator for fixed_string
/// @tparam Lit StringLiteral containing the compile-time string
/// @return fixed_string with auto-deduced size
template<StringLiteral Lit>
constexpr auto operator""_fs() {
    return fixed_string<Lit.size>(Lit.value);
}

} // namespace literals

} // namespace sertial
