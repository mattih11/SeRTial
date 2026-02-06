#pragma once

/// SchemaGenerator - Direct schema generation from StructLayout
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
#include "schema_export.hpp"

namespace sertial {

/// Schema collection output format
struct SchemaOutput {
    std::string version = "5.0.0";
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
        output.version = "5.0.0";
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
        std::cout << "SeRTial Schema Generator v5.0\n";
        std::cout << "=============================\n";
        std::cout << "Using StructLayout for direct schema export\n\n";
        
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
        std::cout << std::string(70, '-') << "\n";
        std::cout << std::setw(30) << std::left << "Name"
                  << std::setw(10) << "sizeof"
                  << std::setw(10) << "base"
                  << std::setw(10) << "max"
                  << std::setw(5) << "blks"
                  << "flags\n";
        std::cout << std::string(70, '-') << "\n";
        
        std::size_t variable_count = 0;
        
        for (const auto& m : output.messages) {
            if (m.has_variable_fields) variable_count++;
            
            std::string name = m.name;
            if (name.length() > 29) name = name.substr(0, 26) + "...";
            
            std::size_t total_blocks = m.fixed_block_count + 
                                      m.dynamic_block_count + 
                                      m.runtime_offset_block_count;
            
            std::cout << std::setw(30) << std::left << name
                      << std::setw(10) << m.sizeof_bytes
                      << std::setw(10) << m.base_packed_size
                      << std::setw(10) << m.max_packed_size
                      << std::setw(5) << total_blocks;
            
            if (m.has_variable_fields) std::cout << "[VAR]";
            std::cout << "\n";
        }
        
        std::cout << std::string(70, '-') << "\n";
        std::cout << "Variable-size types: " << variable_count
                  << "/" << output.messages.size() << " messages\n";
    }

private:
    /// Recursive schema generation (compile-time iteration)
    template<std::size_t I>
    static void generate_impl(SchemaOutput& output) {
        if constexpr (I < Collection::count) {
            using T = typename Collection::template type_at<I>;
            output.messages.push_back(export_schema<T>());
            generate_impl<I + 1>(output);
        }
    }
};

} // namespace sertial
