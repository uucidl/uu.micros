#pragma once

/**
 * @file
 * some rare compiler specific macros
 */

#if defined(__clang__)
#  define BEGIN_NOWARN_BLOCK \
        _Pragma("clang diagnostic push") \
        _Pragma("clang diagnostic ignored \"-Wmissing-field-initializers\"") \
        _Pragma("clang diagnostic ignored \"-Wmissing-braces\"") \
        _Pragma("clang diagnostic ignored \"-Wunused-function\"")

#  define END_NOWARN_BLOCK \
        _Pragma("clang diagnostic pop")

#  define BEGIN_NAMELESS_STRUCT_DEF_BLOCK
#  define END_NAMELESS_STRUCT_DEF_BLOCK

#elif defined(_MSC_VER)
#  define BEGIN_NOWARN_BLOCK \
        __pragma(warning(push, 3)) \
        __pragma(warning(disable : 4244))

#  define END_NOWARN_BLOCK \
        __pragma(warning(pop))

#  define BEGIN_NAMELESS_STRUCT_DEF_BLOCK \
        __pragma(warning(push)) \
        __pragma(warning(disable: 4201))

#  define END_NAMELESS_STRUCT_DEF_BLOCK \
        __pragma(warning(pop))

#else
#  warning "missing definitions for BEGIN/END NOWARN BLOCKs"
#endif
