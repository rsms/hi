#ifndef _HI_COMMON_H_
#define _HI_COMMON_H_

#define _HI_INDIRECT_INCLUDE_

// The classic stringifier macro
#define HI_STR1(str) #str
#define HI_STR(str) HI_STR1(str)

// Version
#define HI_VERSION_MAJOR 0
#define HI_VERSION_MINOR 1
#define HI_VERSION_BUILD 0
#define HI_VERSION_STRING \
  HI_STR(HI_VERSION_MAJOR) "." HI_STR(HI_VERSION_MINOR) "." HI_STR(HI_VERSION_BUILD)

// Configuration
#ifndef HI_DEBUG
  #define HI_DEBUG 0
#elif HI_DEBUG && defined(NDEBUG)
  #undef NDEBUG
#endif

// Disable (symmetical) multiprocessing?
//#define HI_WITHOUT_SMP 1

// Defines the host target
#include <hi/common-target.h>

// HI_FILENAME expands to the current translation unit's filename
#ifdef __BASE_FILE__
  #define HI_FILENAME __BASE_FILE__
#else
  #define HI_FILENAME ((strrchr(__FILE__, '/') ?: __FILE__ - 1) + 1)
#endif

// Number of items in a constant array
#define HI_COUNTOF(a) (sizeof(a) / sizeof(*(a)))

// Terminate process with status 70 (EX_SOFTWARE), writing `fmt` with optional
// arguments to stderr.
#define HI_FATAL(fmt, ...) \
  errx(70, "FATAL: " fmt " at " __FILE__ ":" HI_STR(__LINE__), ##__VA_ARGS__)

// Print `fmt` with optional arguments to stderr, and abort() process causing
// EXC_CRASH/SIGABRT.
#define HI_CRASH(fmt, ...) do { \
    fprintf(stderr, "CRASH: " fmt " at " __FILE__ ":" HI_STR(__LINE__) "\n", \
      ##__VA_ARGS__); \
    abort(); \
  } while (0)

// Attributes
#ifndef __has_attribute
  #define __has_attribute(x) 0
#endif
#ifndef __has_builtin
  #define __has_builtin(x) 0
#endif

#if __has_attribute(always_inline)
  #define HI_ALWAYS_INLINE __attribute__((always_inline))
#else
  #define HI_ALWAYS_INLINE
#endif
#if __has_attribute(noinline)
  #define HI_NO_INLINE __attribute__((noinline))
#else
  #define HI_NO_INLINE
#endif
#if __has_attribute(unused)
  // Attached to a function, means that the function is meant to be possibly
  // unused. The compiler will not produce a warning for this function.
  #define HI_UNUSED __attribute__((unused))
#else
  #define HI_UNUSED
#endif
#if __has_attribute(warn_unused_result)
  #define HI_WUNUSEDR __attribute__((warn_unused_result))
#else
  #define HI_WUNUSEDR
#endif
#if __has_attribute(packed)
  #define HI_PACKED __attribute((packed))
#else
  #define HI_PACKED
#endif
#if __has_attribute(aligned)
  #define HI_ALIGNED(bytes) __attribute__((aligned (bytes)))
#else
  #warning "No align attribute available. Things might break"
  #define HI_ALIGNED
#endif

#if __has_builtin(__builtin_unreachable)
  #define HI_UNREACHABLE do { \
    assert(!"Declared UNREACHABLE but was reached"); __builtin_unreachable(); } while(0);
#else
  #define HI_UNREACHABLE assert(!"Declared UNREACHABLE but was reached");
#endif

#if __has_attribute(noreturn)
  #define HI_NORETURN __attribute__((noreturn))
#else
  #define HI_NORETURN 
#endif

#define HI_NOT_IMPLEMENTED \
  HI_FATAL("NOT IMPLEMENTED in %s", __PRETTY_FUNCTION__)

#define HI_MAX(a,b) \
  ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define HI_MIN(a,b) \
  ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#include <hi/common-types.h> // .. include <std{io,int,def,bool}>

// libc
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <err.h>
#include <math.h>
#if HI_HOST_OS_POSIX
  #include <unistd.h>
#endif

// libc++
#ifdef __cplusplus
  #include <string>
  #include <map>
  #include <vector>
  #include <list>
  #include <forward_list>
  #include <exception>
  #include <stdexcept>
  #include <utility>
  #include <iostream>
  #include <memory>
  #include <functional>
#endif // __cplusplus

// Atomic operations
#include <hi/common-atomic.h>

#ifdef __cplusplus
  #include <hi/common-cxxdetail.h>
  #include <hi/common-ref.h>
#endif

#undef _HI_INDIRECT_INCLUDE_

#endif // _HI_COMMON_H_
