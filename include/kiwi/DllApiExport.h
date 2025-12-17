/**
 * @file DllApiExport.h
 * @brief DLL import/export macros for Windows and visibility control for other platforms
 *
 * This header provides KIWI_API macro that handles:
 * - __declspec(dllexport) when building the DLL on Windows (KIWI_EXPORTS defined)
 * - __declspec(dllimport) when consuming the DLL on Windows
 * - __attribute__((visibility("default"))) on GCC/Clang when building
 * - Empty definition otherwise
 */

#ifndef KIWI_DLL_MACRO_H
#define KIWI_DLL_MACRO_H

#ifndef KIWI_API
#  if defined(_WIN32) || defined(__CYGWIN__)
#    ifdef KIWI_EXPORTS
#      define KIWI_API __declspec(dllexport)
#    else
#      define KIWI_API __declspec(dllimport)
#    endif
#  elif defined(__GNUC__)
#    ifdef KIWI_EXPORTS
#      define KIWI_API __attribute__((visibility("default")))
#    else
#      define KIWI_API
#    endif
#  else
#    define KIWI_API
#  endif
#endif

#endif // KIWI_DLL_MACRO_H
