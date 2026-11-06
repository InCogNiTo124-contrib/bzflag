/* bzflag
 * Copyright (c) 1993-2025 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * common definitions
 */

#ifndef BZF_COMMON_H
#define BZF_COMMON_H

/* this should always be the very FIRST header */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif // HAVE_CONFIG_H

#ifdef _WIN32
#  undef NOMINMAX
#  define NOMINMAX 1
#  include "win32.h"
#endif

#include <stdio.h>
#include <stdlib.h> /* needed for bzfrand */
#include <math.h>
#include <limits.h>
#include <inttypes.h>
#include <stdint.h>
#ifdef __cplusplus
#  include <cmath>
#  include <cstdint>
#  include <cinttypes>
#  include <climits>
#  include <algorithm>
#  include <iterator>
#endif


extern int debugLevel;


/* Provide a means to conveniently test the version of the GNU
 * compiler.  Use it like this:
 *
 * #if GCC_PREREQ(2,8)
 * ... code requiring gcc 2.8 or later ...
 * #endif
 *
 * WARNING: THIS MACRO IS CONSIDERED PRIVATE AND SHOULD NOT BE USED
 * OUTSIDE OF THIS HEADER FILE.  DO NOT RELY ON IT.
 */
#ifndef GCC_PREREQ
#  if defined __GNUC__
#    define GCC_PREREQ(major, minor) __GNUC__ > (major) || (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor))
#  else
#    define GCC_PREREQ(major, minor) 0
#  endif
#else
#  warning "GCC_PREREQ is already defined.  See the common.h header."
#endif


/**
 * UNUSED provides a common mechanism for declaring unused parameters.
 * Use it like this:
 *
 * int
 * my_function(int argc, char **UNUSED(argv))
 * {
 *   ...
 * }
 *
 */
#ifndef UNUSED
#  if GCC_PREREQ(2, 5)
/* GCC-style */
#    define UNUSED(parameter) (parameter) __attribute__((unused))
#  else
/* MSVC/C++ */
#    ifdef __cplusplus
#      if defined(NDEBUG)
#   define UNUSED(parameter) /* parameter */
#      else /* some of them are asserted */
#    define UNUSED(parameter) (parameter)
#      endif
#    else
#      define UNUSED(parameter) (parameter)
#    endif
#  endif
#else
#  undef UNUSED
#  define UNUSED(parameter) (parameter)
#  warning "UNUSED was previously defined.  Parameter declaration behavior is unknown, see common.h"
#endif


/* near zero by some epsilon convenience define since relying on
 * the floating point unit for proper equivalence is not safe
 */
#define NEAR_ZERO(_value,_epsilon)  ( ((_value) > -_epsilon) && ((_value) < _epsilon) )

/* (radians <--> degrees) conversion values */
#define DEG2RAD 0.0174532925199432957692369076848861271344287189
#define RAD2DEG 57.29577951308232087679815481410517033240547247
#define DEG2RADf ((float)DEG2RAD)
#define RAD2DEGf ((float)RAD2DEG)


/* seven places of precision is pretty safe, so something less precise */
#ifdef FLT_EPSILON
#  define ZERO_TOLERANCE FLT_EPSILON
#else
#  define ZERO_TOLERANCE 1.0e-06f
#endif

/* Might we be BSDish? sys/param.h has BSD defined if so */
#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

#ifdef HAVE__STRICMP
#  define strcasecmp _stricmp
#endif
#ifdef HAVE__STRNICMP
#  define strncasecmp _strnicmp
#endif
#ifndef HAVE_VSNPRINTF
#  ifdef HAVE__VSNPRINTF
#    define vsnprintf _vsnprintf
#  else
#    define vsnprintf(buf, size, fmt, list) vsprintf(buf, fmt, list)
#  endif
#endif

/* random number stuff */
#define bzfrand()   ((double)rand() / ((double)RAND_MAX + 1.0))
#define bzfsrand(_s)    srand(_s)

#include <regex.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif


/* missing constants */

#ifndef MAXFLOAT
#  define   MAXFLOAT    3.402823466e+38f
#endif

#ifndef M_PI
#  define   M_PI        3.14159265358979323846f
#endif

#ifndef M_SQRT1_2
#  define   M_SQRT1_2   0.70710678118654752440f
#endif

#ifdef __cplusplus
#  define bzcountof(__x)   std::size(__x)
#else
#  define bzcountof(__x)   (sizeof(__x) / sizeof(__x[0]))
#endif

#ifdef isnan
#  undef isnan
#endif
#define isnan std::isnan

// vectors implemented as arrays
typedef float afvec2[2];
typedef float afvec3[3];

#endif /* BZF_COMMON_H */


/* Local Variables: ***
 * mode: C++ ***
 * tab-width: 8 ***
 * c-basic-offset: 2 ***
 * indent-tabs-mode: t ***
 * End: ***
 * ex: shiftwidth=2 tabstop=8
 */
