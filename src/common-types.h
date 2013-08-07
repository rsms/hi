// Declares integer types like size_t, uin32_t, etc.
#ifndef _HI_COMMON_STDINT_H_
#define _HI_COMMON_STDINT_H_

#ifndef _HI_INDIRECT_INCLUDE_
#error "Please #include <hi/common.h> instead of this file directly."
#endif

#include <stddef.h>
#include <stdio.h>  // size_t, ssize_t, off_t

#if defined(_WIN32) && !defined(__MINGW32__)
  typedef signed char      int8_t;
  typedef unsigned char    uint8_t;
  typedef short            int16_t;
  typedef unsigned short   uint16_t;
  typedef int              int32_t;
  typedef unsigned int     uint32_t;
  typedef __int64          int64_t;
  typedef unsigned __int64 uint64_t;
  // intptr_t and friends are defined in crtdefs.h through stdio.h.
  typedef unsigned char    bool;
#else
  #include <stdint.h>
  #include <stdbool.h>
#endif

#endif // _HI_COMMON_STDINT_H_
