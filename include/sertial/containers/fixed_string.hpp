#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sertial {

/// @brief Fixed-capacity string with zero heap allocation
/// @tparam MaxSize Maximum string length (excluding null terminator)
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
    
    constexpr fixed_string() noexcept : size_(0) {
        data_[0] = '\0';
    }
    
    constexpr fixed_string(const char* str) : size_(0) {
        if (str == nullptr) {
            data_[0] = '\0';
            return;
        }
        
        size_type len = std::strlen(str);
        if (len >= MaxSize) {
            throw std::length_error("fixed_string: string exceeds max_size");
        }
        
        std::memcpy(data_, str, len);
        size_ = len;
        data_[size_] = '\0';
    }
    
    constexpr fixed_string(std::string_view sv) : size_(0) {
        if (sv.size() >= MaxSize) {
            throw std::length_error("fixed_string: string_view exceeds max_size");
        }
        
        std::memcpy(data_, sv.data(), sv.size());
        size_ = sv.size();
        data_[size_] = '\0';
    }
    
    constexpr fixed_string(const std::string& str) : fixed_string(std::string_view(str)) {}
    
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
        
        size_type len = std::strlen(str);
        if (len >= MaxSize) {
            throw std::length_error("fixed_string: assign exceeds max_size");
        }
        
        std::memcpy(data_, str, len);
        size_ = len;
        data_[size_] = '\0';
    }
    
    constexpr void assign(std::string_view sv) {
        if (sv.size() >= MaxSize) {
            throw std::length_error("fixed_string: assign exceeds max_size");
        }
        
        std::memcpy(data_, sv.data(), sv.size());
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
        
        size_type len = std::strlen(str);
        if (size_ + len >= MaxSize) {
            throw std::length_error("fixed_string: append exceeds max_size");
        }
        
        std::memcpy(data_ + size_, str, len);
        size_ += len;
        data_[size_] = '\0';
        return *this;
    }
    
    constexpr fixed_string& append(std::string_view sv) {
        if (size_ + sv.size() >= MaxSize) {
            throw std::length_error("fixed_string: append exceeds max_size");
        }
        
        std::memcpy(data_ + size_, sv.data(), sv.size());
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
    char data_[MaxSize];
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

} // namespace sertial
