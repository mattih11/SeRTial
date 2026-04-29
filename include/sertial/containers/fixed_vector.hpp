#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>

namespace sertial {

/// @brief Fixed-capacity vector with zero heap allocation
/// @tparam T Element type
/// @tparam MaxSize Maximum number of elements
template<typename T, std::size_t MaxSize>
class fixed_vector {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;
    
    static constexpr size_type max_size_v = MaxSize;
    
    // ========================================================================
    // Construction
    // ========================================================================
    
    constexpr fixed_vector() noexcept : size_(0) {}
    
    constexpr fixed_vector(std::initializer_list<T> init) : size_(0) {
        if (init.size() > MaxSize) {
            throw std::length_error("fixed_vector: initializer list exceeds max_size");
        }
        for (const auto& item : init) {
            push_back(item);
        }
    }
    
    constexpr fixed_vector(size_type count, const T& value) : size_(0) {
        if (count > MaxSize) {
            throw std::length_error("fixed_vector: count exceeds max_size");
        }
        for (size_type i = 0; i < count; ++i) {
            push_back(value);
        }
    }
    
    // ========================================================================
    // Capacity
    // ========================================================================
    
    [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
    
    [[nodiscard]] constexpr size_type max_size() const noexcept { return MaxSize; }
    
    [[nodiscard]] constexpr size_type capacity() const noexcept { return MaxSize; }
    
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
    
    [[nodiscard]] constexpr bool full() const noexcept { return size_ == MaxSize; }
    
    // ========================================================================
    // Element Access
    // ========================================================================
    
    constexpr reference operator[](size_type index) noexcept {
        assert(index < size_ && "fixed_vector: index out of bounds");
        return data_[index];
    }
    
    constexpr const_reference operator[](size_type index) const noexcept {
        assert(index < size_ && "fixed_vector: index out of bounds");
        return data_[index];
    }
    
    constexpr reference at(size_type index) {
        if (index >= size_) {
            throw std::out_of_range("fixed_vector: index out of bounds");
        }
        return data_[index];
    }
    
    constexpr const_reference at(size_type index) const {
        if (index >= size_) {
            throw std::out_of_range("fixed_vector: index out of bounds");
        }
        return data_[index];
    }
    
    constexpr reference front() noexcept {
        assert(!empty() && "fixed_vector: front() on empty vector");
        return data_[0];
    }
    
    constexpr const_reference front() const noexcept {
        assert(!empty() && "fixed_vector: front() on empty vector");
        return data_[0];
    }
    
    constexpr reference back() noexcept {
        assert(!empty() && "fixed_vector: back() on empty vector");
        return data_[size_ - 1];
    }
    
    constexpr const_reference back() const noexcept {
        assert(!empty() && "fixed_vector: back() on empty vector");
        return data_[size_ - 1];
    }
    
    constexpr pointer data() noexcept { return data_; }
    
    constexpr const_pointer data() const noexcept { return data_; }
    
    // ========================================================================
    // Modifiers
    // ========================================================================
    
    constexpr void push_back(const T& value) {
        if (size_ >= MaxSize) {
            throw std::length_error("fixed_vector: push_back() on full vector");
        }
        data_[size_++] = value;
    }
    
    constexpr void push_back(T&& value) {
        if (size_ >= MaxSize) {
            throw std::length_error("fixed_vector: push_back() on full vector");
        }
        data_[size_++] = std::move(value);
    }
    
    template<typename... Args>
    constexpr reference emplace_back(Args&&... args) {
        if (size_ >= MaxSize) {
            throw std::length_error("fixed_vector: emplace_back() on full vector");
        }
        T* ptr = &data_[size_++];
        new (ptr) T(std::forward<Args>(args)...);
        return *ptr;
    }
    
    constexpr void pop_back() noexcept {
        assert(!empty() && "fixed_vector: pop_back() on empty vector");
        --size_;
        if constexpr (!std::is_trivially_destructible_v<T>) {
            data_[size_].~T();
        }
    }
    
    constexpr void clear() noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_type i = 0; i < size_; ++i) {
                data_[i].~T();
            }
        }
        size_ = 0;
    }
    
    constexpr void resize(size_type new_size) {
        if (new_size > MaxSize) {
            throw std::length_error("fixed_vector: resize exceeds max_size");
        }
        
        if (new_size < size_) {
            // Shrink
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (size_type i = new_size; i < size_; ++i) {
                    data_[i].~T();
                }
            }
        } else if (new_size > size_) {
            // Grow with default construction
            for (size_type i = size_; i < new_size; ++i) {
                new (&data_[i]) T();
            }
        }
        size_ = new_size;
    }
    
    constexpr void resize(size_type new_size, const T& value) {
        if (new_size > MaxSize) {
            throw std::length_error("fixed_vector: resize exceeds max_size");
        }
        
        if (new_size < size_) {
            // Shrink
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (size_type i = new_size; i < size_; ++i) {
                    data_[i].~T();
                }
            }
        } else if (new_size > size_) {
            // Grow with value
            for (size_type i = size_; i < new_size; ++i) {
                new (&data_[i]) T(value);
            }
        }
        size_ = new_size;
    }
    
    /**
     * @brief Erase element at position
     * @param pos Iterator to element to remove
     * @return Iterator following the last removed element
     * @note O(n) time complexity - shifts subsequent elements
     */
    constexpr iterator erase(const_iterator pos) {
        assert(pos >= begin() && pos < end() && "fixed_vector: erase iterator out of range");
        
        iterator mutable_pos = const_cast<iterator>(pos);
        iterator last = end();
        
        // Shift elements left
        std::move(mutable_pos + 1, last, mutable_pos);
        
        // Destroy last element
        --size_;
        if constexpr (!std::is_trivially_destructible_v<T>) {
            data_[size_].~T();
        }
        
        return mutable_pos;
    }
    
    /**
     * @brief Erase range of elements
     * @param first Iterator to first element to remove
     * @param last Iterator past the last element to remove
     * @return Iterator following the last removed element
     * @note O(n) time complexity - shifts subsequent elements
     */
    constexpr iterator erase(const_iterator first, const_iterator last) {
        assert(first >= begin() && first <= end() && "fixed_vector: erase first iterator out of range");
        assert(last >= first && last <= end() && "fixed_vector: erase last iterator out of range");
        
        if (first == last) return const_cast<iterator>(last);
        
        iterator mutable_first = const_cast<iterator>(first);
        iterator mutable_last = const_cast<iterator>(last);
        iterator vec_end = end();
        
        // Calculate number of elements to remove
        size_type num_erased = static_cast<size_type>(mutable_last - mutable_first);
        
        // Shift elements left
        std::move(mutable_last, vec_end, mutable_first);
        
        // Destroy removed elements at end
        size_type new_size = size_ - num_erased;
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_type i = new_size; i < size_; ++i) {
                data_[i].~T();
            }
        }
        size_ = new_size;
        
        return mutable_first;
    }
    
    /**
     * @brief Remove all elements equal to value
     * @param value Value to remove
     * @return Number of elements removed
     * @note O(n) time complexity
     */
    constexpr size_type remove(const T& value) {
        size_type old_size = size_;
        auto new_end = std::remove(begin(), end(), value);
        
        // Destroy removed elements
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (iterator it = new_end; it != end(); ++it) {
                it->~T();
            }
        }
        
        size_ = static_cast<size_type>(new_end - begin());
        return old_size - size_;
    }
    
    /**
     * @brief Remove all elements satisfying predicate
     * @tparam Predicate Unary predicate type
     * @param pred Predicate that returns true for elements to remove
     * @return Number of elements removed
     * @note O(n) time complexity
     * 
     * @example
     * @code
     * fixed_vector<int, 10> vec = {1, 2, 3, 4, 5};
     * vec.remove_if([](int x) { return x % 2 == 0; }); // Removes evens
     * // vec is now {1, 3, 5}
     * @endcode
     */
    template<typename Predicate>
    constexpr size_type remove_if(Predicate pred) {
        size_type old_size = size_;
        auto new_end = std::remove_if(begin(), end(), pred);
        
        // Destroy removed elements
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (iterator it = new_end; it != end(); ++it) {
                it->~T();
            }
        }
        
        size_ = static_cast<size_type>(new_end - begin());
        return old_size - size_;
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
    // Internal API (for BinaryReflector)
    // ========================================================================
    
    /// @brief Unsafe direct size setter (for deserialization)
    /// @warning Caller must ensure data is properly initialized!
    constexpr void set_size_unsafe(size_type new_size) noexcept {
        assert(new_size <= MaxSize && "fixed_vector: set_size_unsafe exceeds max_size");
        size_ = new_size;
    }
    
    /// @brief Unsafe direct data access (for deserialization)
    constexpr pointer data_unsafe() noexcept { return data_; }
    
private:
    T data_[MaxSize];
    size_type size_;
};

// ============================================================================
// Comparison Operators
// ============================================================================

template<typename T, std::size_t N>
constexpr bool operator==(const fixed_vector<T, N>& lhs, const fixed_vector<T, N>& rhs) {
    if (lhs.size() != rhs.size()) return false;
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template<typename T, std::size_t N>
constexpr bool operator!=(const fixed_vector<T, N>& lhs, const fixed_vector<T, N>& rhs) {
    return !(lhs == rhs);
}

template<typename T, std::size_t N>
constexpr bool operator<(const fixed_vector<T, N>& lhs, const fixed_vector<T, N>& rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template<typename T, std::size_t N>
constexpr bool operator<=(const fixed_vector<T, N>& lhs, const fixed_vector<T, N>& rhs) {
    return !(rhs < lhs);
}

template<typename T, std::size_t N>
constexpr bool operator>(const fixed_vector<T, N>& lhs, const fixed_vector<T, N>& rhs) {
    return rhs < lhs;
}

template<typename T, std::size_t N>
constexpr bool operator>=(const fixed_vector<T, N>& lhs, const fixed_vector<T, N>& rhs) {
    return !(lhs < rhs);
}

} // namespace sertial
