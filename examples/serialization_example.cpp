/// Example: Optimized Binary Serialization with SeRTial
///
/// This example demonstrates all the convenience functions for high-performance
/// binary serialization and deserialization of message types.
///
/// Key Features Shown:
/// - Zero-allocation serialization with Message<T>::serialize()
/// - Type-deduced convenience functions: serialize(), deserialize()
/// - Compile-time buffer sizing (no runtime allocation)
/// - Static buffers with stack allocation
/// - Memory layout analysis and optimization info
///
/// Build: cmake --build build --target serialization_example
/// Run:   ./build/serialization_example

#include <sertial/sertial.hpp>
#include <sertial/debug/print_utils.hpp>
#include <iostream>
#include <chrono>
#include <iomanip>

// Include your message definitions
#include "defines/defines.hpp"
#include "messages/messages.hpp"

using namespace sertial;
using namespace examples::defines;
using namespace examples::messages;
using sertial::debug::print_bytes;

// ============================================================================
// Example 1: Basic Serialization (Zero-Allocation)
// ============================================================================

void example_basic_serialization() {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "Example 1: Basic Serialization (Zero-Allocation)\n";
    std::cout << "============================================================\n";
    
    // Create a Position message
    Position<> pos;
    pos.header.frame_id = 1;  // Coordinate frame ID
    pos.header.seq = 42;
    pos.header.stamp.sec = 1234567890;
    pos.header.stamp.nsec = 123456789;
    pos.pose.position = Point3D<>{1.5f, 2.5f, 3.5f};
    pos.pose.orientation = Quaternion<>{0.0f, 0.0f, 0.0f, 1.0f};
    
    // Method 1: Using Message<T>::serialize() - returns stack-allocated buffer
    auto result = Message<Position<>>::serialize(pos);
    
    std::cout << "\n  Position message serialized:\n";
    std::cout << "    frame_id: " << pos.header.frame_id << "\n";
    std::cout << "    seq: " << pos.header.seq << "\n";
    std::cout << "    position: (" << pos.pose.position.x << ", " 
              << pos.pose.position.y << ", " << pos.pose.position.z << ")\n";
    print_bytes(result.view());
    
    // Method 2: Using convenience function (type-deduced)
    auto result2 = serialize(pos);  // Same as Message<Position<>>::serialize(pos)
    
    std::cout << "\n  serialize(pos) - same result, less typing:\n";
    print_bytes(result2.view());
    
    // Key point: Buffer is stack-allocated!
    std::cout << "\n  [OK] Buffer is std::array<byte, " << Message<Position<>>::max_buffer_size 
              << "> (stack-allocated)\n";
    std::cout << "  [OK] Actual bytes used: " << result.size << "\n";
    std::cout << "  [OK] ZERO heap allocations!\n";
}

// ============================================================================
// Example 2: Deserialization
// ============================================================================

void example_deserialization() {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "Example 2: Deserialization\n";
    std::cout << "============================================================\n";
    
    // Create and serialize an IMU message
    Imu<> imu;
    imu.header.frame_id = 2;  // Coordinate frame ID
    imu.header.seq = 100;
    imu.header.stamp.sec = 1000;
    imu.header.stamp.nsec = 500;
    imu.orientation = Quaternion<>{0.1f, 0.2f, 0.3f, 0.9f};
    imu.angular_velocity = Point3D<>{0.01f, 0.02f, 0.03f};
    imu.linear_acceleration = Point3D<>{0.0f, 0.0f, 9.81f};
    
    auto serialized = serialize(imu);
    
    std::cout << "\n  Original IMU:\n";
    std::cout << "    angular_velocity: (" << imu.angular_velocity.x << ", "
              << imu.angular_velocity.y << ", " << imu.angular_velocity.z << ")\n";
    std::cout << "    linear_accel: (" << imu.linear_acceleration.x << ", "
              << imu.linear_acceleration.y << ", " << imu.linear_acceleration.z << ")\n";
    std::cout << "    serialized size: " << serialized.size() << " bytes\n";
    
    // Method 1: Using Message<T>::deserialize()
    auto restored = Message<Imu<>>::deserialize(serialized.view());
    
    if (restored) {
        std::cout << "\n  Deserialized (Message<Imu<>>::deserialize):\n";
        std::cout << "    angular_velocity: (" << restored->angular_velocity.x << ", "
                  << restored->angular_velocity.y << ", " << restored->angular_velocity.z << ")\n";
        std::cout << "    linear_accel: (" << restored->linear_acceleration.x << ", "
                  << restored->linear_acceleration.y << ", " << restored->linear_acceleration.z << ")\n";
    }
    
    // Method 2: Using convenience function
    auto restored2 = deserialize<Imu<>>(serialized.view());
    
    if (restored2) {
        std::cout << "\n  Deserialized (deserialize<Imu<>>):\n";
        std::cout << "    [OK] Same result with less typing!\n";
    }
    
    // Method 3: Deserialize into existing object (avoids default construction)
    Imu<> reused_imu;
    bool success = Message<Imu<>>::deserialize_into(serialized.view(), reused_imu);
    
    if (success) {
        std::cout << "\n  Deserialized into existing object:\n";
        std::cout << "    [OK] Useful when reusing message objects in a loop\n";
    }
    
    // Show that simpler types work perfectly
    std::cout << "\n  Simple type roundtrip (Point3D<float>):\n";
    Point3D<float> pt{1.0f, 2.0f, 3.0f};
    auto pt_serialized = serialize(pt);
    auto pt_restored = deserialize<Point3D<float>>(pt_serialized.view());
    if (pt_restored) {
        std::cout << "    [OK] Original:  (" << pt.x << ", " << pt.y << ", " << pt.z << ")\n";
        std::cout << "    [OK] Restored:  (" << pt_restored->x << ", " << pt_restored->y << ", " << pt_restored->z << ")\n";
    }
}

// ============================================================================
// Example 3: Compile-Time Memory Layout Analysis
// ============================================================================

void example_memory_analysis() {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "Example 3: Compile-Time Memory Layout Analysis\n";
    std::cout << "============================================================\n";
    
    std::cout << "\n  Point3D<float>:\n";
    std::cout << "    sizeof:        " << sizeof(Point3D<float>) << " bytes\n";
    std::cout << "    packed_size:   " << Message<Point3D<float>>::packed_size << " bytes\n";
    std::cout << "    has_padding:   " << (Message<Point3D<float>>::has_padding ? "yes" : "no") << "\n";
    std::cout << "    can_memcpy:    " << (Message<Point3D<float>>::can_single_memcpy ? "yes" : "no") << "\n";
    
    std::cout << "\n  Header<>:\n";
    std::cout << "    sizeof:        " << sizeof(Header<>) << " bytes\n";
    std::cout << "    packed_size:   " << Message<Header<>>::packed_size << " bytes\n";
    std::cout << "    max_buffer:    " << Message<Header<>>::max_buffer_size << " bytes\n";
    std::cout << "    field_count:   " << Message<Header<>>::field_count << "\n";
    std::cout << "    memcpy_count:  " << Message<Header<>>::memcpy_count << "\n";
    
    std::cout << "\n  Position<>:\n";
    std::cout << "    sizeof:        " << sizeof(Position<>) << " bytes\n";
    std::cout << "    max_buffer:    " << Message<Position<>>::max_buffer_size << " bytes\n";
    std::cout << "    field_count:   " << Message<Position<>>::field_count << "\n";
    
    // Static assertions (compile-time guarantees)
    static_assert(Message<Point3D<float>>::is_bounded, "Point3D must be bounded");
    static_assert(Message<Position<>>::max_buffer_size > 0, "Buffer size must be positive");
    
    std::cout << "\n  [OK] All size information computed at COMPILE TIME\n";
    std::cout << "  [OK] No runtime overhead for buffer sizing\n";
}



// ============================================================================
// Example 4b: Static Buffer (True Zero-Allocation)
// ============================================================================

void example_static_buffer() {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "Example 4b: Static Buffer (True Zero-Allocation)\n";
    std::cout << "============================================================\n";
    
    // Create a simple position
    Point3D<float> point{1.0f, 2.0f, 3.0f};
    
    // Method 1: serialize() - returns a static_buffer sized exactly for the type
    auto buf = serialize(point);
    
    std::cout << "\n  serialize(point):\n";
    std::cout << "    Buffer capacity: " << buf.capacity() << " bytes (compile-time)\n";
    std::cout << "    Buffer used:     " << buf.size() << " bytes\n";
    std::cout << "    Type: static_buffer<" << buf.capacity() << "> (STACK allocated)\n";
    print_bytes(buf.view());
    
    // Method 2: Serialize different types and reuse the result
    Position<> pos{};
    pos.header.frame_id = 1;
    pos.header.seq = 42;
    pos.pose.position = point;
    
    auto pos_buf = serialize(pos);  // Returns static_buffer sized to max capacity
    std::cout << "\n  Position serialized:\n";
    std::cout << "    Buffer capacity: " << pos_buf.capacity() << ", Used: " << pos_buf.size() << "\n";
    
    // Deserialize back
    auto restored = deserialize<Position<>>(pos_buf);
    if (restored) {
        std::cout << "    Restored position: (" << restored->pose.position.x << ", "
                  << restored->pose.position.y << ", " << restored->pose.position.z << ")\n";
    }
    
    // Method 3: Common pre-defined sizes
    static_buffer_256 buf256;  // Alias for static_buffer<256>
    static_buffer_1k buf1k;    // Alias for static_buffer<1024>
    (void)buf256; (void)buf1k;  // Suppress unused warnings
    
    std::cout << "\n  Pre-defined buffer sizes available:\n";
    std::cout << "    static_buffer_64, static_buffer_128, static_buffer_256\n";
    std::cout << "    static_buffer_512, static_buffer_1k, static_buffer_4k, static_buffer_64k\n";
    
    std::cout << "\n  [OK] ZERO heap allocations throughout!\n";
    std::cout << "  [OK] Perfect for embedded/real-time systems\n";
}

// ============================================================================
// Example 5: Performance Benchmark
// ============================================================================

void example_performance() {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "Example 5: Performance Benchmark\n";
    std::cout << "============================================================\n";
    
    constexpr int ITERATIONS = 100000;
    
    // Create test data
    Position<> pos;
    pos.header.frame_id = 1;  // Coordinate frame ID
    pos.header.seq = 42;
    pos.header.stamp.sec = 1234567890;
    pos.header.stamp.nsec = 123456789;
    pos.pose.position = Point3D<>{1.5f, 2.5f, 3.5f};
    pos.pose.orientation = Quaternion<>{0.0f, 0.0f, 0.0f, 1.0f};
    
    // Warm up
    for (int i = 0; i < 1000; ++i) {
        auto r = serialize(pos);
        (void)r;
    }
    
    // Benchmark serialization
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        auto result = serialize(pos);
        (void)result;  // Prevent optimization
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto ser_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double ser_per_op = static_cast<double>(ser_time) / ITERATIONS;
    
    // Benchmark deserialization
    auto serialized = serialize(pos);
    
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        auto restored = deserialize<Position<>>(serialized.view());
        (void)restored;  // Prevent optimization
    }
    end = std::chrono::high_resolution_clock::now();
    
    auto deser_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double deser_per_op = static_cast<double>(deser_time) / ITERATIONS;
    
    std::cout << "\n  Benchmark: Position<> (" << serialized.size() << " bytes)\n";
    std::cout << "    Iterations:     " << ITERATIONS << "\n";
    std::cout << "    Serialize:      " << std::fixed << std::setprecision(1) 
              << ser_per_op << " ns/op\n";
    std::cout << "    Deserialize:    " << deser_per_op << " ns/op\n";
    std::cout << "    Throughput:     " << std::setprecision(2)
              << (1e9 / ser_per_op) << " msg/sec (serialize)\n";
    std::cout << "                    " << (1e9 / deser_per_op) << " msg/sec (deserialize)\n";
}

// ============================================================================
// Example 6: Error Handling
// ============================================================================

void example_error_handling() {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "Example 6: Error Handling\n";
    std::cout << "============================================================\n";
    
    // Truncated data
    std::vector<std::byte> truncated = {std::byte{0x01}, std::byte{0x02}};
    
    auto result = deserialize<Position<>>(truncated);
    
    if (!result) {
        std::cout << "\n  Truncated data handling:\n";
        std::cout << "    [OK] Deserialization failed safely\n";
        std::cout << "    [OK] No crash, no undefined behavior\n";
    }
    
    // Empty data
    std::vector<std::byte> empty;
    auto result2 = deserialize<Point3D<float>>(empty);
    
    if (!result2) {
        std::cout << "\n  Empty data handling:\n";
        std::cout << "    [OK] Deserialization failed safely\n";
    }
    
    // Valid data works
    auto valid = serialize(Point3D<float>{1.0f, 2.0f, 3.0f});
    auto result3 = deserialize<Point3D<float>>(valid.view());
    
    if (result3) {
        std::cout << "\n  Valid data:\n";
        std::cout << "    [OK] Deserialization succeeded\n";
        std::cout << "    [OK] Value: (" << result3->x << ", " << result3->y << ", " << result3->z << ")\n";
    }
}



// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  SeRTial Serialization Example                           ║\n";
    std::cout << "║  Zero-Allocation Binary Serialization for C++20          ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    
    example_basic_serialization();
    example_deserialization();
    example_memory_analysis();
    example_static_buffer();
    example_performance();
    example_error_handling();
    
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "All examples completed successfully!\n";
    std::cout << "============================================================\n\n";
    
    return 0;
}
