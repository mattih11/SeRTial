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
    std::string version = "5.1.0";
    std::string generated;
    std::vector<std::string> messages;
};

/// SchemaGenerator - Generate JSON schemas for MessageCollection types
template<typename Collection>
struct SchemaGenerator {
    static_assert(!Collection::empty, "MessageCollection cannot be empty");
    
    /// Generate SchemaOutput with type schemas (JSON Schema format)
    static SchemaOutput generate_schemas() {
        SchemaOutput output;
        output.version = "5.1.0";
        output.generated = __DATE__ " " __TIME__;
        
        generate_schemas_impl<0>(output);
        return output;
    }
    
    /// Generate SchemaOutput with actual layout data (runtime values)
    static SchemaOutput generate_data() {
        SchemaOutput output;
        output.version = "5.1.0";
        output.generated = __DATE__ " " __TIME__;
        
        generate_data_impl<0>(output);
        return output;
    }
    
    /// Convert schemas to JSON string
    static std::string to_json_schema() {
        auto output = generate_schemas();
        return rfl::json::write(output);
    }
    
    /// Convert data to JSON string
    static std::string to_json_data() {
        auto output = generate_data();
        return rfl::json::write(output);
    }
    
    /// Write schemas to file (type definitions)
    static bool write_schemas(const std::string& filepath) {
        auto json = to_json_schema();
        std::ofstream out(filepath);
        if (!out) return false;
        out << json;
        return true;
    }
    
    /// Write data to file (actual metadata values)
    static bool write_data(const std::string& filepath) {
        auto json = to_json_data();
        std::ofstream out(filepath);
        if (!out) return false;
        out << json;
        return true;
    }
    
    /// Write both schemas and data with verbose console output
    static bool write_verbose(const std::string& data_filepath, const std::string& schema_filepath = "") {
        std::cout << "SeRTial Schema Generator v5.1\n";
        std::cout << "=============================\n";
        std::cout << "Using StructLayout reflector for automatic export\n\n";
        
        // Generate data (metadata values)
        auto data_output = generate_data();
        std::cout << "Generated " << data_output.messages.size() << " type layouts (data)\n";
        
        auto data_json = rfl::json::write(data_output);
        std::ofstream data_out(data_filepath);
        if (!data_out) {
            std::cerr << "Error: Cannot write to " << data_filepath << "\n";
            return false;
        }
        data_out << data_json;
        std::cout << "Written data to: " << data_filepath << "\n";
        
        // Generate schemas (type definitions) if requested
        if (!schema_filepath.empty()) {
            auto schema_output = generate_schemas();
            auto schema_json = rfl::json::write(schema_output);
            std::ofstream schema_out(schema_filepath);
            if (!schema_out) {
                std::cerr << "Error: Cannot write to " << schema_filepath << "\n";
                return false;
            }
            schema_out << schema_json;
            std::cout << "Written schemas to: " << schema_filepath << "\n";
        }
        
        std::cout << "\n";
        print_summary(data_output);
        return true;
    }
    
    /// Print summary table
    static void print_summary(const SchemaOutput& output) {
        std::cout << "Summary:\n";
        std::cout << std::string(70, '-') << "\n";
        std::cout << "Generated " << output.messages.size() << " type layouts\n";
        std::cout << std::string(70, '-') << "\n";
    }

private:
    /// Recursive schema generation (compile-time iteration)
    template<std::size_t I>
    static void generate_schemas_impl(SchemaOutput& output) {
        if constexpr (I < Collection::count) {
            using T = typename Collection::template type_at<I>;
            output.messages.push_back(export_schema<T>());
            generate_schemas_impl<I + 1>(output);
        }
    }
    
    /// Recursive data generation (compile-time iteration)
    template<std::size_t I>
    static void generate_data_impl(SchemaOutput& output) {
        if constexpr (I < Collection::count) {
            using T = typename Collection::template type_at<I>;
            output.messages.push_back(export_layout_data<T>());
            generate_data_impl<I + 1>(output);
        }
    }
};

} // namespace sertial
