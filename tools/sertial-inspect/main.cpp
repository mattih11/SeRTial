/// sertial-inspect - CLI tool for inspecting SeRTial type schemas
///
/// Usage:
///   sertial-inspect <schema.json> --summary
///   sertial-inspect <schema.json> --message "TypeName"
///   sertial-inspect <schema.json> --all
///   sertial-inspect <schema.json> --no-color

#include "schema_types.hpp"
#include "cli_renderer.hpp"
#include <rfl.hpp>
#include <rfl/json.hpp>
#include <iostream>
#include <string>
#include <vector>

using namespace sertial::tools;

// Type alias for StructLayout reflector output
using TypeSchema = typename rfl::Reflector<sertial::StructLayout<int>>::ReflType;

/// Find schema by name (supports partial matching)
std::optional<TypeSchema> find_schema(const std::vector<TypeSchema>& schemas, 
                                      const std::string& name) {
    // Exact match first
    for (const auto& schema : schemas) {
        if (schema.name == name) {
            return schema;
        }
    }
    
    // Partial match (case-insensitive)
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    
    for (const auto& schema : schemas) {
        std::string lower_schema_name = schema.name;
        std::transform(lower_schema_name.begin(), lower_schema_name.end(), 
                       lower_schema_name.begin(), ::tolower);
        
        if (lower_schema_name.find(lower_name) != std::string::npos) {
            return schema;
        }
    }
    
    return std::nullopt;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: sertial-inspect <schema.json> [options]\n\n";
        std::cout << "Options:\n";
        std::cout << "  --summary             Show summary table of all types\n";
        std::cout << "  --message <name>      Show detailed view of specific type\n";
        std::cout << "  --all                 Show detailed view of all types\n";
        std::cout << "  --no-color            Disable colored output\n";
        return 1;
    }
    
    std::string filepath = argv[1];
    bool show_summary = false;
    bool show_all = false;
    bool use_color = true;
    std::string message_name;
    
    // Parse command-line arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--summary") {
            show_summary = true;
        } else if (arg == "--all") {
            show_all = true;
        } else if (arg == "--no-color") {
            use_color = false;
        } else if (arg == "--message" && i + 1 < argc) {
            message_name = argv[++i];
        }
    }
    
    // Default to summary if no option specified
    if (!show_summary && !show_all && message_name.empty()) {
        show_summary = true;
    }
    
    // Load schemas using rfl::json::load
    auto collection_result = rfl::json::load<SchemaOutput>(filepath);
    if (!collection_result) {
        std::cerr << "Error: Failed to load schema file: " 
                  << collection_result.error().what() << "\n";
        return 1;
    }
    
    auto collection = collection_result.value();
    
    // Parse each message JSON string into TypeSchema
    std::vector<TypeSchema> schemas;
    for (const auto& msg_json : collection.messages) {
        auto schema_result = rfl::json::read<TypeSchema>(msg_json);
        if (schema_result) {
            schemas.push_back(schema_result.value());
        } else {
            std::cerr << "Warning: Failed to parse type schema: " 
                      << schema_result.error().what() << "\n";
        }
    }
    
    if (schemas.empty()) {
        std::cerr << "Error: No valid type schemas found\n";
        return 1;
    }
    
    CLIRenderer renderer(use_color);
    
    // Execute requested action
    if (show_summary) {
        renderer.print_summary(schemas);
    }
    
    if (!message_name.empty()) {
        auto schema = find_schema(schemas, message_name);
        if (schema) {
            renderer.print_detailed(*schema);
        } else {
            std::cerr << "Error: Type not found: " << message_name << "\n";
            return 1;
        }
    }
    
    if (show_all) {
        for (const auto& schema : schemas) {
            renderer.print_detailed(schema);
        }
    }
    
    return 0;
}
