#pragma once

/// RuntimeTest - One-liner test execution for MessageCollection types
///
/// Usage:
///   using MyMessages = MessageCollection<Position<>, PointCloud<>>;
///   RuntimeTest<MyMessages>::run_all();
///
/// Individual tests:
///   RuntimeTest<MyMessages>::run_roundtrip();

#include <rfl.hpp>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstring>
#include <vector>
#include <functional>

#include "message_collection.hpp"
#include "../core/traits/memory_map.hpp"
#include "../io/unified_binary.hpp"

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
    
    /// Run all tests (roundtrip)
    static Results run_all(bool verbose = true) {
        Results results;
        
        if (verbose) {
            std::cout << "=====================================\n";
            std::cout << "RuntimeTest: " << Collection::count << " message types\n";
            std::cout << "=====================================\n";
        }
        
        run_roundtrip_impl<0>(results, verbose);
        
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
    
    /// Run roundtrip tests (serialize -> deserialize -> compare)
    static Results run_roundtrip(bool verbose = true) {
        return run_all(verbose);
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
        using HMM = HybridMemoryMap<T>;
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
        
        // Use HybridMemoryMap to calculate expected size (unified serialization uses HMM)
        // NOTE: HMM may produce slightly larger output than MemoryMap for nested structs
        //       due to preserved alignment padding. This is a known limitation.
        std::size_t expected_size;
        if constexpr (HMM::has_variable_fields) {
            expected_size = HMM::calculate_packed_size(original);
        } else {
            expected_size = HMM::base_packed_size;
        }
        
        if (serialized.size() != expected_size) {
            if (verbose) {
                std::cout << "    FAIL: Wrong serialized size (expected " 
                          << expected_size << ", got " << serialized.size() << ")\n";
            }
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
};

} // namespace sertial