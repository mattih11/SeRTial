#include <sertial/containers/fixed_vector.hpp>
#include <iostream>
#include <string>
#include <cstdint>

using namespace sertial;

// Example: Sensor data filtering with real-time constraints
struct SensorReading {
    uint64_t timestamp;
    float value;
    uint8_t quality;  // 0-100, quality score
    
    bool operator==(const SensorReading& other) const {
        return timestamp == other.timestamp && 
               value == other.value && 
               quality == other.quality;
    }
};

void print_readings(const char* label, const fixed_vector<SensorReading, 100>& readings) {
    std::cout << label << " (" << readings.size() << " readings):\n";
    for (size_t i = 0; i < readings.size() && i < 5; ++i) {
        std::cout << "  [" << i << "] ts=" << readings[i].timestamp 
                  << " value=" << readings[i].value
                  << " quality=" << (int)readings[i].quality << "\n";
    }
    if (readings.size() > 5) {
        std::cout << "  ... (" << (readings.size() - 5) << " more)\n";
    }
    std::cout << "\n";
}

int main() {
    std::cout << "Fixed Vector Erase/Remove Examples\n";
    std::cout << "===================================\n\n";
    
    // Example 1: Remove low-quality sensor readings
    std::cout << "Example 1: Filter Low-Quality Readings\n";
    std::cout << "----------------------------------------\n";
    
    fixed_vector<SensorReading, 100> sensors;
    sensors.push_back({1000, 23.5f, 95});
    sensors.push_back({1001, 23.7f, 45});  // Low quality
    sensors.push_back({1002, 23.6f, 92});
    sensors.push_back({1003, 99.9f, 15});  // Low quality, outlier
    sensors.push_back({1004, 23.8f, 88});
    sensors.push_back({1005, 23.9f, 30});  // Low quality
    sensors.push_back({1006, 24.0f, 97});
    
    print_readings("Raw sensor data", sensors);
    
    // Remove all readings with quality < 50 (real-time safe, no allocation)
    size_t removed = sensors.remove_if([](const SensorReading& r) {
        return r.quality < 50;
    });
    
    std::cout << "Removed " << removed << " low-quality readings\n\n";
    print_readings("Filtered data", sensors);
    
    // Example 2: Erase single outlier
    std::cout << "Example 2: Erase Single Outlier\n";
    std::cout << "--------------------------------\n";
    
    fixed_vector<float, 50> temperatures = {20.1f, 20.3f, 20.2f, 99.9f, 20.4f, 20.3f};
    
    std::cout << "Temperatures: ";
    for (auto temp : temperatures) std::cout << temp << " ";
    std::cout << "\n";
    
    // Find and erase the outlier (99.9)
    for (auto it = temperatures.begin(); it != temperatures.end(); ++it) {
        if (*it > 50.0f) {
            temperatures.erase(it);
            break;
        }
    }
    
    std::cout << "After removing outlier: ";
    for (auto temp : temperatures) std::cout << temp << " ";
    std::cout << "\n\n";
    
    // Example 3: Remove all occurrences of a value
    std::cout << "Example 3: Remove Duplicate Values\n";
    std::cout << "-----------------------------------\n";
    
    fixed_vector<int, 20> data = {1, 2, 3, 2, 4, 2, 5, 6, 2};
    
    std::cout << "Original data: ";
    for (auto val : data) std::cout << val << " ";
    std::cout << "\n";
    
    removed = data.remove(2);  // Remove all 2s
    
    std::cout << "After removing all 2s (removed " << removed << "): ";
    for (auto val : data) std::cout << val << " ";
    std::cout << "\n\n";
    
    // Example 4: Erase range
    std::cout << "Example 4: Erase Range of Elements\n";
    std::cout << "-----------------------------------\n";
    
    fixed_vector<std::string, 10> messages = {
        "msg0", "msg1", "msg2", "msg3", "msg4", "msg5", "msg6"
    };
    
    std::cout << "Messages: ";
    for (const auto& msg : messages) std::cout << msg << " ";
    std::cout << "\n";
    
    // Erase middle 3 messages (msg2, msg3, msg4)
    messages.erase(messages.begin() + 2, messages.begin() + 5);
    
    std::cout << "After erasing range [2, 5): ";
    for (const auto& msg : messages) std::cout << msg << " ";
    std::cout << "\n\n";
    
    // Example 5: Real-time buffer management
    std::cout << "Example 5: Real-Time Event Buffer\n";
    std::cout << "----------------------------------\n";
    
    struct Event {
        uint64_t timestamp;
        std::string type;
        bool processed;
    };
    
    fixed_vector<Event, 100> event_buffer;
    
    // Add events
    event_buffer.push_back({1000, "sensor", true});
    event_buffer.push_back({1001, "motor", false});
    event_buffer.push_back({1002, "sensor", true});
    event_buffer.push_back({1003, "alert", false});
    event_buffer.push_back({1004, "sensor", true});
    
    std::cout << "Events in buffer: " << event_buffer.size() << "\n";
    
    // Remove all processed events (common pattern in real-time systems)
    removed = event_buffer.remove_if([](const Event& e) {
        return e.processed;
    });
    
    std::cout << "Removed " << removed << " processed events\n";
    std::cout << "Remaining events: " << event_buffer.size() << "\n";
    for (const auto& evt : event_buffer) {
        std::cout << "  - " << evt.type << " at " << evt.timestamp << "\n";
    }
    std::cout << "\n";
    
    std::cout << "Key Features:\n";
    std::cout << "  ✓ Zero heap allocation (all stack-based)\n";
    std::cout << "  ✓ Real-time safe (deterministic O(n) performance)\n";
    std::cout << "  ✓ STL-compatible erase/remove operations\n";
    std::cout << "  ✓ Works with non-trivial types (strings, structs)\n";
    std::cout << "  ✓ Custom predicates for flexible filtering\n";
    
    return 0;
}
