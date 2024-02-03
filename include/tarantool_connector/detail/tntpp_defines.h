//
// Created by root on 2/3/24.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_DEFINES_H
#define TARANTOOL_CONNECTOR_TNTPP_DEFINES_H

#if defined(_MSC_VER) && !defined(__clang__)  // MSVC
#  define TNTPP_UNREACHABLE __assume(false)
#else  // GCC, Clang
#  define TNTPP_UNREACHABLE __builtin_unreachable()
#endif



#endif  // TARANTOOL_CONNECTOR_TNTPP_DEFINES_H
