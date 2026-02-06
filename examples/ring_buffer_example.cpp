#include <sertial/containers/ring_buffer.hpp>
#include <iostream>
#include <cstdint>
#include <cmath>
#include <optional>

// Example: CommRaT-style message buffer with timestamp-based retrieval
// This demonstrates the ring buffer use case described in the specification

struct TimsMessage {
    uint64_t timestamp;
    uint32_t seq_number;
    float position[3];
    
    TimsMessage() = default;
    TimsMessage(uint64_t ts, uint32_t seq, float x, float y, float z)
        : timestamp(ts), seq_number(seq), position{x, y, z} {}
};

// Producer: Stores messages in a fixed-capacity ring buffer
class MessageProducer {
public:
    static constexpr size_t HISTORY_SIZE = 100;
    
    void add_message(const TimsMessage& msg) {
        buffer_.push_back(msg);
    }
    
    // Get message closest to target timestamp
    std::optional<TimsMessage> get_at_timestamp(uint64_t target_ts) const {
        if (buffer_.empty()) {
            return std::nullopt;
        }
        
        // Linear search for closest timestamp (RACK pattern)
        size_t best_idx = 0;
        uint64_t min_diff = UINT64_MAX;
        
        for (size_t i = 0; i < buffer_.size(); ++i) {
            uint64_t ts = buffer_[i].timestamp;
            uint64_t diff = (ts >= target_ts) ? (ts - target_ts) : (target_ts - ts);
            
            if (diff < min_diff) {
                min_diff = diff;
                best_idx = i;
            } else if (ts > target_ts) {
                break;  // Timestamps increasing, found minimum
            }
        }
        
        return buffer_[best_idx];
    }
    
    // Get latest message
    std::optional<TimsMessage> get_latest() const {
        if (buffer_.empty()) {
            return std::nullopt;
        }
        return buffer_.back();
    }
    
    size_t history_count() const { return buffer_.size(); }
    
private:
    sertial::RingBuffer<TimsMessage, HISTORY_SIZE> buffer_;
};

int main() {
    MessageProducer producer;
    
    std::cout << "CommRaT Ring Buffer Example\n";
    std::cout << "============================\n\n";
    
    // Producer: Simulate realtime message generation
    std::cout << "Producer: Adding 150 messages (buffer capacity: 100)...\n";
    for (uint64_t t = 1000; t <= 1149; ++t) {
        float phase = (t - 1000) * 0.1f;
        TimsMessage msg{
            t,
            static_cast<uint32_t>(t - 1000),
            std::sin(phase), std::cos(phase), phase
        };
        producer.add_message(msg);
    }
    
    std::cout << "History size: " << producer.history_count() << " messages\n";
    std::cout << "(Oldest 50 messages were automatically overwritten)\n\n";
    
    // Consumer: Retrieve messages by timestamp
    std::cout << "Consumer: Querying historical data...\n";
    
    // Test 1: Get latest message
    if (auto msg = producer.get_latest()) {
        std::cout << "  Latest message:\n";
        std::cout << "    timestamp: " << msg->timestamp << "\n";
        std::cout << "    seq: " << msg->seq_number << "\n";
        std::cout << "    position: [" << msg->position[0] << ", "
                  << msg->position[1] << ", " << msg->position[2] << "]\n\n";
    }
    
    // Test 2: Get message at specific timestamp
    uint64_t query_ts = 1100;
    std::cout << "  Query timestamp: " << query_ts << "\n";
    if (auto msg = producer.get_at_timestamp(query_ts)) {
        std::cout << "    Found message:\n";
        std::cout << "      timestamp: " << msg->timestamp << "\n";
        std::cout << "      seq: " << msg->seq_number << "\n";
        std::cout << "      position: [" << msg->position[0] << ", "
                  << msg->position[1] << ", " << msg->position[2] << "]\n\n";
    }
    
    // Test 3: Get oldest available message
    uint64_t old_ts = 1000;  // Too old, was overwritten
    std::cout << "  Query old timestamp: " << old_ts << " (was overwritten)\n";
    if (auto msg = producer.get_at_timestamp(old_ts)) {
        std::cout << "    Closest available message:\n";
        std::cout << "      timestamp: " << msg->timestamp << "\n";
        std::cout << "      seq: " << msg->seq_number << "\n";
        std::cout << "      (Note: Returns oldest available, not exact match)\n\n";
    }
    
    std::cout << "Key Features Demonstrated:\n";
    std::cout << "  ✓ Fixed capacity (100 messages)\n";
    std::cout << "  ✓ Zero allocation (stack-based)\n";
    std::cout << "  ✓ Automatic overwrite of oldest data\n";
    std::cout << "  ✓ O(1) push operations\n";
    std::cout << "  ✓ Timestamp-based retrieval\n";
    std::cout << "  ✓ Realtime-safe (no malloc/free)\n";
    
    return 0;
}
