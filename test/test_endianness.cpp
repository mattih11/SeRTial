#include "test_framework.hpp"
#include <sertial/sertial.hpp>
#include <sertial/core/endian.hpp>

using namespace sertial;
using namespace sertial::test;

// ============================================================================
// Test Types
// ============================================================================

struct SimpleMessage {
    uint16_t id;
    uint32_t value;
    float x, y, z;
};

struct NestedMessage {
    uint32_t header;
    SimpleMessage data;
    uint64_t timestamp;
};

// ============================================================================
// Test Suite
// ============================================================================

struct EndianTests : TestSuite<EndianTests> {
    static constexpr const char* name = "Endianness Conversion Tests";
    
    static bool run() {
        // Byte swap primitives
        TEST_ASSERT(bswap16(0x1234) == 0x3412, "bswap16");
        TEST_ASSERT(bswap16(0xABCD) == 0xCDAB, "bswap16");
        TEST_ASSERT(bswap32(0x12345678) == 0x78563412, "bswap32");
        TEST_ASSERT(bswap32(0xABCDEF01) == 0x01EFCDAB, "bswap32");
        TEST_ASSERT(bswap64(0x0123456789ABCDEFULL) == 0xEFCDAB8967452301ULL, "bswap64");
        
        // Endianness detection
        TEST_ASSERT((is_little_endian() || is_big_endian()), "Endianness detection");
        TEST_ASSERT((native_endian() == std::endian::little || 
                     native_endian() == std::endian::big), "Native endian");
        
        std::cout << "    System endianness: " 
                  << (is_little_endian() ? "little" : "big") << "\n";
        
        // Simple message swap
        {
            SimpleMessage msg{0x1234, 0x56789ABC, 1.0f, 2.0f, 3.0f};
            auto buffer = serialize(msg);
            auto original = buffer;
            
            // Get mutable span from buffer
            std::span<std::byte> buf_span{buffer.data(), buffer.size()};
            swap_endianness<SimpleMessage>(buf_span);
            TEST_ASSERT(!std::equal(buffer.view().begin(), buffer.view().end(),
                                   original.view().begin(), original.view().end()),
                       "Swap changes data");
            
            swap_endianness<SimpleMessage>(buf_span);
            TEST_ASSERT(std::equal(buffer.view().begin(), buffer.view().end(),
                                   original.view().begin(), original.view().end()),
                       "Double swap restores original");
        }
        
        // Nested message swap
        {
            NestedMessage msg{
                0x11223344,
                SimpleMessage{0x1234, 0x56789ABC, 1.0f, 2.0f, 3.0f},
                0xAABBCCDDEEFF0011ULL
            };
            
            auto buffer = serialize(msg);
            auto original = buffer;
            
            std::span<std::byte> buf_span{buffer.data(), buffer.size()};
            swap_endianness<NestedMessage>(buf_span);
            TEST_ASSERT(!std::equal(buffer.view().begin(), buffer.view().end(),
                                   original.view().begin(), original.view().end()),
                       "Nested swap changes data");
            
            swap_endianness<NestedMessage>(buf_span);
            TEST_ASSERT(std::equal(buffer.view().begin(), buffer.view().end(),
                                   original.view().begin(), original.view().end()),
                       "Nested double swap restores original");
        }
        
        // Deserialize with endian conversion
        {
            SimpleMessage original{0x1234, 0x56789ABC, 1.0f, 2.0f, 3.0f};
            auto buffer = serialize(original);
            
            // Simulate receiving from different endianness
            auto swapped_buffer = buffer;
            std::span<std::byte> swap_span{swapped_buffer.data(), swapped_buffer.size()};
            swap_endianness<SimpleMessage>(swap_span);
            
            // Determine opposite endianness
            std::endian opposite = (std::endian::native == std::endian::little) 
                ? std::endian::big 
                : std::endian::little;
            
            // Deserialize with endian conversion
            auto result = Message<SimpleMessage>::deserialize(swapped_buffer.view(), opposite);
            
            TEST_ASSERT(result, "Deserialize with conversion succeeds");
            TEST_ASSERT_EQ(result->id, original.id, "ID matches");
            TEST_ASSERT_EQ(result->value, original.value, "Value matches");
            TEST_ASSERT_EQ(result->x, original.x, "X matches");
            TEST_ASSERT_EQ(result->y, original.y, "Y matches");
            TEST_ASSERT_EQ(result->z, original.z, "Z matches");
        }
        
        // Deserialize same endian (no conversion)
        {
            SimpleMessage original{0x1234, 0x56789ABC, 1.0f, 2.0f, 3.0f};
            auto buffer = serialize(original);
            
            auto result = Message<SimpleMessage>::deserialize(buffer.view(), std::endian::native);
            
            TEST_ASSERT(result, "Deserialize same endian succeeds");
            TEST_ASSERT_EQ(result->id, original.id, "ID matches");
            TEST_ASSERT_EQ(result->value, original.value, "Value matches");
        }
        
        // Conditional swap
        {
            SimpleMessage msg{0x1234, 0x56789ABC, 1.0f, 2.0f, 3.0f};
            auto buffer = serialize(msg);
            auto original = buffer;
            
            std::span<std::byte> buf_span{buffer.data(), buffer.size()};
            
            // Same endian - should not swap
            swap_endianness_from<SimpleMessage>(buf_span, std::endian::native);
            TEST_ASSERT(std::equal(buffer.view().begin(), buffer.view().end(),
                                   original.view().begin(), original.view().end()),
                       "Same endian: no swap");
            
            // Different endian - should swap
            std::endian opposite = (std::endian::native == std::endian::little) 
                ? std::endian::big 
                : std::endian::little;
            
            swap_endianness_from<SimpleMessage>(buf_span, opposite);
            TEST_ASSERT(!std::equal(buffer.view().begin(), buffer.view().end(),
                                   original.view().begin(), original.view().end()),
                       "Different endian: swaps");
        }
        
        return true;
    }
};

// ============================================================================
// Main
// ============================================================================

int main() {
    return TestRunner::run<EndianTests>();
}
