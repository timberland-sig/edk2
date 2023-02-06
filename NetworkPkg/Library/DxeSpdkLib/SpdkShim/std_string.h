/*-
 *   std_string.h - Implements std library for string functions

 *   Copyright (c) 2020, Dell EMC All rights reserved
 *   SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#ifndef STD_STRING_H
#define STD_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include "stdlib_types.h"

#define MAX_STRING_SIZE 0x1000
#define MAX_SLEEP_DELAY 0xfffffffe
#define intmax_t long long

#define calloc(count, size)  AllocateZeroPool ((count) * (size))
#define realloc(buffer, new_size) ReallocatePool (sizeof (*buffer), new_size, buffer)
#define free(x) FreePool(x)

#define UUID_SIZE 16

typedef struct {
    uint8_t b[UUID_SIZE];
} uuid_t;

#define UUID_INIT(a, b, c, d0, d1, d2, d3, d4, d5, d6, d7)      \
((uuid_t)               \
{{ ((a) >> 24) & 0xff, ((a) >> 16) & 0xff, ((a) >> 8) & 0xff, (a) & 0xff, \
   ((b) >> 8) & 0xff, (b) & 0xff,         \
   ((c) >> 8) & 0xff, (c) & 0xff,         \
   (d0), (d1), (d2), (d3), (d4), (d5), (d6), (d7) }})

/*
* The length of a UUID string ("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee")
* not including trailing NUL.
*/
#define UUID_STRING_LEN   36

#if !defined(UNUSED)
#define UNUSED(x) ((void)(x)) /* to avoid warnings */
#endif

char* 
strcasestr (
  const char *s, 
  const char *find
  ); 

char 
toupper (
  char s
  );
  
char 
tolower (
  char s
  );
  
int 
isspace (
  int c
  );
  
int 
isdigit (
  int c
  );
  
int 
isupper (
  int c
  );
  
int 
isalpha (
  int c
  );
  
int isalnum (
  int c
  );
  
char 
  *strchr (
  const char *s, 
  int c
  );
  
int 
strcmp (
  const char *src1, 
  const char *src2
  );
  
int 
getpid ();

size_t 
strspn (
  const char *str, 
  const char *chars
  );
  
unsigned int 
strcspn (
  const char *s1, 
  const char *s2
  );
  
long 
strtol (
  const char * __restrict nptr, 
  char       ** __restrict endptr, 
  int        base
  );
  
long long 
strtoll (
  const char * __restrict nptr, 
  char       ** __restrict endptr, 
  int        base
  );

int 
snprintf (
  char       *str, 
  size_t     n, 
  char const *fmt,
  ...
  );
  
int 
usleep (
  UINT32 Microseconds
  );
  
int 
rand ();

void 
srand (
  unsigned int seed
  );

int
strerror_r (
  int errnum, 
  char *buf,
  int buflen);

const char
*spdk_strerror (
  int errnum
  );
  
char 
*strcasestr (
  const char *String, 
  const char *SearchString
  );

#define strlen(str)                             (size_t)(AsciiStrnLenS(str,MAX_STRING_SIZE))
#define strnlen(s, max)                         AsciiStrnLenS((CONST CHAR8*)s, (UINTN)max)
#define memcpy(dest,source,count)               CopyMem(dest,source,(UINTN)(count))
#define memset(dest,ch,count)                   SetMem(dest,(UINTN)(count),(UINT8)(ch))
#define memchr(buf,ch,count)                    ScanMem8(buf,(UINTN)(count),(UINT8)ch)
#define memcmp(buf1,buf2,count)                 (int)(CompareMem(buf1,buf2,(UINTN)(count)))
#define memmove(dest,source,count)              CopyMem(dest,source,(UINTN)(count))
#define strcpy(strDest,strSource)               AsciiStrCpyS(strDest,MAX_STRING_SIZE,strSource)
#define strncpy(strDest,strSource,count)        AsciiStrnCpyS(strDest,MAX_STRING_SIZE,strSource,(UINTN)count)
#define strcat(strDest,strSource)               AsciiStrCatS(strDest,MAX_STRING_SIZE,strSource)
#define strncmp(string1,string2,count)          (int)(AsciiStrnCmp(string1,string2,(UINTN)(count)))
#define strcasecmp(str1,str2)                   (int)AsciiStriCmp(str1,str2)
#define sprintf(buf,...)                        AsciiSPrint(buf,MAX_STRING_SIZE,__VA_ARGS__)
#define assert(expression)                      ASSERT(expression)
#define offsetof(type,member)                   OFFSET_OF(type,member)
#define atoi(nptr)                              AsciiStrDecimalToUintn(nptr)
#define gettimeofday(tvp,tz)                    do { (tvp)->tv_sec = time(NULL); (tvp)->tv_usec = 0; } while (0)

int 
EFIAPI 
sprintf_s (
  char       *str, 
  size_t     sizeOfBuffer, 
  char const *fmt,
  ...
  );
  
#define snprintf(str, n, fmt, ...) sprintf_s(str, n, fmt, __VA_ARGS__)

#define vsnprintf AsciiVSPrint
#define sscanf(str, fmt, ...) 1

#ifdef _MSC_VER
// Variable used to traverse the list of arguments. This type can vary by
// implementation and could be an array or structure.

typedef struct __va_list { void *__ap; } va_list;

#define _INT_SIZE_OF(n) ((sizeof (n) + sizeof (UINTN) - 1) &~(sizeof (UINTN) - 1))

#define VA_ARG(Marker, TYPE) (*(TYPE *) ((Marker += _INT_SIZE_OF (TYPE)) - _INT_SIZE_OF (TYPE)))
#define VA_START(Marker, Parameter) (Marker = (VA_LIST) ((UINTN) & (Parameter) + _INT_SIZE_OF (Parameter)))
#define VA_END(Marker) (Marker = (VA_LIST) 0)
#endif

#define va_copy(dest, src) memcpy(dest, src, sizeof(va_list))

#define va_list     VA_LIST
#define va_arg      VA_ARG
#define va_start    VA_START
#define va_end      VA_END

#endif
