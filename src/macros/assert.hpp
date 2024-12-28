#pragma once

#include <iostream>
#include <source_location>
#include <string>

// what you are seing down below is a comptime (codegen) system that allows enabling/disabling assert's for debug/release builds
// it also provide ability to see where the function was called FROM when it crashed

#define ENABLE_SOURCE_LOCATION_ASSERT

#ifdef DISABLE_CHECKS
#define ASSERT(...)                                                                                                \
    do {                                                                                                           \
    } while (false)
#else
#ifdef ENABLE_THROW // default is just abort
#ifdef ENABLE_SOURCE_LOCATION_ASSERT
#define ASSERT(condition, msg)                                                                                \
    do {                                                                                                           \
        if (!(condition)) {                                                                                        \
            throw std::runtime_error("Assertion failed: " #condition ", " msg ", at " << ___loc.file_name() << ":" << ___loc.line() << ":" << ___loc.column()); \
        }                                                                                                          \
    } while (false)
#else
#define ASSERT(condition, msg)                                                                                \
    do {                                                                                                           \
        if (!(condition)) {                                                                                        \
            throw std::runtime_error("Assertion failed: " #condition ", " msg ", in " __FILE__ ", line " + std::to_string(__LINE__)); \
        }                                                                                                          \
    } while (false)
#endif
#else
#ifdef ENABLE_SOURCE_LOCATION_ASSERT
#define ASSERT(condition, msg)                                                                                \
    do {                                                                                                           \
        if (!(condition)) {                                                                                        \
            std::cerr << "Assertion failed: " #condition ", " msg ", at " << ___loc.file_name() << ":" << ___loc.line() << ":" << ___loc.column() << std::endl; \
            std::abort();                                                                                          \
        }                                                                                                          \
    } while (false)
#else
#define ASSERT(condition, msg)                                                                                \
    do {                                                                                                           \
        if (!(condition)) {                                                                                        \
            std::cerr << "Assertion failed: " #condition ", " msg ", in " << __FILE__ << ", line " << __LINE__ << std::endl; \
            std::abort();                                                                                          \
        }                                                                                                          \
    } while (false)
#endif
#endif
#endif

#ifdef ENABLE_SOURCE_LOCATION_ASSERT
    #define SLOC_DECLARE_DEFARG , std::source_location ___loc = std::source_location::current()
    #define SLOC_DECLARE_DEFARG_NOCOMMA std::source_location ___loc = std::source_location::current()
#else
    #define SLOC_DECLARE_DEFARG         /* no source_location arg */
    #define SLOC_DECLARE_DEFARG_NOCOMMA /* no source_location arg */
#endif