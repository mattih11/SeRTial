#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <bit>
#include "traits/memory_map.hpp"
#include "traits/padding.hpp"
#include "../traits/container_detection.hpp"

namespace sertial {

// ============================================================================
// Endianness Detection
// ============================================================================

/// @brief Get the system's native endianness
constexpr std::endian native_endian() noexcept {
    return std::endian::native;
}

/// @brief Check if system is little endian
constexpr bool is_little_endian() noexcept {
    return std::endian::native == std::endian::little;
}

/// @brief Check if system is big endian
constexpr bool is_big_endian() noexcept {
    return std::endian::native == std::endian::big;
}

// ============================================================================
// Byte Swap Primitives (Use compiler intrinsics for performance)
// ============================================================================

/// @brief Byte swap for 16-bit values using compiler intrinsics
constexpr uint16_t bswap16(uint16_t value) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(value);
#elif defined(_MSC_VER)
    return _byteswap_ushort(value);
#else
    return (value >> 8) | (value << 8);
#endif
}

/// @brief Byte swap for 32-bit values using compiler intrinsics
constexpr uint32_t bswap32(uint32_t value) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(value);
#elif defined(_MSC_VER)
    return _byteswap_ulong(value);
#else
    return ((value >> 24) & 0x000000FF) |
           ((value >>  8) & 0x0000FF00) |
           ((value <<  8) & 0x00FF0000) |
           ((value << 24) & 0xFF000000);
#endif
}

/// @brief Byte swap for 64-bit values using compiler intrinsics
constexpr uint64_t bswap64(uint64_t value) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(value);
#elif defined(_MSC_VER)
    return _byteswap_uint64(value);
#else
    return ((value >> 56) & 0x00000000000000FFULL) |
           ((value >> 40) & 0x000000000000FF00ULL) |
           ((value >> 24) & 0x0000000000FF0000ULL) |
           ((value >>  8) & 0x00000000FF000000ULL) |
           ((value <<  8) & 0x000000FF00000000ULL) |
           ((value << 24) & 0x0000FF0000000000ULL) |
           ((value << 40) & 0x00FF000000000000ULL) |
           ((value << 56) & 0xFF00000000000000ULL);
#endif
}

// ============================================================================
// Field Type Byte Swap (Generic)
// ============================================================================

namespace detail {

// ============================================================================
// Byte Swap at Field Offsets
// ============================================================================

/// @brief Swap bytes of a field at a specific offset in serialized data
/// This is instantiated at compile-time for each field type and offset
template<typename FieldType, std::size_t Offset>
void swap_field_at_offset(std::span<std::byte> data) noexcept {
    // Only swap multi-byte arithmetic types
    if constexpr (std::is_arithmetic_v<FieldType> && sizeof(FieldType) > 1) {
        if constexpr (sizeof(FieldType) == 2) {
            auto* ptr = reinterpret_cast<uint16_t*>(data.data() + Offset);
            *ptr = bswap16(*ptr);
        } else if constexpr (sizeof(FieldType) == 4) {
            auto* ptr = reinterpret_cast<uint32_t*>(data.data() + Offset);
            *ptr = bswap32(*ptr);
        } else if constexpr (sizeof(FieldType) == 8) {
            auto* ptr = reinterpret_cast<uint64_t*>(data.data() + Offset);
            *ptr = bswap64(*ptr);
        }
        // sizeof == 1: no swap needed (bytes, chars, bool)
    }
    // Non-arithmetic types (structs): handled via recursion
}

// Note: field_type_t is already defined in padding.hpp, just use it directly

/// @brief Helper to convert runtime array element to compile-time constant
/// This creates a template instantiation for each index, turning the array
/// value into a non-type template parameter
template<typename T, std::size_t Index>
struct offset_constant {
    static constexpr std::size_t value = MemoryMap<T>::packed_offsets[Index];
};

/// @brief Swap a single field using compile-time offset
template<typename T, typename Field, std::size_t Index>
void swap_one_field(std::span<std::byte> data) noexcept {
    using FieldType = field_type_t<Field>;
    constexpr std::size_t Offset = offset_constant<T, Index>::value;
    swap_field_at_offset<FieldType, Offset>(data);
}

/// @brief Swap all fields by expanding over Fields and Is parameter packs
template<typename T, typename... Fields, std::size_t... Is>
void swap_all_fields(std::span<std::byte> data, 
                     rfl::NamedTuple<Fields...>*,
                     std::index_sequence<Is...>) noexcept {
    // Fold expression: for each (Field, Index) pair, call swap_one_field
    // All offsets are resolved at compile-time via offset_constant
    (swap_one_field<T, Fields, Is>(data), ...);
}

/// @brief Entry point: dispatch to field swapping
template<typename T, std::size_t... Is>
void swap_serialized_fields_impl(std::span<std::byte> data, std::index_sequence<Is...>) noexcept {
    using NT = rfl::named_tuple_t<T>;
    swap_all_fields<T>(data, static_cast<NT*>(nullptr), std::index_sequence<Is...>{});
}

} // namespace detail

// ============================================================================
// Public API: Swap Endianness in Serialized Data
// ============================================================================

/// @brief Swap endianness of all fields in serialized (packed) data
/// 
/// This operates on the serialized byte stream using compile-time MemoryMap
/// information. All offsets and types are resolved at compile-time, resulting
/// in a sequence of direct bswap operations with hardcoded offsets.
/// 
/// **IMPORTANT**: Structs with fixed containers (fixed_vector/fixed_string) cannot
/// use this function, as they have variable-length serialized format.
/// Only works on fixed-size serialized data from trivially copyable structs.
/// 
/// @tparam T The message type
/// @param data The serialized data (will be modified in-place)
/// 
/// @note This is a zero-cost abstraction: at runtime, this becomes a sequence
/// of bswap instructions at hardcoded offsets.
/// 
/// @example
/// ```cpp
/// auto buffer = serialize(message);
/// swap_endianness<Message>(buffer.data());  // Convert to different endianness
/// send(buffer.view());
/// ```
template<typename T>
void swap_endianness(std::span<std::byte> data) noexcept {
    static_assert(detail::has_named_tuple_t_v<T>, 
        "swap_endianness requires a reflectable struct type");
    
    // Compile-time check: fail if struct contains fixed containers
    // (they have variable-length serialized format, not fixed packed layout)
    static_assert(!detail::struct_has_fixed_containers<T>(),
        "Structs with fixed_vector/fixed_string have variable-length serialized format. "
        "Endianness conversion must be done during deserialization, not on raw bytes.");
    
    constexpr std::size_t field_count = MemoryMap<T>::field_count;
    detail::swap_serialized_fields_impl<T>(data, std::make_index_sequence<field_count>{});
}

/// @brief Swap endianness conditionally based on source endianness
/// 
/// Only performs the swap if the source endianness differs from native.
/// 
/// **IMPORTANT**: Only works on fixed-size serialized data from trivially
/// copyable structs without fixed containers.
/// 
/// @tparam T The message type
/// @param data The serialized data
/// @param source_endian The endianness of the source data
/// 
/// @example
/// ```cpp
/// // Receiver side
/// auto buffer = receive();
/// swap_endianness_from<Message>(buffer.data(), sender_endianness);
/// auto message = deserialize<Message>(buffer);
/// ```
template<typename T>
void swap_endianness_from(std::span<std::byte> data, std::endian source_endian) noexcept {
    static_assert(!detail::struct_has_fixed_containers<T>(),
        "Structs with fixed_vector/fixed_string have variable-length serialized format. "
        "Endianness conversion must be done during deserialization.");
    
    if (source_endian != std::endian::native) {
        swap_endianness<T>(data);
    }
}

/// @brief Swap endianness conditionally based on target endianness
/// 
/// Only performs the swap if the target endianness differs from native.
/// 
/// **IMPORTANT**: Only works on fixed-size serialized data from trivially
/// copyable structs without fixed containers.
/// 
/// @tparam T The message type
/// @param data The serialized data
/// @param target_endian The endianness required by the target
/// 
/// @example
/// ```cpp
/// // Sender side
/// auto buffer = serialize(message);
/// swap_endianness_to<Message>(buffer.data(), target_endianness);
/// send(buffer.view());
/// ```
template<typename T>
void swap_endianness_to(std::span<std::byte> data, std::endian target_endian) noexcept {
    static_assert(!detail::struct_has_fixed_containers<T>(),
        "Structs with fixed_vector/fixed_string have variable-length serialized format. "
        "Endianness conversion must be done during deserialization.");
    
    if (target_endian != std::endian::native) {
        swap_endianness<T>(data);
    }
}

} // namespace sertial
