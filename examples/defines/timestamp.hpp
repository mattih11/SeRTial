#pragma once
/// @file timestamp.hpp
/// @brief Timestamp field type - used in all messages

#include <rfl.hpp>
#include <cstdint>

namespace examples::defines {

/// @brief Timestamp with seconds and nanoseconds
/// @tparam SecType Type for seconds (default int64_t)
/// @tparam NsecType Type for nanoseconds (default uint32_t)
template<typename SecType = int64_t, typename NsecType = uint32_t>
struct Timestamp {
    SecType sec = SecType{0};    ///< Seconds since epoch
    NsecType nsec = NsecType{0}; ///< Nanoseconds [0, 999999999]
};

// Common type aliases
using TimestampDefault = Timestamp<int64_t, uint32_t>;
using TimestampCompact = Timestamp<uint32_t, uint32_t>;  ///< For shorter time ranges

} // namespace examples::defines
