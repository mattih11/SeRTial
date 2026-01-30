#pragma once

/// SchemaGenerator - One-liner schema generation from MessageCollection
///
/// Usage:
///   using MyMessages = MessageCollection<Position<>, PointCloud<>>;
///   SchemaGenerator<MyMessages>::write("schemas.json");
///
/// Or generate to stdout:
///   auto json = SchemaGenerator<MyMessages>::to_json();

#include <rfl.hpp>
#include <rfl/json.hpp>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>

#include "message_collection.hpp"
#include "../core/traits/memory_map.hpp"
#include "../core/traits/hybrid_memory_map.hpp"

namespace sertial {

/// Schema collection output format
struct SchemaOutput {
    std::string version = "4.0.0";
    std::string generated;
    std::vector<TypeSchema> messages;
};

/// SchemaGenerator - Generate JSON schemas for MessageCollection types
template<typename Collection>
struct SchemaGenerator {
    static_assert(!Collection::empty, "MessageCollection cannot be empty");
    
    /// Generate SchemaOutput with all type schemas
    static SchemaOutput generate() {
        SchemaOutput output;
        output.version = "4.0.0";
        output.generated = __DATE__ " " __TIME__;
        
        generate_impl<0>(output);
        return output;
    }
    
    /// Convert to JSON string
    static std::string to_json() {
        auto output = generate();
        return rfl::json::write(output);
    }
    
    /// Write schemas to file
    static bool write(const std::string& filepath) {
        auto json = to_json();
        std::ofstream out(filepath);
        if (!out) return false;
        out << json;
        return true;
    }
    
    /// Write with verbose console output
    static bool write_verbose(const std::string& filepath) {
        std::cout << "SeRTial Schema Generator v4.0\n";
        std::cout << "=============================\n";
        std::cout << "Using HybridMemoryMap for unified serialization\n\n";
        
        auto output = generate();
        std::cout << "Generated " << output.messages.size() << " schemas\n";
        
        auto json = rfl::json::write(output);
        std::ofstream out(filepath);
        if (!out) {
            std::cerr << "Error: Cannot write to " << filepath << "\n";
            return false;
        }
        out << json;
        std::cout << "Written to: " << filepath << "\n\n";
        
        print_summary(output);
        return true;
    }
    
    /// Print summary table
    static void print_summary(const SchemaOutput& output) {
        std::cout << "Summary:\n";
        std::cout << std::string(78, '-') << "\n";
        std::cout << std::setw(24) << std::left << "Name"
                  << std::setw(10) << "Category"
                  << std::setw(7) << "sizeof"
                  << std::setw(7) << "packed"
                  << std::setw(5) << "pad"
                  << std::setw(5) << "blks"
                  << std::setw(5) << "dyn"
                  << "flags\n";
        std::cout << std::string(78, '-') << "\n";
        
        std::size_t total_padding = 0;
        std::size_t total_blocks = 0;
        std::size_t single_memcpy_count = 0;
        std::size_t variable_count = 0;
        
        for (const auto& m : output.messages) {
            total_padding += m.padding_bytes;
            total_blocks += m.fixed_block_count + m.dynamic_block_count + m.runtime_offset_block_count;
            if (m.can_single_memcpy) single_memcpy_count++;
            if (m.has_variable_fields) variable_count++;
            
            std::string name = m.name;
            if (name.length() > 23) name = name.substr(0, 20) + "...";
            
            std::cout << std::setw(24) << std::left << name
                      << std::setw(10) << m.category
                      << std::setw(7) << m.sizeof_bytes
                      << std::setw(7) << (m.has_variable_fields ? m.base_packed_size : m.packed_size)
                      << std::setw(5) << m.padding_bytes
                      << std::setw(5) << (m.fixed_block_count + m.dynamic_block_count + m.runtime_offset_block_count)
                      << std::setw(5) << m.dynamic_block_count;
            
            if (m.has_padding) std::cout << "[PAD]";
            if (m.can_single_memcpy) std::cout << "[1CPY]";
            if (m.has_variable_fields) std::cout << "[VAR]";
            std::cout << "\n";
        }
        
        std::cout << std::string(78, '-') << "\n";
        std::cout << "Total padding eliminated: " << total_padding << " bytes\n";
        std::cout << "Single-memcpy optimizable: " << single_memcpy_count 
                  << "/" << output.messages.size() << " messages\n";
        std::cout << "Variable-size structs: " << variable_count
                  << "/" << output.messages.size() << " messages\n";
        std::cout << "Average blocks per message: " 
                  << std::fixed << std::setprecision(2)
                  << (output.messages.size() > 0 ? 
                      static_cast<double>(total_blocks) / output.messages.size() : 0)
                  << "\n";
    }

private:
    /// Recursive schema generation (compile-time iteration)
    template<std::size_t I>
    static void generate_impl(SchemaOutput& output) {
        if constexpr (I < Collection::count) {
            using T = typename Collection::template type_at<I>;
            output.messages.push_back(get_hybrid_schema<T>());
            generate_impl<I + 1>(output);
        }
    }
};

} // namespace sertial
