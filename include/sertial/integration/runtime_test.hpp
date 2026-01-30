#pragma once

/// RuntimeTest - One-liner test execution for MessageCollection types
///
/// Usage:
///   using MyMessages = MessageCollection<Position<>, PointCloud<>>;
///   RuntimeTest<MyMessages>::run_all();
///
/// Individual tests:
///   RuntimeTest<MyMessages>::run_roundtrip();
///   RuntimeTest<MyMessages>::run_streaming();

#include <rfl.hpp>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstring>
#include <vector>
#include <functional>

#include "message_collection.hpp"
#include "../core/traits/memory_map.hpp"
#include "../io/optimized_binary.hpp"

namespace sertial {

/// RuntimeTest - Test serialization round-trips for MessageCollection types
template<typename Collection>
struct RuntimeTest {
    static_assert(!Collection::empty, "MessageCollection cannot be empty");
    
    /// Test result summary
    struct Results {
        std::size_t total = 0;
        std::size_t passed = 0;
        std::size_t failed = 0;
        
        bool all_passed() const { return failed == 0; }
    };
    
    /// Run all tests (roundtrip + streaming)
    static Results run_all(bool verbose = true) {
        Results results;
        
        if (verbose) {
            std::cout << "=====================================\n";
            std::cout << "RuntimeTest: " << Collection::count << " message types\n";
            std::cout << "=====================================\n";
        }
        
        run_roundtrip_impl<0>(results, verbose);
        run_streaming_impl<0>(results, verbose);
        
        if (verbose) {
            std::cout << "\n=====================================\n";
            std::cout << "Results: " << results.passed << "/" << results.total 
                      << " tests passed";
            if (results.failed > 0) {
                std::cout << " (" << results.failed << " failed)";
            }
            std::cout << "\n=====================================\n";
        }
        
        return results;
    }
    
    /// Run only roundtrip tests (serialize -> deserialize -> compare)
    static Results run_roundtrip(bool verbose = true) {
        Results results;
        
        if (verbose) {
            std::cout << "=== Roundtrip Tests ===\n";
        }
        
        run_roundtrip_impl<0>(results, verbose);
        return results;
    }
    
    /// Run only streaming tests (multi-message write/read)
    static Results run_streaming(bool verbose = true) {
        Results results;
        
        if (verbose) {
            std::cout << "=== Streaming Tests ===\n";
        }
        
        run_streaming_impl<0>(results, verbose);
        return results;
    }

private:
    /// Print hex bytes for debugging
    static void print_hex(std::span<const std::byte> data, std::size_t max = 32) {
        for (std::size_t i = 0; i < std::min(data.size(), max); ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(data[i]) << " ";
        }
        if (data.size() > max) std::cout << "...";
        std::cout << std::dec << "\n";
    }
    
    /// Roundtrip test for a single type
    template<typename T>
    static bool test_roundtrip_single(bool verbose) {
        using MM = MemoryMap<T>;
        std::string name = rfl::type_name_t<T>().str();
        
        // Truncate long template names
        if (name.length() > 40) {
            name = name.substr(0, 37) + "...";
        }
        
        T original{};  // Default initialized
        
        if (verbose) {
            std::cout << "\n  [ROUNDTRIP] " << name << "\n";
            std::cout << "    sizeof=" << MM::unpacked_size 
                      << " packed=" << MM::packed_size
                      << " regions=" << MM::memcpy_region_count << "\n";
        }
        
        // Serialize (returns static_buffer)
        auto serialized = serialize(original);
        if (serialized.size() != MM::packed_size) {
            if (verbose) std::cout << "    FAIL: Wrong serialized size\n";
            return false;
        }
        
        // Deserialize
        auto restored = deserialize<T>(serialized);
        if (!restored.has_value()) {
            if (verbose) std::cout << "    FAIL: Deserialization failed\n";
            return false;
        }
        
        // Re-serialize and compare (ignores padding differences)
        auto reserialized = serialize(*restored);
        bool match = (serialized.size() == reserialized.size()) &&
                     std::memcmp(serialized.data(), reserialized.data(), serialized.size()) == 0;
        
        if (verbose) {
            std::cout << "    " << (match ? "PASS" : "FAIL") << "\n";
            if (!match) {
                std::cout << "    Original:     ";
                print_hex(serialized.view());
                std::cout << "    Reserialized: ";
                print_hex(reserialized.view());
            }
        }
        
        return match;
    }
    
    /// Streaming test for a single type
    template<typename T>
    static bool test_streaming_single(bool verbose) {
        using MM = MemoryMap<T>;
        std::string name = rfl::type_name_t<T>().str();
        
        if (name.length() > 40) {
            name = name.substr(0, 37) + "...";
        }
        
        constexpr std::size_t count = 5;
        std::array<T, count> original{};  // Default initialized, stack allocated
        
        if (verbose) {
            std::cout << "\n  [STREAMING] " << name << " (x" << count << ")\n";
        }
        
        // Write using static streaming writer
        StaticWriter<count * MM::packed_size> writer;
        for (const auto& item : original) {
            if (!writer.write(item)) {
                if (verbose) std::cout << "    FAIL: Write failed\n";
                return false;
            }
        }
        
        if (writer.size() != count * MM::packed_size) {
            if (verbose) std::cout << "    FAIL: Wrong total size\n";
            return false;
        }
        
        // Read back
        StaticReader reader(writer.view());
        std::array<T, count> restored{};
        
        for (std::size_t i = 0; i < count; ++i) {
            auto item = reader.read<T>();
            if (!item.has_value()) {
                if (verbose) std::cout << "    FAIL: Read failed\n";
                return false;
            }
            restored[i] = *item;
        }
        
        // Compare all via serialization
        bool all_match = true;
        for (std::size_t i = 0; i < count; ++i) {
            auto orig_serial = serialize(original[i]);
            auto rest_serial = serialize(restored[i]);
            if (orig_serial.size() != rest_serial.size() ||
                std::memcmp(orig_serial.data(), rest_serial.data(), orig_serial.size()) != 0) {
                all_match = false;
                break;
            }
        }
        
        if (verbose) {
            std::cout << "    " << (all_match ? "PASS" : "FAIL") << "\n";
        }
        
        return all_match;
    }
    
    /// Recursive roundtrip test implementation
    template<std::size_t I>
    static void run_roundtrip_impl(Results& results, bool verbose) {
        if constexpr (I < Collection::count) {
            using T = typename Collection::template type_at<I>;
            results.total++;
            if (test_roundtrip_single<T>(verbose)) {
                results.passed++;
            } else {
                results.failed++;
            }
            run_roundtrip_impl<I + 1>(results, verbose);
        }
    }
    
    /// Recursive streaming test implementation
    template<std::size_t I>
    static void run_streaming_impl(Results& results, bool verbose) {
        if constexpr (I < Collection::count) {
            using T = typename Collection::template type_at<I>;
            results.total++;
            if (test_streaming_single<T>(verbose)) {
                results.passed++;
            } else {
                results.failed++;
            }
            run_streaming_impl<I + 1>(results, verbose);
        }
    }
};

} // namespace sertial
