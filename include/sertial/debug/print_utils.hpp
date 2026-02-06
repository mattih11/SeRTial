#pragma once

/// Debug/Print Utilities for SeRTial
/// 
/// This header provides utility functions for debugging and analysis.
/// 
/// NAMING CONVENTION:
/// - Functions with "print_" prefix produce terminal output (stdout)
/// - Functions with "to_" prefix return strings without I/O
/// - Functions with "dump_" prefix produce detailed multi-line output
///
/// Example:
///   print_hex(data);              // Prints: "01 02 03 ..."
///   print_type_info<MyStruct>();  // Prints detailed type analysis
///   auto s = to_hex_string(data); // Returns hex string, no output

#include <iostream>
#include <iomanip>
#include <span>
#include <cstddef>
#include <sstream>
#include <string>
#include <chrono>

#include "../message.hpp"

namespace sertial::debug {

// ============================================================================
// Hex/Bytes Printing (produces terminal output)
// ============================================================================

/// Print bytes as hex to stdout
/// @param data The bytes to print
/// @param max_bytes Maximum number of bytes to show (truncates with "...")
inline void print_hex(std::span<const std::byte> data, size_t max_bytes = 32) {
    for (size_t i = 0; i < std::min(data.size(), max_bytes); ++i) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) 
                  << static_cast<int>(data[i]) << " ";
    }
    if (data.size() > max_bytes) std::cout << "...";
    std::cout << std::dec << "\n";
}

/// Print bytes with size prefix: "  [N bytes]: xx xx xx ..." (indented)
/// @param data The bytes to print
/// @param max_bytes Maximum number of bytes to show
inline void print_bytes(std::span<const std::byte> data, size_t max_bytes = 32) {
    std::cout << "  [" << data.size() << " bytes]: ";
    for (size_t i = 0; i < std::min(data.size(), max_bytes); ++i) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) 
                  << static_cast<int>(data[i]) << " ";
    }
    if (data.size() > max_bytes) std::cout << "...";
    std::cout << std::dec << "\n";
}

/// Print bytes with custom prefix
/// @param prefix Text to print before the hex dump
/// @param data The bytes to print
/// @param max_bytes Maximum number of bytes to show
inline void print_bytes_prefixed(const char* prefix, std::span<const std::byte> data, size_t max_bytes = 32) {
    std::cout << prefix << "[" << data.size() << " bytes]: ";
    for (size_t i = 0; i < std::min(data.size(), max_bytes); ++i) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) 
                  << static_cast<int>(data[i]) << " ";
    }
    if (data.size() > max_bytes) std::cout << "...";
    std::cout << std::dec << "\n";
}

// ============================================================================
// String Conversion (no terminal output)
// ============================================================================

/// Convert bytes to hex string (no output)
/// @return String like "01 02 03 ..."
[[nodiscard]] inline std::string to_hex_string(std::span<const std::byte> data, size_t max_bytes = 64) {
    std::ostringstream oss;
    for (size_t i = 0; i < std::min(data.size(), max_bytes); ++i) {
        oss << std::hex << std::setfill('0') << std::setw(2) 
            << static_cast<int>(data[i]);
        if (i < data.size() - 1 && i < max_bytes - 1) oss << " ";
    }
    if (data.size() > max_bytes) oss << " ...";
    return oss.str();
}

// ============================================================================
// Type Analysis (produces terminal output)
// ============================================================================

/// Print detailed type analysis for a BoundedSerializable type
/// Shows sizeof, packed_size, padding info, field count, etc.
template<typename T>
    requires BoundedSerializable<T>
void print_type_info(const char* name) {
    std::cout << "\n" << name << ":\n";
    std::cout << "  sizeof:           " << sizeof(T) << " bytes\n";
    std::cout << "  base_packed_size: " << Message<T>::base_packed_size << " bytes\n";
    std::cout << "  max_packed_size:  " << Message<T>::max_packed_size << " bytes\n";
    std::cout << "  has_variable_fields: " << (Message<T>::has_variable_fields ? "yes" : "no") << "\n";
    std::cout << "  field_count:      " << Message<T>::field_count << "\n";
    std::cout << "  max_buffer_size:  " << Message<T>::max_buffer_size << " bytes\n";
}

/// Print compact type info on one line
template<typename T>
    requires BoundedSerializable<T>
void print_type_summary(const char* name) {
    std::cout << name << ": "
              << "sizeof=" << sizeof(T) 
              << ", packed=" << Message<T>::packed_size
              << ", padding=" << (Message<T>::has_padding ? "yes" : "no")
              << "\n";
}

// ============================================================================
// Benchmark Utilities (produces terminal output)
// ============================================================================

/// Result of a benchmark run
struct BenchmarkResult {
    size_t iterations;
    double total_ns;
    double ns_per_op;
    double ops_per_sec;
};

/// Run a benchmark and return results (no output)
/// @param iterations Number of iterations
/// @param fn Function to benchmark (called iterations times)
/// @return BenchmarkResult with timing data
template<typename Fn>
[[nodiscard]] BenchmarkResult benchmark(size_t iterations, Fn&& fn) {
    // Warmup
    for (size_t i = 0; i < std::min(iterations / 10, size_t{1000}); ++i) {
        fn();
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        fn();
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    double total_ns = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()
    );
    double ns_per_op = total_ns / iterations;
    double ops_per_sec = 1e9 / ns_per_op;
    
    return {iterations, total_ns, ns_per_op, ops_per_sec};
}

/// Run a benchmark and print results
/// @param name Name of the benchmark
/// @param iterations Number of iterations  
/// @param fn Function to benchmark
template<typename Fn>
void print_benchmark(const char* name, size_t iterations, Fn&& fn) {
    auto result = benchmark(iterations, std::forward<Fn>(fn));
    
    std::cout << name << ":\n";
    std::cout << "  Iterations: " << result.iterations << "\n";
    std::cout << "  Time/op:    " << std::fixed << std::setprecision(1) 
              << result.ns_per_op << " ns\n";
    std::cout << "  Throughput: " << std::setprecision(2) 
              << result.ops_per_sec << " ops/sec\n";
}

} // namespace sertial::debug

// ============================================================================
// Convenience using declarations for common use
// ============================================================================

namespace sertial {
    using debug::print_hex;
    using debug::print_bytes;
    using debug::print_type_info;
    using debug::to_hex_string;
}
