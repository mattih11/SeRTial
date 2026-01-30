#include <iostream>
#include <iomanip>
#include <sertial/sertial.hpp>
#include <sertial/debug/print_utils.hpp>

using namespace sertial;
using sertial::debug::print_type_info;

// ============================================================================
// Example Structs
// ============================================================================

// Simple struct without padding
struct Vec3 {
    float x, y, z;
};

// Struct with padding (char followed by int causes 3 bytes padding)
struct Entity {
    char type;       // 1 byte + 3 padding
    uint32_t id;     // 4 bytes
    float health;    // 4 bytes
};
// sizeof = 12, packed_size = 9

// Optimal layout (no padding)
struct Player {
    uint32_t id;
    float health;
    float x, y, z;
};
// sizeof = 20, packed_size = 20

// Nested struct
struct GameState {
    uint32_t tick;
    Player player;
};

// ============================================================================
// Main Demo
// ============================================================================

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║     SeRTial - Zero-Allocation Binary Serialization Demo      ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";

    // ========================================================================
    // Part 1: Type Analysis (Compile-Time)
    // ========================================================================
    
    std::cout << "\n═══ Part 1: Compile-Time Type Analysis ═══\n";
    
    print_type_info<Vec3>("Vec3 (3 floats)");
    print_type_info<Entity>("Entity (char + uint32 + float - has padding!)");
    print_type_info<Player>("Player (uint32 + 4 floats - optimal layout)");
    print_type_info<GameState>("GameState (uint32 + Player)");
    
    // Compile-time static assertions
    static_assert(Message<Vec3>::packed_size == 12, "Vec3 should be 12 bytes packed");
    static_assert(!Message<Vec3>::has_padding, "Vec3 should have no padding");
    
    static_assert(Message<Entity>::packed_size == 9, "Entity should be 9 bytes packed");
    static_assert(Message<Entity>::has_padding, "Entity should have padding");
    
    static_assert(Message<Player>::packed_size == 20, "Player should be 20 bytes packed");
    static_assert(!Message<Player>::has_padding, "Player should have no padding");

    // ========================================================================
    // Part 2: Serialization (Zero Allocation)
    // ========================================================================
    
    std::cout << "\n═══ Part 2: Zero-Allocation Serialization ═══\n";
    
    // Create test data
    Player player{42, 100.0f, 1.5f, 2.5f, 3.5f};
    
    // Serialize to stack-allocated buffer
    auto result = Message<Player>::serialize(player);
    
    std::cout << "\nPlayer serialization:\n";
    std::cout << "  Original: id=" << player.id << ", health=" << player.health 
              << ", pos=(" << player.x << ", " << player.y << ", " << player.z << ")\n";
    std::cout << "  Serialized bytes: " << result.size << "\n";
    std::cout << "  Buffer type: stack-allocated std::array<" << Message<Player>::max_buffer_size << ">\n";
    std::cout << "  Data: ";
    debug::print_hex(result.view());

    // ========================================================================
    // Part 3: Deserialization
    // ========================================================================
    
    std::cout << "\n═══ Part 3: Deserialization ═══\n";
    
    auto restored = Message<Player>::deserialize(result.view());
    
    if (restored) {
        std::cout << "\nRestored Player:\n";
        std::cout << "  id=" << restored->id << ", health=" << restored->health 
                  << ", pos=(" << restored->x << ", " << restored->y << ", " << restored->z << ")\n";
        
        // Verify round-trip
        bool match = (player.id == restored->id && 
                      player.health == restored->health &&
                      player.x == restored->x && 
                      player.y == restored->y && 
                      player.z == restored->z);
        std::cout << "  Round-trip: " << (match ? "[OK] PASS" : "[X] FAIL") << "\n";
    } else {
        std::cout << "  Deserialization failed: " << restored.error().what << "\n";
    }

    // ========================================================================
    // Part 4: Convenience API
    // ========================================================================
    
    std::cout << "\n═══ Part 4: Convenience API ═══\n";
    
    Vec3 position{10.0f, 20.0f, 30.0f};
    
    // Type-deduced serialize
    auto pos_result = serialize(position);
    std::cout << "\nVec3 serialization (type-deduced):\n";
    std::cout << "  Bytes: " << pos_result.size() << "\n";
    std::cout << "  Data: ";
    debug::print_hex(pos_result.view());
    
    // Deserialize
    auto pos_restored = deserialize<Vec3>(pos_result.view());
    if (pos_restored) {
        std::cout << "  Restored: (" << pos_restored->x << ", " 
                  << pos_restored->y << ", " << pos_restored->z << ")\n";
    }

    // ========================================================================
    // Part 5: Padding-Aware Serialization
    // ========================================================================
    
    std::cout << "\n═══ Part 5: Padding-Aware Serialization ═══\n";
    
    Entity entity{'P', 1001, 75.5f};
    
    std::cout << "\nEntity (has padding):\n";
    std::cout << "  sizeof(Entity):     " << sizeof(Entity) << " bytes\n";
    std::cout << "  Message packed_size:" << Message<Entity>::packed_size << " bytes\n";
    std::cout << "  Padding saved:      " << (sizeof(Entity) - Message<Entity>::packed_size) << " bytes\n";
    
    auto entity_result = serialize(entity);
    std::cout << "  Serialized bytes:   " << entity_result.size() << "\n";
    std::cout << "  Data: ";
    debug::print_hex(entity_result.view());
    
    auto entity_restored = deserialize<Entity>(entity_result.view());
    if (entity_restored) {
        std::cout << "  Restored: type='" << entity_restored->type 
                  << "', id=" << entity_restored->id 
                  << ", health=" << entity_restored->health << "\n";
    }

    // ========================================================================
    // Part 6: Nested Structs
    // ========================================================================
    
    std::cout << "\n═══ Part 6: Nested Structs ═══\n";
    
    GameState state{12345, {42, 100.0f, 1.0f, 2.0f, 3.0f}};
    
    auto state_result = serialize(state);
    std::cout << "\nGameState (nested Player):\n";
    std::cout << "  Serialized bytes: " << state_result.size() << "\n";
    std::cout << "  Data: ";
    debug::print_hex(state_result.view());
    
    auto state_restored = deserialize<GameState>(state_result.view());
    if (state_restored) {
        std::cout << "  Restored: tick=" << state_restored->tick 
                  << ", player.id=" << state_restored->player.id << "\n";
    }

    // ========================================================================
    // Summary
    // ========================================================================
    
    std::cout << "\n═══ Summary ═══\n";
    std::cout << "\nSeRTial Features Demonstrated:\n";
    std::cout << "  [OK] Compile-time type analysis (padding, sizes, field count)\n";
    std::cout << "  [OK] Zero-allocation serialization (stack buffers)\n";
    std::cout << "  [OK] Automatic padding removal in serialized form\n";
    std::cout << "  [OK] Type-deduced convenience API\n";
    std::cout << "  [OK] Nested struct support\n";
    std::cout << "  [OK] Full round-trip fidelity\n";
    
    std::cout << "\nVersion: " << sertial::VERSION_STRING << "\n";

    return 0;
}
