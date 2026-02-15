#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

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
/// // Explicit size with braces (CTAD - Class Template Argument Deduction)
/// constexpr fixed_string name{"SeRTial"};  // Deduces fixed_string<7>
/// 
/// // Auto-deduced size using helper functions
/// constexpr auto str1 = fixed_string_literal("Hello");  // fixed_string<5>
/// constexpr auto str2 = make_fixed<"World">();          // fixed_string<5> (C++20 NTTP)
/// 
/// // Using user-defined literal (C++20)
/// using namespace sertial::literals;
/// auto str3 = "RealTime"_fs;  // fixed_string<8>
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
        if (size_ >= MaxSize - 1) {
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
        if (size_ + sv.size() >= MaxSize) {
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
