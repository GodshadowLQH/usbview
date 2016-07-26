#ifndef _TYPE_H_
#define _TYPE_H_

/*
   The operating system, must be one of: (OS_x)

     WIN32    - Win32 (Windows 2000/XP/Vista/7 and Windows Server 2003/2008)
     WINCE    - WinCE (Windows CE 5.0)
     LINUX    - Linux
*/

#if !defined(SAG_COM) && (defined(WIN64) || defined(_WIN64) || defined(__WIN64__))
#  define OS_WIN32
#  define OS_WIN64
#elif !defined(SAG_COM) && (defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__))
#  if defined(WINCE) || defined(_WIN32_WCE)
#    define OS_WINCE
#  else
#    define OS_WIN32
#  endif
#elif defined(__MWERKS__) && defined(__INTEL__)
#  define OS_WIN32
#elif defined(__linux__) || defined(__linux)
#  define OS_LINUX
#  if defined(ANDROID) || defined(__ANDROID__)
#     define OS_ANDROID
#  endif
#else
#  error "Aclas Library has not been ported to this OS"
#endif

#if defined(OS_WIN32) || defined(OS_WIN64) || defined(OS_WINCE)
#  define OS_WIN
#endif


#ifdef __cplusplus
#	define BEGIN_EXTERN_C extern "C"{
#	define END_EXTERN_C	 }
#else  /* __cplusplus */
#	define BEGIN_EXTERN_C
#	define END_EXTERN_C
#endif /* __cplusplus */


#ifdef __cplusplus
    #ifndef NAMESPACE /* don't use namespace */
    #	define USE_NAMESPACE
    #	define BEGIN_NAMESPACE
    # 	define END_NAMESPACE
    #else	/* use namespace */
    #	define USE_NAMESPACE		using namespace NAMESPACE;
    #	define BEGIN_NAMESPACE	namespace NAMESPACE{
    # 	define END_NAMESPACE		}
    #endif
#else
    #	define USE_NAMESPACE
    #	define BEGIN_NAMESPACE
    # 	define END_NAMESPACE
#endif

#ifndef DECL_EXPORT
#	if defined(OS_WIN)
#		define DECL_EXPORT __declspec(dllexport)
#       define CALL     WINAPI
#	else
#		define DECL_EXPORT export
#       define CALL
#	endif
#endif

#ifndef DECL_IMPORT
#	if defined(OS_WIN)
#		define DECL_IMPORT __declspec(dllimport)
#	else
#		define DECL_IMPORT
#	endif
#endif


#if defined(OS_WIN) || defined(OS_SYMBIAN)
#	define EXPORT DECL_EXPORT
#else
#	define EXPORT extern
#endif


/* for size_t sszie_t uint8_t int8_t uint16_t int16_t ...*/
#include <stddef.h>
#include <sys/types.h>
#if defined OS_WIN
#   include "windows.h"
#endif

#ifdef _MSC_VER
/* on MS environments, the inline keyword is available in C++ only */
#   if !defined(__cplusplus)
#       define inline __inline
#   endif
/* ssize_t is also not available (copy/paste from MinGW) */
#   ifndef _SSIZE_T_DEFINED
#       define _SSIZE_T_DEFINED
#       undef ssize_t
#       ifdef _WIN64
            typedef __int64 ssize_t;
#       else
            typedef int ssize_t;
#       endif /* _WIN64 */
#   endif /* _SSIZE_T_DEFINED */
#endif /* _MSC_VER */

/* stdint.h is not available on older MSVC */
#if defined(_MSC_VER) && (_MSC_VER < 1600) && (!defined(_STDINT)) && (!defined(_STDINT_H))
    typedef __int8	int8_t;
    typedef __int16	int16_t;
    typedef __int32	int32_t;
    typedef unsigned __int8   uint8_t;
    typedef unsigned __int16  uint16_t;
    typedef unsigned __int32  uint32_t;
#else
#   include <stdint.h>
#endif

//#ifndef byte
//#define byte char
//#endif

#ifndef NULL
#define NULL ((void*)0)
#endif


typedef int MYBOOL;
#ifndef BOOL
#define BOOL MYBOOL
#endif

#ifndef TRUE
#   define TRUE 1
#endif

#ifndef FALSE
#   define FALSE 0
#endif

#endif /* TYPE_H_ */
