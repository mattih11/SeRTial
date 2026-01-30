#pragma once

#include <rfl.hpp>
#include "../defines/timestamp.hpp"

namespace examples::messages {

/// Standard message header with sequence number and timestamp
template<typename TTimestamp = defines::Timestamp<>>
struct Header {
    uint32_t seq = 0;       // Sequence number
    TTimestamp stamp;       // Time stamp
    uint32_t frame_id = 0;  // Coordinate frame ID
};

} // namespace examples::messages
