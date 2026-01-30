#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <cassert>

namespace sertial {

/// @brief Fixed-capacity byte buffer with zero heap allocation
/// 
/// This is a stack-allocated buffer for serialization that avoids
/// std::vector<std::byte> heap allocations. The capacity is fixed
/// at compile time, but the actual used size is tracked at runtime.
/// 
/// @tparam Capacity Maximum number of bytes the buffer can hold
/// 
/// @example
/// ```cpp
/// // Create buffer with exact size for a type
/// static_buffer<HybridMemoryMap<Position>::max_packed_size> buf;
/// buf.resize(serialize_to(pos, buf.data()));
/// send(buf.view());
/// 
/// // Or use serialize() which returns static_buffer directly
/// auto buf = serialize(pos);
/// send(buf.view());
/// ```
template<std::size_t Capacity>
class static_buffer {
public:
    using value_type = std::byte;
    using size_type = std::size_t;
    using pointer = std::byte*;
    using const_pointer = const std::byte*;
    using iterator = std::byte*;
    using const_iterator = const std::byte*;
    
    static constexpr size_type capacity_v = Capacity;
    
    // ========================================================================
    // Construction
    // ========================================================================
    
    /// @brief Default constructor - empty buffer
    constexpr static_buffer() noexcept : size_(0) {}
    
    /// @brief Construct with initial size (uninitialized data)
    constexpr explicit static_buffer(size_type initial_size) noexcept 
        : size_(initial_size) 
    {
        assert(initial_size <= Capacity && "static_buffer: initial_size exceeds capacity");
    }
    
    /// @brief Construct from a span of bytes (copies data)
    constexpr static_buffer(std::span<const std::byte> data) noexcept : size_(0) {
        assign(data);
    }
    
    // ========================================================================
    // Capacity
    // ========================================================================
    
    /// @brief Current number of bytes in the buffer
    [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
    
    /// @brief Maximum capacity (compile-time constant)
    [[nodiscard]] constexpr size_type capacity() const noexcept { return Capacity; }
    
    /// @brief Remaining space available
    [[nodiscard]] constexpr size_type remaining() const noexcept { return Capacity - size_; }
    
    /// @brief Is the buffer empty?
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
    
    /// @brief Is the buffer full?
    [[nodiscard]] constexpr bool full() const noexcept { return size_ == Capacity; }
    
    // ========================================================================
    // Data Access
    // ========================================================================
    
    /// @brief Pointer to buffer data
    [[nodiscard]] constexpr pointer data() noexcept { return data_.data(); }
    
    /// @brief Const pointer to buffer data
    [[nodiscard]] constexpr const_pointer data() const noexcept { return data_.data(); }
    
    /// @brief Get a view of the used portion of the buffer
    [[nodiscard]] constexpr std::span<const std::byte> view() const noexcept {
        return {data_.data(), size_};
    }
    
    /// @brief Get a mutable span of the used portion
    [[nodiscard]] constexpr std::span<std::byte> span() noexcept {
        return {data_.data(), size_};
    }
    
    /// @brief Get a span of the entire capacity (for writing into)
    [[nodiscard]] constexpr std::span<std::byte> full_span() noexcept {
        return {data_.data(), Capacity};
    }
    
    /// @brief Access byte at index
    [[nodiscard]] constexpr std::byte& operator[](size_type index) noexcept {
        assert(index < size_ && "static_buffer: index out of bounds");
        return data_[index];
    }
    
    /// @brief Access byte at index (const)
    [[nodiscard]] constexpr const std::byte& operator[](size_type index) const noexcept {
        assert(index < size_ && "static_buffer: index out of bounds");
        return data_[index];
    }
    
    // ========================================================================
    // Iterators
    // ========================================================================
    
    [[nodiscard]] constexpr iterator begin() noexcept { return data_.data(); }
    [[nodiscard]] constexpr iterator end() noexcept { return data_.data() + size_; }
    [[nodiscard]] constexpr const_iterator begin() const noexcept { return data_.data(); }
    [[nodiscard]] constexpr const_iterator end() const noexcept { return data_.data() + size_; }
    [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return data_.data(); }
    [[nodiscard]] constexpr const_iterator cend() const noexcept { return data_.data() + size_; }
    
    // ========================================================================
    // Modifiers
    // ========================================================================
    
    /// @brief Set the used size (must be <= capacity)
    constexpr void resize(size_type new_size) noexcept {
        assert(new_size <= Capacity && "static_buffer: resize exceeds capacity");
        size_ = new_size;
    }
    
    /// @brief Clear the buffer (sets size to 0)
    constexpr void clear() noexcept { size_ = 0; }
    
    /// @brief Assign from a span of bytes
    constexpr void assign(std::span<const std::byte> data) noexcept {
        assert(data.size() <= Capacity && "static_buffer: data exceeds capacity");
        std::memcpy(data_.data(), data.data(), data.size());
        size_ = data.size();
    }
    
    /// @brief Append bytes to the buffer
    constexpr void append(std::span<const std::byte> data) noexcept {
        assert(size_ + data.size() <= Capacity && "static_buffer: append exceeds capacity");
        std::memcpy(data_.data() + size_, data.data(), data.size());
        size_ += data.size();
    }
    
    /// @brief Append a single byte
    constexpr void push_back(std::byte b) noexcept {
        assert(size_ < Capacity && "static_buffer: push_back on full buffer");
        data_[size_++] = b;
    }
    
private:
    std::array<std::byte, Capacity> data_{};
    size_type size_ = 0;
};

// ============================================================================
// Helper to create a buffer sized for a specific type
// ============================================================================

/// @brief Create a static_buffer sized for type T's packed size
/// @tparam T The type to size the buffer for
/// @return A static_buffer with capacity = MemoryMap<T>::packed_size
template<typename T>
[[nodiscard]] constexpr auto make_static_buffer() {
    // Forward declare to avoid circular include
    // The actual MemoryMap is included where this is used
    constexpr std::size_t size = sizeof(T);  // Conservative: use sizeof as minimum
    return static_buffer<size>{};
}

// ============================================================================
// Type alias for common sizes
// ============================================================================

using static_buffer_64   = static_buffer<64>;
using static_buffer_128  = static_buffer<128>;
using static_buffer_256  = static_buffer<256>;
using static_buffer_512  = static_buffer<512>;
using static_buffer_1k   = static_buffer<1024>;
using static_buffer_4k   = static_buffer<4096>;
using static_buffer_64k  = static_buffer<65536>;

} // namespace sertial
