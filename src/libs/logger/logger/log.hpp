#pragma once

#include <iostream>

#ifndef NDEBUG
    #define LOG_DEBUG(...) logger::print_variadic(std::cout, logger::color_modifier(logger::FG_BLUE), "[DEBUG] ", __VA_ARGS__, logger::color_modifier(logger::FG_DEFAULT));
#else
    #define LOG_DEBUG(str)
#endif

#define LOG_ERROR(...) logger::print_variadic(std::cout, logger::color_modifier(logger::FG_RED), "[ERROR] ", __VA_ARGS__, logger::color_modifier(logger::FG_DEFAULT));
#define LOG_WARN(...) logger::print_variadic(std::cout, logger::color_modifier(logger::FG_YELLOW), "[WARNING] ", __VA_ARGS__, logger::color_modifier(logger::FG_DEFAULT));
#define LOG_INFO(...) logger::print_variadic(std::cout, "[INFO] ", __VA_ARGS__);


#define LOGGER_COLOR_MODIFIER_FG_RED "\033[31m"
#define LOGGER_COLOR_MODIFIER_FG_GREEN "\033[32m"
#define LOGGER_COLOR_MODIFIER_FG_YELLOW "\033[33m"
#define LOGGER_COLOR_MODIFIER_FG_BLUE "\033[34m"
#define LOGGER_COLOR_MODIFIER_FG_DEFAULT "\033[39m"

#define LOGGER_COLOR_MODIFIER_BG_RED "\033[41m"
#define LOGGER_COLOR_MODIFIER_BG_GREEN "\033[42m"
#define LOGGER_COLOR_MODIFIER_BG_YELLOW "\033[43m"
#define LOGGER_COLOR_MODIFIER_BG_BLUE "\033[44m"
#define LOGGER_COLOR_MODIFIER_BG_DEFAULT "\033[49m"


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

    template <typename OS, typename... Args>
    void print_variadic(OS& os, Args&&... args)
    {
        ((os << args), ...);
        os << std::endl;
    }
}

