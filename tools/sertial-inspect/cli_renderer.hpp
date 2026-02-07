#pragma once

#include "ansi_colors.hpp"
#include <sertial/core/layout/struct_layout.hpp>
#include <sertial/core/layout/struct_layout_reflector.hpp>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>

namespace sertial::tools {

// Use the actual StructLayout reflector type
using TypeSchema = typename rfl::Reflector<sertial::StructLayout<int>>::ReflType;

/// CLI renderer for type schemas (replaces Python CLI)
class CLIRenderer {
public:
    explicit CLIRenderer(bool use_color = true) : color_(use_color) {}
    
    /// Print summary table of all types
    void print_summary(const std::vector<TypeSchema>& schemas) const {
        std::cout << color_.bold("SeRTial Type Schemas") << "\n";
        std::cout << std::string(80, '=') << "\n\n";
        
        // Header
        std::cout << std::left
                  << color_.cyan(pad_right("Type", 40))
                  << color_.cyan(pad_right("Size", 12))
                  << color_.cyan(pad_right("Packed", 12))
                  << color_.cyan(pad_right("Max Pack", 12))
                  << color_.cyan(pad_right("Fields", 8))
                  << "\n";
        std::cout << std::string(80, '-') << "\n";
        
        // Data rows
        for (const auto& schema : schemas) {
            std::cout << std::left
                      << std::setw(40) << truncate(schema.name, 38)
                      << std::setw(12) << schema.sizeof_bytes
                      << std::setw(12) << schema.base_packed_size;
            
            // Highlight variable types
            if (schema.has_variable_fields) {
                std::cout << std::setw(12) << color_.yellow(std::to_string(schema.max_packed_size));
            } else {
                std::cout << std::setw(12) << schema.max_packed_size;
            }
            
            std::cout << std::setw(8) << schema.field_count << "\n";
        }
        
        std::cout << std::string(80, '-') << "\n";
        std::cout << "Total: " << color_.green(std::to_string(schemas.size())) << " types\n";
    }
    
    /// Print detailed view of a single type
    void print_detailed(const TypeSchema& schema) const {
        std::cout << "\n" << color_.bold(schema.name) << "\n";
        std::cout << std::string(80, '=') << "\n\n";
        
        // Size information
        std::cout << color_.cyan("Memory Layout:") << "\n";
        std::cout << "  Struct size:      " << schema.sizeof_bytes << " bytes\n";
        std::cout << "  Base packed:      " << schema.base_packed_size << " bytes\n";
        std::cout << "  Max packed:       ";
        if (schema.has_variable_fields) {
            std::cout << color_.yellow(std::to_string(schema.max_packed_size)) << " bytes (variable)\n";
        } else {
            std::cout << schema.max_packed_size << " bytes\n";
        }
        std::cout << "\n";
        
        // Block information
        std::cout << color_.cyan("Serialization Blocks:") << "\n";
        std::cout << "  Fixed blocks:     " << schema.fixed_block_count << "\n";
        std::cout << "  Padding blocks:   " << schema.padding_block_count << "\n";
        std::cout << "  Dynamic blocks:   " << schema.dynamic_block_count << "\n";
        std::cout << "  Runtime blocks:   " << schema.runtime_offset_block_count << "\n";
        std::cout << "  Total blocks:     " << schema.total_blocks << "\n";
        std::cout << "\n";
        
        // Field table
        if (!schema.field_names.empty()) {
            print_field_table(schema);
        }
        
        // Memory visualizations
        print_memory_bar_struct(schema);
        print_memory_bar_packed(schema);
    }
    
private:
    ColorString color_;
    
    void print_field_table(const TypeSchema& schema) const {
        std::cout << color_.cyan("Fields:") << "\n";
        std::cout << "  " << std::left
                  << color_.cyan(pad_right("Name", 20))
                  << color_.cyan(pad_right("Type", 30))
                  << color_.cyan(pad_right("Size", 8))
                  << color_.cyan(pad_right("Offset", 8))
                  << color_.cyan(pad_right("Align", 8))
                  << color_.cyan(pad_right("Var", 8))
                  << "\n";
        std::cout << "  " << std::string(78, '-') << "\n";
        
        for (std::size_t i = 0; i < schema.field_names.size(); ++i) {
            // Color the field marker
            auto field_color = AnsiColors::get_field_color(i);
            std::string field_marker = color_(field_color, " ░ ");
            
            std::cout << "  " << field_marker << std::left
                      << std::setw(17) << truncate(schema.field_names[i], 15)
                      << std::setw(30) << truncate(schema.field_types[i], 28)
                      << std::setw(8) << schema.field_sizes[i]
                      << std::setw(8) << schema.field_offsets[i]
                      << std::setw(8) << schema.field_alignments[i];
            
            if (schema.field_is_variable[i]) {
                std::cout << color_.yellow("yes");
                if (schema.capacities[i] > 0) {
                    std::cout << " [" << schema.capacities[i] << "]";
                }
            } else {
                std::cout << "no";
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }
    
    struct MemoryRegion {
        std::size_t offset;
        std::size_t size;
        int field_index; // -1 for padding, >= 0 for field
        bool is_variable;
        bool is_length_prefix; // true if this is a length prefix for the field
    };
    
    void print_memory_bar_struct(const TypeSchema& schema) const {
        std::cout << color_.cyan("Memory Layout (struct):") << "\n";
        
        if (schema.sizeof_bytes == 0) return;
        
        // Calculate regions with padding
        std::vector<MemoryRegion> regions;
        std::size_t current_offset = 0;
        
        for (std::size_t i = 0; i < schema.field_names.size(); ++i) {
            std::size_t field_offset = schema.field_offsets[i];
            std::size_t field_size = schema.field_sizes[i];
            
            // Add padding before field if any
            if (field_offset > current_offset) {
                regions.push_back({current_offset, field_offset - current_offset, -1, false, false});
            }
            
            // Add field
            regions.push_back({field_offset, field_size, static_cast<int>(i), schema.field_is_variable[i], false});
            current_offset = field_offset + field_size;
        }
        
        // Add trailing padding if any
        if (current_offset < schema.sizeof_bytes) {
            regions.push_back({current_offset, schema.sizeof_bytes - current_offset, -1, false, false});
        }
        
        render_memory_layout(regions, schema.sizeof_bytes);
    }
    
    void print_memory_bar_packed(const TypeSchema& schema) const {
        std::cout << color_.cyan("Serialized Layout (packed):") << "\n";
        
        if (schema.base_packed_size == 0) return;
        
        // Use max size for scaling consistency with struct layout
        std::size_t max_size = schema.has_variable_fields ? schema.max_packed_size : schema.base_packed_size;
        
        // Build packed regions (no padding, with length prefixes for variable fields)
        std::vector<MemoryRegion> regions;
        std::size_t current_offset = 0;
        
        for (std::size_t i = 0; i < schema.field_names.size(); ++i) {
            if (schema.field_is_variable[i] && schema.capacities[i] > 0) {
                // Length prefix (4 bytes) - associate with field for coloring
                regions.push_back({current_offset, 4, static_cast<int>(i), false, true}); // is_length_prefix = true
                current_offset += 4;
                
                // Variable data (max capacity)
                std::size_t max_data_size = schema.capacities[i] * schema.element_sizes[i];
                regions.push_back({current_offset, max_data_size, static_cast<int>(i), true, false});
                current_offset += max_data_size;
            } else {
                // Fixed size field
                regions.push_back({current_offset, schema.field_sizes[i], static_cast<int>(i), false, false});
                current_offset += schema.field_sizes[i];
            }
        }
        
        // Pad to same total size as struct for consistent visualization
        std::size_t saved_bytes = schema.sizeof_bytes > max_size ? schema.sizeof_bytes - max_size : 0;
        
        render_memory_layout(regions, schema.sizeof_bytes);
        
        if (saved_bytes > 0) {
            std::cout << "  " << color_.green("(" + std::to_string(saved_bytes) + " bytes saved by removing padding)") << "\n";
        }
        
        if (schema.has_variable_fields) {
            std::cout << "  " << schema.base_packed_size << " bytes (base) → " 
                      << schema.max_packed_size << " bytes (max)\n";
        } else {
            std::cout << "  " << schema.base_packed_size << " bytes\n";
        }
        
        print_legend();
    }
    
    void print_legend() const {
        std::cout << "\n" << color_.cyan("Legend:") << "\n";
        std::cout << "  " << color_(AnsiColors::get_field_color(0), " ") << " Regular field\n";
        std::cout << "  " << color_(AnsiColors::get_field_color(1), "░") << " Variable field\n";
        std::cout << "  " << color_(AnsiColors::PADDING, "░") << " Padding (not serialized)\n";
        std::cout << "  " << color_(AnsiColors::HEADER, "├─┤") << " Length prefix (4 bytes)\n";
        std::cout << "\n";
    }
    
    void render_memory_layout(const std::vector<MemoryRegion>& regions, std::size_t total_bytes) const {
        // Get terminal width (default 120 if can't detect)
        std::size_t terminal_width = 120; // Could use ioctl to detect
        
        // Calculate extra space needed for margins (byte numbers + spacing + markers)
        // extra_space = (num_digits) + 2 (spacing) + 1 (marker) = num_digits + 3
        // times 2 for left and right sides
        std::size_t num_digits = total_bytes > 0 ? static_cast<std::size_t>(std::log10(total_bytes)) + 1 : 1;
        std::size_t extra_space = (num_digits + 3) * 2;
        
        std::size_t chars_per_line = terminal_width - extra_space; // Leave room for margins and axis
        
        // Calculate number of lines needed (integer arithmetic)
        // line_number = 1 + intdivision((bytenum-1) / chars_per_line)
        std::size_t num_lines = 1 + (total_bytes > 0 ? (total_bytes - 1) / chars_per_line : 0);
        
        // Calculate chars per byte (integer division)
        std::size_t bytes_per_line = (total_bytes + num_lines - 1) / num_lines;  // Ceiling division
        std::size_t chars_per_byte = chars_per_line / bytes_per_line;
        if (chars_per_byte < 1) chars_per_byte = 1; // Minimum 1 char per byte
        
        // Calculate indent for alignment
        std::size_t indent = num_digits + 2; // num_digits + 2 spaces
        
        // Render top axis
        render_axis_top(total_bytes, chars_per_byte, bytes_per_line, num_lines, indent);
        
        // Render each line of memory
        for (std::size_t line = 0; line < num_lines; ++line) {
            std::size_t start_byte = line * bytes_per_line;
            std::size_t end_byte = std::min(total_bytes, (line + 1) * bytes_per_line);
            
            // Left axis
            if (num_lines > 1) {
                std::cout << std::setw(num_digits) << std::right << start_byte << "  "; // Proper width
            } else {
                std::cout << std::string(indent, ' '); // Align with axis
            }
            
            // Border
            std::cout << "├";
            
            // Memory content
            std::size_t line_bytes = end_byte - start_byte;
            std::size_t line_chars = line_bytes * chars_per_byte;
            render_memory_line(regions, start_byte, end_byte, chars_per_byte, line_chars);
            
            // Border
            std::cout << "┤";
            
            // Right axis
            if (num_lines > 1) {
                std::cout << " " << std::setw(num_digits) << std::left << (end_byte - 1); // Proper width
            }
            
            std::cout << "\n";
        }
        
        // Render bottom axis
        render_axis_bottom(total_bytes, chars_per_byte, bytes_per_line, num_lines, indent);
        
        std::cout << "  " << total_bytes << " bytes\n\n";
    }
    
    void render_memory_line(const std::vector<MemoryRegion>& regions, std::size_t start_byte, 
                           std::size_t end_byte, std::size_t chars_per_byte, std::size_t target_chars) const {
        std::size_t current_char = 0;
        
        for (const auto& region : regions) {
            std::size_t region_end = region.offset + region.size;
            
            // Skip regions entirely before this line
            if (region_end <= start_byte) continue;
            
            // Stop if region starts after this line
            if (region.offset >= end_byte) break;
            
            // Calculate overlap
            std::size_t overlap_start = std::max(region.offset, start_byte);
            std::size_t overlap_end = std::min(region_end, end_byte);
            std::size_t overlap_size = overlap_end - overlap_start;
            
            std::size_t chars_for_region = overlap_size * chars_per_byte;
            if (chars_for_region == 0) chars_for_region = 1;
            
            // Render based on region type
            if (region.is_length_prefix) {
                // Length prefix - use field color
                auto field_color = AnsiColors::get_field_color(region.field_index);
                render_length_prefix(chars_for_region, field_color);
            } else if (region.field_index == -1) {
                // Padding - gray shading
                for (std::size_t j = 0; j < chars_for_region; ++j) {
                    std::cout << color_(AnsiColors::PADDING, "░");
                }
            } else {
                // Regular or variable field
                auto field_color = AnsiColors::get_field_color(region.field_index);
                if (region.is_variable) {
                    // Variable field: shade character with field color background
                    for (std::size_t j = 0; j < chars_for_region; ++j) {
                        std::cout << color_(field_color, "░");
                    }
                } else {
                    // Regular field: space with colored background
                    std::cout << color_(field_color, std::string(chars_for_region, ' '));
                }
            }
            
            current_char += chars_for_region;
        }
        
        // Fill any remaining space (shouldn't happen, but safety)
        while (current_char < target_chars) {
            std::cout << " ";
            current_char++;
        }
    }
    
    void render_length_prefix(std::size_t chars, std::string_view field_color) const {
        // ║ for 1, ├─┤ for 2+
        std::string prefix_str;
        if (chars == 1) {
            prefix_str = "║"; // Single char length field
        } else if (chars == 2) {
            prefix_str = "├┤";
        } else {
            prefix_str = "├";
            for (std::size_t i = 0; i < chars - 2; ++i) {
                prefix_str += "─";
            }
            prefix_str += "┤";
        }
        std::cout << color_(field_color, prefix_str); // Use field color background
    }
    
    void render_axis_top(std::size_t total_bytes, std::size_t chars_per_byte, std::size_t bytes_per_line, std::size_t num_lines, std::size_t indent) const {
        std::size_t display_bytes = num_lines <= 1 ? total_bytes : bytes_per_line;
        std::size_t display_chars = display_bytes * chars_per_byte;
        
        // Numbers line (indent + 1 extra space for alignment with byte 0)
        std::cout << std::string(indent + 1, ' ');
        render_number_labels(0, display_bytes, chars_per_byte, display_chars);
        std::cout << "\n";
        
        // Axis line with ┬ connectors (pointing down into memory)
        std::cout << std::string(indent, ' ') << "┌"; // Proper indent + corner
        render_axis_line(0, display_bytes, chars_per_byte, display_chars, true); // true = top axis (┬)
        std::cout << "┐\n"; // Corner
    }
    
    void render_axis_bottom(std::size_t total_bytes, std::size_t chars_per_byte, 
                           std::size_t bytes_per_line, std::size_t num_lines, std::size_t indent) const {
        if (num_lines > 1) {
            // Multi-line: show last line range
            std::size_t last_line_start = (num_lines - 1) * bytes_per_line;
            std::size_t last_line_bytes = total_bytes - last_line_start;
            std::size_t last_line_chars = last_line_bytes * chars_per_byte;
            
            // Axis line with ┴ connectors (pointing up from memory)
            std::cout << std::string(indent, ' ') << "└"; // Proper indent + corner
            render_axis_line(last_line_start, total_bytes, chars_per_byte, last_line_chars, false); // false = bottom axis (┴)
            std::cout << "┘\n"; // Corner
            
            // Numbers line (indent + 1 extra space for alignment)
            std::cout << std::string(indent + 1, ' ');
            render_number_labels(last_line_start, total_bytes, chars_per_byte, last_line_chars);
            std::cout << "\n";
        } else {
            // Single line: just close the box (no bottom axis needed, already shown on top)
        }
    }
    void render_number_labels(std::size_t start_byte, std::size_t end_byte, 
                             std::size_t chars_per_byte, std::size_t total_chars) const {
        // Determine tick spacing
        std::size_t byte_range = end_byte - start_byte;
        std::size_t tick_spacing = 1;
        if (byte_range > 100) tick_spacing = 20;
        else if (byte_range > 50) tick_spacing = 10;
        else if (byte_range > 20) tick_spacing = 5;
        else if (byte_range > 10) tick_spacing = 4;
        
        // Build label line
        std::vector<std::string> label_chars(total_chars, " ");
        
        // Find first tick position >= start_byte that aligns with tick_spacing
        std::size_t first_tick = ((start_byte + tick_spacing - 1) / tick_spacing) * tick_spacing;
        
        // Place labels at tick positions (using integer arithmetic)
        for (std::size_t byte_pos = first_tick; byte_pos < end_byte; byte_pos += tick_spacing) {
            std::size_t char_pos = (byte_pos - start_byte) * chars_per_byte;
            if (char_pos >= total_chars) break;
            
            std::string label = std::to_string(byte_pos);
            for (std::size_t i = 0; i < label.length() && (char_pos + i) < total_chars; ++i) {
                label_chars[char_pos + i] = std::string(1, label[i]);
            }
        }
        
        // Output labels
        for (const auto& ch : label_chars) {
            std::cout << ch;
        }
    }
    
    void render_axis_line(std::size_t start_byte, std::size_t end_byte, 
                         std::size_t chars_per_byte, std::size_t total_chars, bool is_top) const {
        // Determine tick spacing
        std::size_t byte_range = end_byte - start_byte;
        std::size_t tick_spacing = 1;
        if (byte_range > 100) tick_spacing = 20;
        else if (byte_range > 50) tick_spacing = 10;
        else if (byte_range > 20) tick_spacing = 5;
        else if (byte_range > 10) tick_spacing = 4;
        
        // Build axis line with appropriate connectors
        std::string major_tick = "┼";
        std::string minor_tick = is_top ? "┬" : "┴";
        
        std::vector<std::string> axis_chars;
        
        // Multiple chars per byte - show tick at each byte boundary
        for (std::size_t byte_pos = start_byte; byte_pos < end_byte; ++byte_pos) {
            // First char of byte: tick mark
            if (byte_pos % tick_spacing == 0) {
                axis_chars.push_back(major_tick); // Major tick at labeled positions
            } else {
                axis_chars.push_back(minor_tick); // Minor tick at unlabeled bytes
            }
            
            // Remaining chars for this byte: connectors
            for (std::size_t i = 1; i < chars_per_byte; ++i) {
                axis_chars.push_back("─");
            }
        }
        
        // Output axis
        for (const auto& ch : axis_chars) {
            std::cout << ch;
        }
    }
    
    static std::string truncate(const std::string& str, std::size_t max_len) {
        if (str.length() <= max_len) return str;
        return str.substr(0, max_len - 2) + "..";
    }
    
    static std::string pad_right(const std::string& str, std::size_t width) {
        if (str.length() >= width) return str;
        return str + std::string(width - str.length(), ' ');
    }
};

} // namespace sertial::tools
