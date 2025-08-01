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