#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <iterator>

namespace sertial {

/**
 * @brief Fixed-capacity circular buffer with compile-time sizing
 * 
 * Zero-allocation ring buffer suitable for realtime systems. When full,
 * oldest elements are automatically overwritten (FIFO overwrite behavior).
 * 
 * @tparam T Element type (must be copyable or movable)
 * @tparam MaxSize Maximum capacity (compile-time constant)
 * 
 * Features:
 * - Zero runtime allocation (fully stack-based)
 * - Fixed capacity determined at compile-time
 * - Circular buffer with automatic overwrite when full
 * - O(1) operations for all methods
 * - STL-compatible iterators
 * - constexpr support (C++20)
 * 
 * Thread Safety:
 * - NOT thread-safe by default
 * - Users must provide external synchronization
 * 
 * @example
 * @code
 * RingBuffer<int, 5> buffer;
 * buffer.push_back(1);
 * buffer.push_back(2);
 * assert(buffer.size() == 2);
 * assert(buffer.front() == 1);
 * assert(buffer.back() == 2);
 * 
 * // Fill and overflow
 * for (int i = 3; i <= 6; ++i) {
 *     buffer.push_back(i);
 * }
 * assert(buffer.size() == 5);
 * assert(buffer.front() == 2); // 1 was overwritten
 * @endcode
 */
template<typename T, size_t MaxSize>
class RingBuffer {
    // Compile-time constraints
    static_assert(MaxSize > 0, "MaxSize must be greater than 0");
    static_assert(std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>,
                  "T must be copy or move constructible");
    
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    
    // Forward iterator for traversing buffer from oldest to newest
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;
        
        constexpr iterator() noexcept : buffer_(nullptr), index_(0) {}
        
        constexpr reference operator*() const noexcept {
            return (*buffer_)[index_];
        }
        
        constexpr pointer operator->() const noexcept {
            return &(*buffer_)[index_];
        }
        
        constexpr iterator& operator++() noexcept {
            ++index_;
            return *this;
        }
        
        constexpr iterator operator++(int) noexcept {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        
        constexpr bool operator==(const iterator& other) const noexcept {
            return buffer_ == other.buffer_ && index_ == other.index_;
        }
        
        constexpr bool operator!=(const iterator& other) const noexcept {
            return !(*this == other);
        }
        
    private:
        friend class RingBuffer;
        
        constexpr iterator(RingBuffer* buffer, size_type index) noexcept
            : buffer_(buffer), index_(index) {}
        
        RingBuffer* buffer_;
        size_type index_;
    };
    
    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;
        
        constexpr const_iterator() noexcept : buffer_(nullptr), index_(0) {}
        
        constexpr const_iterator(const iterator& it) noexcept
            : buffer_(it.buffer_), index_(it.index_) {}
        
        constexpr reference operator*() const noexcept {
            return (*buffer_)[index_];
        }
        
        constexpr pointer operator->() const noexcept {
            return &(*buffer_)[index_];
        }
        
        constexpr const_iterator& operator++() noexcept {
            ++index_;
            return *this;
        }
        
        constexpr const_iterator operator++(int) noexcept {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        
        constexpr bool operator==(const const_iterator& other) const noexcept {
            return buffer_ == other.buffer_ && index_ == other.index_;
        }
        
        constexpr bool operator!=(const const_iterator& other) const noexcept {
            return !(*this == other);
        }
        
    private:
        friend class RingBuffer;
        
        constexpr const_iterator(const RingBuffer* buffer, size_type index) noexcept
            : buffer_(buffer), index_(index) {}
        
        const RingBuffer* buffer_;
        size_type index_;
    };
    
    // ========================================================================
    // Construction and Capacity
    // ========================================================================
    
    /**
     * @brief Default constructor - creates empty buffer
     */
    constexpr RingBuffer() noexcept = default;
    
    /**
     * @brief Get maximum capacity (compile-time constant)
     * @return Maximum number of elements the buffer can hold
     */
    static constexpr size_type capacity() noexcept {
        return MaxSize;
    }
    
    /**
     * @brief Get current number of elements in buffer
     * @return Number of elements currently stored
     */
    constexpr size_type size() const noexcept {
        return size_;
    }
    
    /**
     * @brief Check if buffer is empty
     * @return true if buffer contains no elements
     */
    constexpr bool empty() const noexcept {
        return size_ == 0;
    }
    
    /**
     * @brief Check if buffer is full
     * @return true if buffer is at maximum capacity
     */
    constexpr bool full() const noexcept {
        return size_ == MaxSize;
    }
    
    /**
     * @brief Clear all elements from buffer
     * 
     * After this call, size() returns 0. Does not deallocate memory.
     * Runs destructors for contained elements.
     */
    constexpr void clear() noexcept {
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }
    
    // ========================================================================
    // Element Access
    // ========================================================================
    
    /**
     * @brief Access element by logical index (0 = oldest)
     * 
     * @param index Logical index (0 to size()-1)
     * @return Reference to element
     * 
     * @note Undefined behavior if index >= size()
     * @note O(1) time complexity
     */
    constexpr reference operator[](size_type index) noexcept {
        return data_[(tail_ + index) % MaxSize];
    }
    
    constexpr const_reference operator[](size_type index) const noexcept {
        return data_[(tail_ + index) % MaxSize];
    }
    
    /**
     * @brief Access element with bounds checking
     * 
     * @param index Logical index (0 to size()-1)
     * @return Reference to element
     * @throws std::out_of_range if index >= size()
     */
    constexpr reference at(size_type index) {
        if (index >= size_) {
            throw std::out_of_range("RingBuffer::at: index out of range");
        }
        return (*this)[index];
    }
    
    constexpr const_reference at(size_type index) const {
        if (index >= size_) {
            throw std::out_of_range("RingBuffer::at: index out of range");
        }
        return (*this)[index];
    }
    
    /**
     * @brief Access oldest element
     * @return Reference to oldest (first inserted) element
     * @note Undefined behavior if empty()
     */
    constexpr reference front() noexcept {
        return data_[tail_];
    }
    
    constexpr const_reference front() const noexcept {
        return data_[tail_];
    }
    
    /**
     * @brief Access newest element
     * @return Reference to newest (last inserted) element
     * @note Undefined behavior if empty()
     */
    constexpr reference back() noexcept {
        size_type back_idx = (head_ == 0) ? MaxSize - 1 : head_ - 1;
        return data_[back_idx];
    }
    
    constexpr const_reference back() const noexcept {
        size_type back_idx = (head_ == 0) ? MaxSize - 1 : head_ - 1;
        return data_[back_idx];
    }
    
    // ========================================================================
    // Modifiers
    // ========================================================================
    
    /**
     * @brief Push element to back (newest position)
     * 
     * If buffer is full, automatically overwrites oldest element.
     * 
     * @param value Element to copy
     * @note O(1) time complexity
     */
    constexpr void push_back(const T& value)
        noexcept(std::is_nothrow_copy_assignable_v<T>) {
        data_[head_] = value;
        head_ = (head_ + 1) % MaxSize;
        
        if (size_ < MaxSize) {
            ++size_;
        } else {
            // Buffer full - advance tail (overwrite oldest)
            tail_ = (tail_ + 1) % MaxSize;
        }
    }
    
    /**
     * @brief Push element to back (newest position) via move
     * 
     * If buffer is full, automatically overwrites oldest element.
     * 
     * @param value Element to move
     * @note O(1) time complexity
     */
    constexpr void push_back(T&& value)
        noexcept(std::is_nothrow_move_assignable_v<T>) {
        data_[head_] = std::move(value);
        head_ = (head_ + 1) % MaxSize;
        
        if (size_ < MaxSize) {
            ++size_;
        } else {
            // Buffer full - advance tail (overwrite oldest)
            tail_ = (tail_ + 1) % MaxSize;
        }
    }
    
    /**
     * @brief Construct element in-place at back
     * 
     * If buffer is full, automatically overwrites oldest element.
     * 
     * @tparam Args Constructor argument types
     * @param args Arguments forwarded to T's constructor
     * @note O(1) time complexity
     */
    template<typename... Args>
    constexpr void emplace_back(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        // For array storage, we assign rather than placement new
        // This assumes T is default-constructible (array requirement)
        data_[head_] = T(std::forward<Args>(args)...);
        head_ = (head_ + 1) % MaxSize;
        
        if (size_ < MaxSize) {
            ++size_;
        } else {
            // Buffer full - advance tail (overwrite oldest)
            tail_ = (tail_ + 1) % MaxSize;
        }
    }
    
    /**
     * @brief Remove oldest element (front)
     * 
     * @note Undefined behavior if empty()
     * @note O(1) time complexity
     */
    constexpr void pop_front() noexcept {
        tail_ = (tail_ + 1) % MaxSize;
        --size_;
    }
    
    /**
     * @brief Remove newest element (back)
     * 
     * @note Undefined behavior if empty()
     * @note O(1) time complexity
     */
    constexpr void pop_back() noexcept {
        head_ = (head_ == 0) ? MaxSize - 1 : head_ - 1;
        --size_;
    }
    
    // ========================================================================
    // Iterators
    // ========================================================================
    
    /**
     * @brief Get iterator to oldest element
     * @return Iterator pointing to first (oldest) element
     */
    constexpr iterator begin() noexcept {
        return iterator(this, 0);
    }
    
    constexpr const_iterator begin() const noexcept {
        return const_iterator(this, 0);
    }
    
    constexpr const_iterator cbegin() const noexcept {
        return const_iterator(this, 0);
    }
    
    /**
     * @brief Get iterator past the last element
     * @return Iterator pointing past newest element
     */
    constexpr iterator end() noexcept {
        return iterator(this, size_);
    }
    
    constexpr const_iterator end() const noexcept {
        return const_iterator(this, size_);
    }
    
    constexpr const_iterator cend() const noexcept {
        return const_iterator(this, size_);
    }
    
    // ========================================================================
    // Internal API (for BinaryReflector/Serialization)
    // ========================================================================
    
    /// @brief Get raw data pointer to underlying storage
    /// @note For serialization - accesses physical storage, not logical order
    /// @warning Direct access bypasses ring buffer semantics!
    constexpr pointer data_unsafe() noexcept {
        return data_.data();
    }
    
    constexpr const_pointer data_unsafe() const noexcept {
        return data_.data();
    }
    
    /// @brief Const data access (required by SerializableContainer concept)
    /// @note Alias for data_unsafe() - provides raw storage pointer
    /// @warning Does NOT return logical order! Use only with custom serialization
    constexpr const_pointer data() const noexcept {
        return data_unsafe();
    }
    
    /// @brief Unsafe direct size setter (for deserialization)
    /// @warning Caller must ensure data is properly initialized!
    /// @note Resets head/tail to maintain invariants
    constexpr void set_size_unsafe(size_type new_size) noexcept {
        assert(new_size <= MaxSize && "RingBuffer: set_size_unsafe exceeds capacity");
        size_ = new_size;
        head_ = new_size % MaxSize;
        tail_ = 0;
    }
    
    /// @brief Get pointer to logically-ordered data (for serialization)
    /// @return Pointer to array of size() elements in logical order (oldest to newest)
    /// @note This creates a temporary copy for serialization - O(n) operation
    /// @warning Only use for serialization purposes
    constexpr std::array<T, MaxSize> get_ordered_data() const noexcept {
        std::array<T, MaxSize> result{};
        for (size_type i = 0; i < size_; ++i) {
            result[i] = (*this)[i];
        }
        return result;
    }
    
    /// @brief Check if buffer data wraps around
    /// @return true if data crosses the buffer boundary (head <= tail when size > 0)
    /// @note For serialization - determines if 1 or 2 memcpy operations needed
    constexpr bool is_wrapped() const noexcept {
        return size_ > 0 && head_ <= tail_;
    }
    
    /// @brief Get tail index (oldest element position)
    /// @return Physical index of tail in underlying storage
    /// @note For serialization - starting position of first data region
    constexpr size_type tail_index() const noexcept {
        return tail_;
    }
    
    /// @brief Get head index (next write position)
    /// @return Physical index of head in underlying storage
    /// @note For serialization - used in wrap-around calculations
    constexpr size_type head_index() const noexcept {
        return head_;
    }
    
    /// @brief Compile-time capacity constant (for SerializableContainer compatibility)
    /// @note Added for trait detection, but RingBuffer uses custom serialization
    static constexpr std::size_t max_size_v = MaxSize;
    
private:
    std::array<T, MaxSize> data_{};  // Fixed storage, default-initialized
    size_type head_{0};              // Write position (next insert)
    size_type tail_{0};              // Read position (oldest element)
    size_type size_{0};              // Current element count
};

} // namespace sertial
