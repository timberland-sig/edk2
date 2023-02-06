/*-
 *   stdlib_types.h - Standard data types

 *   Copyright (c) 2020, Dell EMC All rights reserved
 *   SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#ifndef STDLIB_TYPES_H
#define STDLIB_TYPES_H
#include "edk_log.h"

#define kill(pid, sig)   0   // 0 is success

#ifdef __cplusplus
extern "C" {
#endif

//
// File operations are not required for EFI building,
// so FILE is mapped to VOID * to pass build
//
typedef VOID *FILE;

typedef INT8  int8_t;
typedef INT16 int16_t;
typedef INT32 int32_t;
typedef INT64 int64_t;

typedef UINT8  uint8_t;
typedef UINT16 uint16_t;
typedef UINT32 uint32_t;
typedef UINT64 uint64_t;

typedef BOOLEAN bool;

#ifndef _MSC_VER
  typedef long unsigned int size_t;
#endif

typedef int64_t ssize_t;

#define KEYSIZE 32
#define VALSIZE 1024


#ifdef _MSC_VER
#define __attribute__(x)
#endif

#define __asm

#define _EFI_SIZE_T_     UINT32  

#ifdef _EFI_SIZE_T_
/** size_t is the unsigned integer type of the result of the sizeof operator. **/
//typedef _EFI_SIZE_T_  size_t;
#undef _EFI_SIZE_T_
#undef _BSD_SIZE_T_
#endif

typedef INT8        __int8_t;
typedef INT8 *      __caddr_t;          //< core address
typedef uint32_t    __gid_t;            //< group id
typedef uint32_t    __in_addr_t;        //< IP(v4) address
typedef uint16_t    __in_port_t;        //< "Internet" port number
typedef uint32_t    __mode_t;           //< file permissions
typedef int64_t     __off_t;            //< file offset
typedef int32_t     __pid_t;            //< process id
typedef uint8_t     __sa_family_t;      //< socket address family
typedef UINT32      __socklen_t;        //< socket-related datum length
typedef uint32_t    __uid_t;            //< user id
typedef uint64_t    __fsblkcnt_t;       //< fs block count (statvfs)
typedef uint64_t    __fsfilcnt_t;       //< fs file count

typedef INTN        __intptr_t;
typedef UINTN       __uintptr_t;
typedef __uintptr_t uintptr_t;

#include "json_write.h"

//
// Definitions for global constants used by CRT library routines
//
#define RAND_MAX         0x7fffffff

#define INT_MAX          0x7FFFFFFF                  /* Maximum (signed) int value */
#define LONG_MAX         0X7FFFFFFFL                 /* max value for a long */
#define LONG_MIN         (-LONG_MAX-1)               /* min value for a long */
#define ULONG_MAX        0xFFFFFFFF                  /* Maximum unsigned long value */
#define CHAR_BIT         8                           /* Number of bits in a char */
#define ULLONG_MAX       0xffffffffffffffffULL       /* max unsigned long long */
#define LLONG_MAX        0x7fffffffffffffffLL        /* max signed long long */
#define LLONG_MIN        (-0x7fffffffffffffffLL-1)   /* min signed long long */

#define __INT64_C(c)    c ## L

/* Minimum for largest signed integral type.    */
#define INTMAX_MIN                       (-__INT64_C(9223372036854775807)-1)
/* Maximum for largest signed integral type.    */
#define INTMAX_MAX                       (__INT64_C(9223372036854775807))

#define PRIx8               "x" /* uint8_t      */
#define PRIx16              "x" /* uint16_t     */
#define PRIx32              "x" /* uint32_t     */
#define PRIx64              "lx"/* uint64_t     */
#define PRIxLEAST8          "x" /* uint_least8_t    */
#define PRIxLEAST16         "x" /* uint_least16_t   */
#define PRIxLEAST32         "x" /* uint_least32_t   */
#define PRIxLEAST64         "lx"/* uint_least64_t   */
#define PRIxFAST8           "x" /* uint_fast8_t     */
#define PRIxFAST16          "x" /* uint_fast16_t    */
#define PRIxFAST32          "x" /* uint_fast32_t    */
#define PRIxFAST64          "lx"/* uint_fast64_t    */
#define PRIxMAX             "lx"/* uintmax_t        */
#define PRIxPTR             "lx"/* uintptr_t        */

#define PRIX8               "X" /* uint8_t      */
#define PRIX16              "X" /* uint16_t     */
#define PRIX32              "X" /* uint32_t     */
#define PRIX64              "lX"/* uint64_t     */
#define PRIXLEAST8          "X" /* uint_least8_t    */
#define PRIXLEAST16         "X" /* uint_least16_t   */
#define PRIXLEAST32         "X" /* uint_least32_t   */
#define PRIXLEAST64         "lX"/* uint_least64_t   */
#define PRIXFAST8           "X" /* uint_fast8_t     */
#define PRIXFAST16          "X" /* uint_fast16_t    */
#define PRIXFAST32          "X" /* uint_fast32_t    */
#define PRIXFAST64          "lX"/* uint_fast64_t    */
#define PRIXMAX             "lX"/* uintmax_t        */
#define PRIXPTR             "lX"/* uintptr_t        */
#define SCNu64              "lu"/* uint64_t     */

#define INT_MAX             0x7FFFFFFF          /* Maximum (signed) int value */

#define UINT8_MAX           0xff 
#define UINT16_MAX          0xffff
#define UINT32_MAX          0xffffffff
#define UINT64_MAX          0xffffffffffffffff
#define UINT64_C(c)         c ## ULL

#define UCHAR_MAX           0xff                /* maximum unsigned char value */

#define false               0
#define true                1

#define PRIu8               "u"     /* uint8_t      */
#define PRIu16              "u"     /* uint16_t     */
#define PRIu32              "u"     /* uint32_t     */
#define PRIu64              "lu"    /* uint64_t     */

#define LOG_EMERG            0      //  system is unusable
#define LOG_ALERT            1      //  action must be taken immediately
#define LOG_CRIT             2      //  critical conditions
#define LOG_ERR              3      //  error conditions
#define LOG_WARNING          4      //  warning conditions
#define LOG_NOTICE           5      //  normal but significant condition
#define LOG_INFO             6      //  informational
#define LOG_DEBUG            7      //  debug - level messages

#endif
