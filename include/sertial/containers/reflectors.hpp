#pragma once

#include "fixed_vector.hpp"
#include "fixed_string.hpp"
#include "ring_buffer.hpp"
#include <rfl.hpp>
#include <vector>
#include <string>
#include <span>

// ============================================================================
// rfl::Reflector specializations for SeRTial containers
// ============================================================================
// These allow reflect-cpp to understand our custom container types.
// Containers are reflected as standard types (vector, string) for schema.
// ============================================================================

namespace rfl {

// ----------------------------------------------------------------------------
// fixed_vector<T, N> → reflected as std::vector<T>
// ----------------------------------------------------------------------------
template<typename T, std::size_t N>
struct Reflector<sertial::fixed_vector<T, N>> {
    using ReflType = std::vector<T>;
    
    // Schema generation only needs from() - read-only export
    static ReflType from(const sertial::fixed_vector<T, N>& v) {
        return std::vector<T>(v.begin(), v.end());
    }
    
    // Optional: enable deserialization from JSON
    static sertial::fixed_vector<T, N> to(const ReflType& vec) {
        sertial::fixed_vector<T, N> result;
        for (const auto& elem : vec) {
            result.push_back(elem);
        }
        return result;
    }
};

// ----------------------------------------------------------------------------
// fixed_string<N> → reflected as std::string
// ----------------------------------------------------------------------------
template<std::size_t N>
struct Reflector<sertial::fixed_string<N>> {
    using ReflType = std::string;
    
    static ReflType from(const sertial::fixed_string<N>& s) {
        return std::string(s.c_str());
    }
    
    static sertial::fixed_string<N> to(const ReflType& str) {
        return sertial::fixed_string<N>(str.c_str());
    }
};

// ----------------------------------------------------------------------------
// RingBuffer<T, N> → reflected as std::vector<T>
// ----------------------------------------------------------------------------
template<typename T, std::size_t N>
struct Reflector<sertial::RingBuffer<T, N>> {
    using ReflType = std::vector<T>;
    
    static ReflType from(const sertial::RingBuffer<T, N>& rb) {
        std::vector<T> result;
        result.reserve(rb.size());
        
        // Iterate through ring buffer in logical order
        for (std::size_t i = 0; i < rb.size(); ++i) {
            result.push_back(rb[i]);
        }
        return result;
    }
    
    static sertial::RingBuffer<T, N> to(const ReflType& vec) {
        sertial::RingBuffer<T, N> result;
        for (const auto& elem : vec) {
            result.push_back(elem);
        }
        return result;
    }
};

} // namespace rfl
