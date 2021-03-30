#pragma once

#include <iostream>

#ifndef NDEBUG
    #define LOG_DEBUG(str) std::cout << logger::color_modifier(logger::FG_BLUE) << "[DEBUG] " << (str) << logger::color_modifier(logger::FG_DEFAULT) << std::endl
#else
    #define LOG_DEBUG(str)
#endif

#define LOG_ERROR(str) std::cout << logger::color_modifier(logger::FG_RED) << "[ERROR] " << (str) << logger::color_modifier(logger::FG_DEFAULT) << std::endl
#define LOG_WARN(str) std::cout << logger::color_modifier(logger::FG_YELLOW) << "[WARNING] " << (str) << logger::color_modifier(logger::FG_DEFAULT) << std::endl
#define LOG_INFO(str) std::cout << "[INFO] " << (str) << std::endl


namespace logger
{
    enum color_code {
        FG_RED      = 31,
        FG_GREEN    = 32,
        FG_YELLOW   = 33,
        FG_BLUE     = 34,
        FG_DEFAULT  = 39,
        BG_RED      = 41,
        BG_GREEN    = 42,
        BG_YELLOW   = 43,
        BG_BLUE     = 44,
        BG_DEFAULT  = 49
    };

    class color_modifier {
        color_code code;
    public:
        color_modifier(color_code pCode) : code(pCode) {}
        friend std::ostream&
        operator<<(std::ostream& os, const color_modifier& mod) {
            return os << "\033[" << mod.code << "m";
        }
    };
}

