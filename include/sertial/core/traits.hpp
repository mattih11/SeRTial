#pragma once

// ============================================================================
// SeRTial Traits Module
// ============================================================================
// Comprehensive compile-time type analysis for binary serialization.
//
// This header includes all traits sub-modules:
// - size_category.hpp: SizeCategory enum (Static/Dynamic/Trailing)
// - padding.hpp: Padding detection using rfl::named_tuple_t
// - type_info.hpp: TypeTraits<T> comprehensive analysis
// - bounded.hpp: Compile-time max size computation for zero-allocation
//
// Note: memory_map.hpp deprecated - use StructLayout instead

#include "traits/size_category.hpp"
#include "traits/padding.hpp"
#include "traits/type_info.hpp"
#include "traits/bounded.hpp"

namespace sertial {

// ============================================================================
// Quick Reference
// ============================================================================
//
// TypeTraits<T>::category          - SizeCategory (Static/Dynamic/Trailing)
// TypeTraits<T>::has_padding       - bool: struct has padding?
// TypeTraits<T>::packed_size       - size without padding
// TypeTraits<T>::unpacked_size     - size with padding (sizeof)
// TypeTraits<T>::can_memcpy_whole  - bool: safe to memcpy entire struct?
//
// MemoryMap<T>::field_count        - number of fields
// MemoryMap<T>::field_sizes        - array of field sizes
// MemoryMap<T>::memcpy_count       - memcpy operations needed
// MemoryMap<T>::can_single_memcpy  - optimizable to single memcpy?
//
// BoundedSerializable<T>           - concept: type has compile-time max size
// is_bounded_v<T>                  - bool: type is bounded?
// max_serialized_size_v<T>         - compile-time max serialized size
//
// Convenience aliases:
// has_padding_v<T>                 - TypeTraits<T>::has_padding
// packed_size_v<T>                 - TypeTraits<T>::packed_size
// unpacked_size_v<T>               - TypeTraits<T>::unpacked_size
// can_memcpy_whole_v<T>            - TypeTraits<T>::can_memcpy_whole
// field_count_v<T>                 - MemoryMap<T>::field_count
// memcpy_count_v<T>                - MemoryMap<T>::memcpy_count

} // namespace sertial
