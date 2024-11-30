#pragma once

#include <iostream>
#include <string_view>
#include <source_location>

// comptime (constexpr) flag for enabling or disabling safety checks
#ifndef SAFE_TYPE_DISABLE_CHECKS
#define SAFE_TYPE_CHECKS_ENABLED 1
#else
#define SAFE_TYPE_CHECKS_ENABLED 0
#endif


// ANSI colors
#define COLOR_RED "\033[31m"
#define COLOR_CYAN "\033[36m"
#define COLOR_RESET "\033[0m"


// Functions to log errors with detailed info

static void log_error(const std::string_view message) {
    std::cerr << COLOR_RED "Error: " << message << COLOR_RESET "\n";
}

static void log_error(const std::string_view message, const std::source_location location) {
    std::cerr << COLOR_RED "Error: " << message << COLOR_RESET "\n"
              << "  at " COLOR_CYAN << location.file_name()
              << ":" << location.line()
              << " in function `" << location.function_name() << "`" COLOR_RESET "\n";
}

static void log_error(const std::string_view message, 
                      const std::source_location first_location,
                      const std::source_location second_location) {
    std::cerr << COLOR_RED "Error: " << message << COLOR_RESET "\n"
              << "  First location: " COLOR_CYAN << first_location.file_name()
              << ":" << first_location.line()
              << " in function `" << first_location.function_name() << "`" COLOR_RESET "\n"
              << "  Second location: " COLOR_CYAN << second_location.file_name()
              << ":" << second_location.line()
              << " in function `" << second_location.function_name() << "`" COLOR_RESET "\n";
}

#undef COLOR_RED
#undef COLOR_CYAN
#undef COLOR_RESET