/// Test: Serialization - High-level serialize/deserialize API
/// Tests optimized binary serialization with compile-time analysis
#include "test_framework.hpp"
#include <sertial/io/optimized_binary.hpp>
#include <sertial/containers/fixed_string.hpp>
#include <sertial/containers/fixed_vector.hpp>
#include <cmath>

using namespace sertial;
using namespace sertial::test;

// ============================================================================
// Test Structs (using bounded types for optimized serialization)
// ============================================================================

struct Vec3 {
    float x, y, z;
};

struct Player {
    fixed_string<32> name;
    int32_t health;
    Vec3 position;
    fixed_vector<int32_t, 16> inventory;
};

struct GameState {
    uint64_t timestamp;
    fixed_vector<Player, 8> players;
    fixed_string<64> map_name;
};

struct ComplexData {
    bool enabled;
    int8_t small_value;
    int64_t large_value;
    double ratio;
    fixed_string<128> description;
    fixed_vector<Vec3, 16> waypoints;
};

// ============================================================================
// Individual Test Functions
// ============================================================================

namespace tests {

bool simple_struct_test() {
    TEST_SECTION("Test 1: Simple Struct (Vec3)");
    
    Vec3 original{1.5f, 2.5f, 3.5f};
    
    auto data = sertial::serialize(original);
    TEST_PRINT("  Serialized: " << data.size() << " bytes");
    
    auto restored = sertial::deserialize<Vec3>(data.view());
    TEST_ASSERT(restored.has_value(), "Deserialization succeeds");
    TEST_ASSERT(std::abs(restored->x - 1.5f) < 0.001f, "x matches");
    TEST_ASSERT(std::abs(restored->y - 2.5f) < 0.001f, "y matches");
    TEST_ASSERT(std::abs(restored->z - 3.5f) < 0.001f, "z matches");
    
    TEST_PRINT("  Original: (" << original.x << ", " << original.y << ", " << original.z << ")");
    TEST_PRINT("  Restored: (" << restored->x << ", " << restored->y << ", " << restored->z << ")");
    
    return true;
}

bool player_struct_test() {
    TEST_SECTION("Test 2: Struct with fixed_string and fixed_vector (Player)");
    
    Player original{};
    original.name = "Alice";
    original.health = 100;
    original.position = {10.0f, 20.0f, 30.0f};
    original.inventory.push_back(1);
    original.inventory.push_back(2);
    original.inventory.push_back(3);
    original.inventory.push_back(4);
    original.inventory.push_back(5);
    
    auto data = sertial::serialize(original);
    TEST_PRINT("  Serialized: " << data.size() << " bytes");
    
    auto restored = sertial::deserialize<Player>(data.view());
    TEST_ASSERT(restored.has_value(), "Deserialization succeeds");
    TEST_ASSERT(std::string_view(restored->name) == "Alice", "name matches");
    TEST_ASSERT(restored->health == 100, "health matches");
    TEST_ASSERT(std::abs(restored->position.x - 10.0f) < 0.001f, "position.x matches");
    TEST_ASSERT(restored->inventory.size() == 5, "inventory size matches");
    TEST_ASSERT(restored->inventory[0] == 1, "inventory[0] matches");
    TEST_ASSERT(restored->inventory[4] == 5, "inventory[4] matches");
    
    TEST_PRINT("  Original: " << std::string_view(original.name) << ", HP=" << original.health);
    TEST_PRINT("  Restored: " << std::string_view(restored->name) << ", HP=" << restored->health);
    
    return true;
}

bool nested_struct_test() {
    TEST_SECTION("Test 3: Deeply Nested Structures (GameState)");
    
    GameState original{};
    original.timestamp = 1234567890ULL;
    original.map_name = "level_1";
    
    Player p1{};
    p1.name = "Alice";
    p1.health = 100;
    p1.position = {1.0f, 2.0f, 3.0f};
    p1.inventory.push_back(1);
    p1.inventory.push_back(2);
    p1.inventory.push_back(3);
    
    Player p2{};
    p2.name = "Bob";
    p2.health = 85;
    p2.position = {4.0f, 5.0f, 6.0f};
    p2.inventory.push_back(4);
    p2.inventory.push_back(5);
    
    original.players.push_back(p1);
    original.players.push_back(p2);
    
    auto data = sertial::serialize(original);
    TEST_PRINT("  Serialized: " << data.size() << " bytes");
    
    auto restored = sertial::deserialize<GameState>(data.view());
    TEST_ASSERT(restored.has_value(), "Deserialization succeeds");
    TEST_ASSERT(restored->timestamp == 1234567890ULL, "timestamp matches");
    TEST_ASSERT(restored->players.size() == 2, "players count matches");
    TEST_ASSERT(std::string_view(restored->players[0].name) == "Alice", "player 0 name matches");
    TEST_ASSERT(std::string_view(restored->players[1].name) == "Bob", "player 1 name matches");
    TEST_ASSERT(restored->players[1].health == 85, "player 1 health matches");
    TEST_ASSERT(std::string_view(restored->map_name) == "level_1", "map_name matches");
    
    TEST_PRINT("  Players: " << restored->players.size());
    TEST_PRINT("  Map: " << std::string_view(restored->map_name));
    TEST_PRINT("  Timestamp: " << restored->timestamp);
    
    return true;
}

bool complex_data_test() {
    TEST_SECTION("Test 4: Complex Data Types");
    
    ComplexData original{};
    original.enabled = true;
    original.small_value = -42;
    original.large_value = 9876543210LL;
    original.ratio = 3.14159265359;
    original.description = "Test description";
    original.waypoints.push_back({1.0f, 2.0f, 3.0f});
    original.waypoints.push_back({4.0f, 5.0f, 6.0f});
    original.waypoints.push_back({7.0f, 8.0f, 9.0f});
    
    auto data = sertial::serialize(original);
    TEST_PRINT("  Serialized: " << data.size() << " bytes");
    
    auto restored = sertial::deserialize<ComplexData>(data.view());
    TEST_ASSERT(restored.has_value(), "Deserialization succeeds");
    TEST_ASSERT(restored->enabled == true, "enabled matches");
    TEST_ASSERT(restored->small_value == -42, "small_value matches");
    TEST_ASSERT(restored->large_value == 9876543210LL, "large_value matches");
    TEST_ASSERT(std::abs(restored->ratio - 3.14159265359) < 0.0001, "ratio matches");
    TEST_ASSERT(std::string_view(restored->description) == "Test description", "description matches");
    TEST_ASSERT(restored->waypoints.size() == 3, "waypoints count matches");
    
    TEST_PRINT("  Enabled: " << restored->enabled);
    TEST_PRINT("  Ratio: " << restored->ratio);
    TEST_PRINT("  Waypoints: " << restored->waypoints.size());
    
    return true;
}

bool error_handling_test() {
    TEST_SECTION("Test 5: Error Handling");
    
    // Truncated data
    std::array<std::byte, 2> truncated = {std::byte{1}, std::byte{2}};
    auto read_truncated = sertial::deserialize<Vec3>(truncated);
    TEST_ASSERT(!read_truncated.has_value(), "Truncated data fails gracefully");
    TEST_PRINT("  Truncated data: failed as expected [OK]");
    
    return true;
}

bool multiple_roundtrips_test() {
    TEST_SECTION("Test 6: Multiple Round-Trips");
    
    Vec3 v{1.0f, 2.0f, 3.0f};
    
    for (int i = 0; i < 5; ++i) {
        auto data = sertial::serialize(v);
        auto restored = sertial::deserialize<Vec3>(data.view());
        TEST_ASSERT(restored.has_value(), "Roundtrip " << i << " succeeds");
        v = *restored;
    }
    
    TEST_ASSERT(std::abs(v.x - 1.0f) < 0.001f, "x preserved after 5 roundtrips");
    TEST_ASSERT(std::abs(v.y - 2.0f) < 0.001f, "y preserved after 5 roundtrips");
    TEST_ASSERT(std::abs(v.z - 3.0f) < 0.001f, "z preserved after 5 roundtrips");
    
    TEST_PRINT("  5 round-trips completed successfully [OK]");
    
    return true;
}

bool size_computation_test() {
    TEST_SECTION("Test 7: Size Computation");
    
    // Float size
    auto float_data = sertial::serialize(1.0f);
    TEST_ASSERT_EQ(float_data.size(), 4u, "Float is 4 bytes");
    
    // Vec3 size (3 floats, no padding)
    Vec3 v{1.0f, 2.0f, 3.0f};
    auto vec3_data = sertial::serialize(v);
    TEST_PRINT("  Float size: " << float_data.size() << " bytes");
    TEST_PRINT("  Vec3 expected: 12 bytes (3 floats, no padding)");
    TEST_PRINT("  Vec3 actual: " << vec3_data.size() << " bytes");
    TEST_ASSERT_EQ(vec3_data.size(), 12u, "Vec3 is 12 bytes (3 floats)");
    
    return true;
}

} // namespace tests

// ============================================================================
// Test Suite
// ============================================================================

struct SerializationTests : TestSuite<SerializationTests> {
    static constexpr const char* name = "SeRTial - Serialization Tests";
    
    static bool run() {
        if (!tests::simple_struct_test()) return false;
        if (!tests::player_struct_test()) return false;
        if (!tests::nested_struct_test()) return false;
        if (!tests::complex_data_test()) return false;
        if (!tests::error_handling_test()) return false;
        if (!tests::multiple_roundtrips_test()) return false;
        if (!tests::size_computation_test()) return false;
        
        TEST_PRINT("");
        TEST_PRINT("Serialization Tests Complete");
        TEST_PRINT("  [OK] Automatic struct serialization");
        TEST_PRINT("  [OK] No manual BinaryReflector needed");
        TEST_PRINT("  [OK] Nested structs supported");
        TEST_PRINT("  [OK] Works with bounded container types");
        TEST_PRINT("  [OK] Full round-trip fidelity");
        TEST_PRINT("  [OK] Zero overhead (direct memcpy regions)");
        
        return true;
    }
};

// ============================================================================
// Main
// ============================================================================

int main() {
    return TestRunner::run<SerializationTests>();
}
