#include <sertial/sertial.hpp>
#include <sertial/containers/ring_buffer.hpp>
#include <sertial/core/layout/block_types.hpp>
#include <cassert>
#include <iostream>
#include <cstdint>

using namespace sertial;
using namespace sertial::detail;  // For BlockType

// Test struct with RingBuffer
struct SensorHistory {
    uint32_t sensor_id;
    RingBuffer<float, 10> readings;  // Last 10 readings
    uint64_t timestamp;
};

int main() {
    std::cout << "Testing RingBuffer Serialization\n";
    std::cout << "=================================\n\n";
    
    // Create sensor history with some readings
    SensorHistory history;
    history.sensor_id = 42;
    history.timestamp = 1234567890;
    
    // Add 7 readings (less than capacity)
    for (int i = 0; i < 7; ++i) {
        history.readings.push_back(10.0f + i * 0.5f);
    }
    
    std::cout << "Original data:\n";
    std::cout << "  sensor_id: " << history.sensor_id << "\n";
    std::cout << "  readings.size(): " << history.readings.size() << "\n";
    std::cout << "  readings: [";
    for (size_t i = 0; i < history.readings.size(); ++i) {
        std::cout << history.readings[i];
        if (i + 1 < history.readings.size()) std::cout << ", ";
    }
    std::cout << "]\n";
    std::cout << "  timestamp: " << history.timestamp << "\n\n";
    
    // Analyze compile-time properties
    using Layout = StructLayout<SensorHistory>;
    std::cout << "Compile-time analysis:\n";
    std::cout << "  sizeof(SensorHistory): " << sizeof(SensorHistory) << " bytes\n";
    std::cout << "  max_packed_size: " << Layout::max_packed_size << " bytes\n";
    std::cout << "  base_packed_size: " << Layout::base_packed_size << " bytes\n";
    std::cout << "  has_variable_fields: " << (Layout::has_variable_fields ? "yes" : "no") << "\n";
    std::cout << "  num_fields: " << Layout::num_fields << "\n";
    
    // Count dynamic blocks
    size_t dynamic_count = 0;
    for (const auto& block : Layout::execution_order) {
        if (block.type == BlockType::Dynamic) {
            dynamic_count++;
        }
    }
    std::cout << "  dynamic_block_count: " << dynamic_count << "\n\n";
    
    // Calculate runtime packed size
    size_t runtime_size = packed_size_of(history);
    std::cout << "Runtime packed size: " << runtime_size << " bytes\n";
    std::cout << "  (Only " << history.readings.size() << " elements serialized, not all 10)\n\n";
    
    // Serialize
    auto buffer = serialize(history);
    std::cout << "Serialization successful!\n";
    std::cout << "  Buffer size: " << buffer.size() << " bytes\n\n";
    
    // Verify size matches runtime calculation
    assert(buffer.size() == runtime_size);
    std::cout << "✓ Buffer size matches calculated size\n\n";
    
    // Deserialize
    auto restored = deserialize<SensorHistory>(buffer.view());
    assert(restored.has_value());
    std::cout << "Deserialization successful!\n\n";
    
    // Verify data
    std::cout << "Restored data:\n";
    std::cout << "  sensor_id: " << restored->sensor_id << "\n";
    std::cout << "  readings.size(): " << restored->readings.size() << "\n";
    std::cout << "  readings: [";
    for (size_t i = 0; i < restored->readings.size(); ++i) {
        std::cout << restored->readings[i];
        if (i + 1 < restored->readings.size()) std::cout << ", ";
    }
    std::cout << "]\n";
    std::cout << "  timestamp: " << restored->timestamp << "\n\n";
    
    // Verify correctness
    assert(restored->sensor_id == history.sensor_id);
    assert(restored->timestamp == history.timestamp);
    assert(restored->readings.size() == history.readings.size());
    
    for (size_t i = 0; i < history.readings.size(); ++i) {
        assert(restored->readings[i] == history.readings[i]);
    }
    
    std::cout << "✓ All data verified correctly!\n\n";
    
    // Test with full buffer
    std::cout << "Testing with full buffer (15 elements, overwrites oldest 5)...\n";
    SensorHistory history2;
    history2.sensor_id = 99;
    history2.timestamp = 9876543210;
    
    for (int i = 0; i < 15; ++i) {
        history2.readings.push_back(20.0f + i);
    }
    
    std::cout << "  Buffer size: " << history2.readings.size() << " (full)\n";
    std::cout << "  First element: " << history2.readings.front() << " (oldest)\n";
    std::cout << "  Last element: " << history2.readings.back() << " (newest)\n\n";
    
    size_t runtime_size2 = packed_size_of(history2);
    std::cout << "  Runtime packed size: " << runtime_size2 << " bytes\n";
    std::cout << "  Max packed size: " << Layout::max_packed_size << " bytes\n";
    std::cout << "  Base packed size: " << Layout::base_packed_size << " bytes\n";
    
    auto buffer2 = serialize(history2);
    std::cout << "  Serialized size: " << buffer2.size() << " bytes\n";
    assert(buffer2.size() == runtime_size2);
    
    auto restored2 = deserialize<SensorHistory>(buffer2.view());
    assert(restored2.has_value());
    assert(restored2->readings.size() == 10);  // Full capacity
    assert(restored2->sensor_id == history2.sensor_id);
    
    // Verify all 10 elements match
    for (size_t i = 0; i < 10; ++i) {
        assert(restored2->readings[i] == history2.readings[i]);
    }
    
    std::cout << "✓ Full buffer serialization verified!\n\n";
    
    // Test with wrap-around
    std::cout << "Testing wrap-around behavior...\n";
    SensorHistory history3;
    history3.sensor_id = 77;
    history3.timestamp = 5555555555;
    
    // Add 5 elements first
    for (int i = 0; i < 5; ++i) {
        history3.readings.push_back(100.0f + i);
    }
    
    std::cout << "  Added 5 elements, head=" << history3.readings.head_index() 
              << " tail=" << history3.readings.tail_index() << "\n";
    
    // Add 8 more (total 13, wraps around, keeps last 10)
    for (int i = 5; i < 13; ++i) {
        history3.readings.push_back(100.0f + i);
    }
    
    std::cout << "  Added 13 total, head=" << history3.readings.head_index() 
              << " tail=" << history3.readings.tail_index() << " (wrapped)\n";
    std::cout << "  Size: " << history3.readings.size() << "\n";
    std::cout << "  Contents: [";
    for (size_t i = 0; i < history3.readings.size(); ++i) {
        std::cout << history3.readings[i];
        if (i + 1 < history3.readings.size()) std::cout << ", ";
    }
    std::cout << "]\n\n";
    
    auto buffer3 = serialize(history3);
    auto restored3 = deserialize<SensorHistory>(buffer3.view());
    assert(restored3.has_value());
    assert(restored3->readings.size() == 10);
    
    std::cout << "  Restored contents: [";
    for (size_t i = 0; i < restored3->readings.size(); ++i) {
        std::cout << restored3->readings[i];
        if (i + 1 < restored3->readings.size()) std::cout << ", ";
    }
    std::cout << "]\n";
    
    // Verify wrap-around data integrity
    for (size_t i = 0; i < history3.readings.size(); ++i) {
        assert(restored3->readings[i] == history3.readings[i]);
    }
    
    std::cout << "✓ Wrap-around serialization verified!\n\n";
    
    std::cout << "========================================\n";
    std::cout << "All RingBuffer serialization tests passed! ✓\n";
    std::cout << "========================================\n";
    std::cout << "\nKey takeaways:\n";
    std::cout << "  • RingBuffer is serialized as a variable-length field\n";
    std::cout << "  • Only the current size() elements are serialized (not full capacity)\n";
    std::cout << "  • Runtime size = base_size + 4 (length) + size() * element_size\n";
    std::cout << "  • Wrap-around state is handled transparently (1-2 memory spans)\n";
    std::cout << "  • Perfect for efficient message history storage!\n";
    
    return 0;
}
