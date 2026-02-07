#pragma once

#include <string>
#include <string_view>

namespace sertial::tools {

/// ANSI color codes for terminal output
struct AnsiColors {
    static constexpr std::string_view RESET = "\033[0m";
    static constexpr std::string_view BOLD = "\033[1m";
    static constexpr std::string_view DIM = "\033[2m";
    
    // Field colors (cycle through for different fields)
    static constexpr std::string_view FIELD_BLUE = "\033[48;5;31m";
    static constexpr std::string_view FIELD_GREEN = "\033[48;5;35m";
    static constexpr std::string_view FIELD_YELLOW = "\033[48;5;136m";
    static constexpr std::string_view FIELD_MAGENTA = "\033[48;5;132m";
    static constexpr std::string_view FIELD_CYAN = "\033[48;5;37m";
    static constexpr std::string_view FIELD_RED = "\033[48;5;167m";
    static constexpr std::string_view FIELD_PURPLE = "\033[48;5;98m";
    static constexpr std::string_view FIELD_TEAL = "\033[48;5;29m";
    
    static constexpr std::string_view PADDING = "\033[48;5;236m";  // Dark gray
    static constexpr std::string_view HEADER = "\033[48;5;240m";   // Medium gray
    
    // Text colors
    static constexpr std::string_view WHITE = "\033[97m";
    static constexpr std::string_view BLACK = "\033[30m";
    static constexpr std::string_view GREEN = "\033[92m";
    static constexpr std::string_view RED = "\033[91m";
    static constexpr std::string_view YELLOW = "\033[93m";
    static constexpr std::string_view CYAN = "\033[96m";
    
    // Field color array for cycling
    static constexpr std::string_view FIELD_COLORS[] = {
        FIELD_BLUE, FIELD_GREEN, FIELD_YELLOW, FIELD_MAGENTA,
        FIELD_CYAN, FIELD_RED, FIELD_PURPLE, FIELD_TEAL
    };
    
    static constexpr std::size_t NUM_FIELD_COLORS = 8;
    
    /// Get field color by index (cycles through colors)
    static std::string_view get_field_color(std::size_t index) {
        return FIELD_COLORS[index % NUM_FIELD_COLORS];
    }
};

/// Helper to conditionally apply colors
class ColorString {
public:
    ColorString(bool use_color = true) : use_color_(use_color) {}
    
    std::string operator()(std::string_view color_code, std::string_view text) const {
        if (!use_color_) return std::string(text);
        return std::string(color_code) + std::string(text) + std::string(AnsiColors::RESET);
    }
    
    std::string bold(std::string_view text) const {
        return (*this)(AnsiColors::BOLD, text);
    }
    
    std::string green(std::string_view text) const {
        return (*this)(AnsiColors::GREEN, text);
    }
    
    std::string yellow(std::string_view text) const {
        return (*this)(AnsiColors::YELLOW, text);
    }
    
    std::string cyan(std::string_view text) const {
        return (*this)(AnsiColors::CYAN, text);
    }
    
    std::string red(std::string_view text) const {
        return (*this)(AnsiColors::RED, text);
    }
    
private:
    bool use_color_;
};

} // namespace sertial::tools
