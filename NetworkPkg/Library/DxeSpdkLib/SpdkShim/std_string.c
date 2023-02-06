/*-
 * std_string.c - Implements std C string functions

 * Copyright (c) 2020, Dell EMC All rights reserved
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "spdk/stdinc.h"

char *
strchr (
  const char *s, 
  int        c
  )
{
  char pattern[2];
 
  pattern[0] = c;
  pattern[1] = 0;
 
  return AsciiStrStr (s, pattern);
}

char 
toupper (
  char s
  )
{
  if (s != '\0') {
      if (s >= 'a' && s <= 'z') {
          s = s - 32;
      }
  }
  return s;
}

char 
tolower (
  char s
  )
{
  if (s != '\0') {
      if (s >= 'A' && s <= 'Z') {
          s = s + 32;
      }
  }
  return s;
}

int 
strcmp (
  const char *src1, 
  const char *src2
  )
{
  int i = 0;
  while ((src1[i]!='\0') || (src2[i]!='\0')) {
          if (src1[i] > src2[i]) {
                  return 1;
          }
          if (src1[i] < src2[i]) {
                  return -1;
          }
          i++;
  }
  return 0;
}

int 
getpid() 
{
  return 0;
}

size_t strspn (
  const char *str,
  const char *chars
  ) 
{
  size_t i, j;
  for (i = 0; str[i] != '\0'; i++) {
    for (j = 0; chars[j] != str[i]; j++) {
      if (chars[j] == '\0') {
        return i;
      }
    }
  }
  return i;
}

unsigned int 
strcspn (
  const char *s1, 
  const char *s2
  )
{
  unsigned int len =0;
  
  if((s1 == NULL) || (s2 == NULL))
          return len;

  while (*s1)
  {
    if(strchr (s2,*s1)) {
      return len;
    } else {
      s1++;
      len++;
    }
  }
  return len;
}

int 
isspace (
  int c
  )
{
  return c == ' ' || c == '\t';
}

int 
isdigit (
  int c
  )
{
  return (('0' <= (c)) && ((c) <= '9'));
}

int 
isupper (
  int c
  )
{
  return (('A' <= (c)) && ((c) <= 'Z'));
}

int 
isalpha (
  int c
  )
{
  return ((('a' <= (c)) && ((c) <= 'z')) ||
    (('A' <= (c)) && ((c) <= 'Z')));
}

int 
isalnum (
  int c
  )
{
  return ((('a' <= (c)) && ((c) <= 'z')) ||
    (('A' <= (c)) && ((c) <= 'Z')) ||
    (('0' <= (c)) && ((c) <= '9')));
}

static int
Digit2Val (
  int c
  )
{
  if (isalpha (c)) {  /* If c is one of [A-Za-z]... */
      c = toupper (c) - 7;   // Adjust so 'A' is ('9' + 1)
  }
  return c - '0';   // Value returned is between 0 and 35, inclusive.
}

long
strtol (
  const char * __restrict nptr,
  char       ** __restrict endptr, 
  int        base
  )
{
  const char *pEnd;
  long        Result = 0;
  long        Previous;
  int         temp;
  BOOLEAN     Negative = FALSE;

  pEnd = nptr;

  if ((base < 0) || (base == 1) || (base > 36)) {
      if (endptr != NULL) {
          *endptr = NULL;
      }
      return 0;
  }
  // Skip leading spaces.
  while (isspace(*nptr)) {
      ++nptr;
  }

  // Process Subject sequence: optional sign followed by digits.
  if (*nptr == '+') {
      Negative = FALSE;
      ++nptr;
  }
  else if (*nptr == '-') {
      Negative = TRUE;
      ++nptr;
  }

  if (*nptr == '0') {  /* Might be Octal or Hex */
      if (toupper (nptr[1]) == 'X') {   /* Looks like Hex */
          if ((base == 0) || (base == 16)) {
              nptr += 2;  /* Skip the "0X"      */
              base = 16;  /* In case base was 0 */
          }
      }
      else {    /* Looks like Octal */
          if ((base == 0) || (base == 8)) {
              ++nptr;     /* Skip the leading "0" */
              base = 8;   /* In case base was 0   */
          }
      }
  }
  if (base == 0) {   /* If still zero then must be decimal */
      base = 10;
  }
  if (*nptr == '0') {
      for (; *nptr == '0'; ++nptr);  /* Skip any remaining leading zeros */
      pEnd = nptr;
  }

  while (isalnum (*nptr) && ((temp = Digit2Val (*nptr)) < base)) {
      Previous = Result;
      Result = (Result * base) + (long int)temp;
      if (Result <= Previous) {   // Detect Overflow
          if (Negative) {
              Result = LONG_MIN;
          } else {
              Result = LONG_MAX;
          }
          Negative = FALSE;
          errno = ERANGE;
          break;
      }
      pEnd = ++nptr;
  }
  if (Negative) {
      Result = -Result;
  }

  // Save pointer to final sequence
  if (endptr != NULL) {
      *endptr = (char *)pEnd;
  }
  return Result;
}

long long
strtoll (
  const char * __restrict nptr,
  char       ** __restrict endptr, 
  int        base
  )
{
  const char  *pEnd;
  long long   Result = 0;
  long long   Previous = 0;
  int         temp = 0;
  BOOLEAN     Negative = FALSE;

  pEnd = nptr;

  if ((base < 0) || (base == 1) || (base > 36)) {
      if (endptr != NULL) {
          *endptr = NULL;
      }
      return 0;
  }
  // Skip leading spaces.
  while (isspace (*nptr))   ++nptr;

  // Process Subject sequence: optional sign followed by digits.
  if (*nptr == '+') {
      Negative = FALSE;
      ++nptr;
  }
  else if (*nptr == '-') {
      Negative = TRUE;
      ++nptr;
  }

  if (*nptr == '0') {  /* Might be Octal or Hex */
      if (toupper (nptr[1]) == 'X') {   /* Looks like Hex */
          if ((base == 0) || (base == 16)) {
              nptr += 2;  /* Skip the "0X"      */
              base = 16;  /* In case base was 0 */
          }
      } else {    /* Looks like Octal */
          if ((base == 0) || (base == 8)) {
              ++nptr;     /* Skip the leading "0" */
              base = 8;   /* In case base was 0   */
          }
      }
  }
  if (base == 0) {   /* If still zero then must be decimal */
      base = 10;
  }
  if (*nptr == '0') {
      for (; *nptr == '0'; ++nptr);  /* Skip any remaining leading zeros */
      pEnd = nptr;
  }

  while (isalnum (*nptr) && ((temp = Digit2Val (*nptr)) < base)) {
      Previous = Result;
      Result = (Result * base) + (long long int)temp;
      if (Result <= Previous) {   // Detect Overflow
          if (Negative) {
              Result = LLONG_MIN;
          }
          else {
              Result = LLONG_MAX;
          }
          Negative = FALSE;
          errno = ERANGE;
          break;
      }
      pEnd = ++nptr;
  }
  if (Negative) {
      Result = -Result;
  }

  // Save pointer to final sequence
  if (endptr != NULL) {
      *endptr = (char *)pEnd;
  }
  return Result;
}

static UINT32 next = 1;

int
rand ()
{
    INT32 hi, lo, x;

    /* Can't be initialized with 0, so use another value. */
    if (next == 0)
        next = 123459876;
    hi = next / 127773;
    lo = next % 127773;
    x = 16807 * lo - 2836 * hi;
    if (x < 0)
        x += 0x7fffffff;
    return ((next = x) % ((UINT32)RAND_MAX + 1));
}

void
srand (
  unsigned int seed
  )
{
  next = (UINT32)seed;
}

int
EFIAPI 
sprintf_s (
  char       *str, 
  size_t     sizeOfBuffer, 
  char const *fmt, 
  ...
  )
{
  VA_LIST Marker;
  int     NumberOfPrinted = 0;
  UINTN   Index = 0;
  char    *fmt1 = NULL;
  CHAR8   *fmtptr = NULL;

  if (fmt == NULL) {
    return NumberOfPrinted;
  }

  for (Index = 0; fmt[Index] != '\0'; Index++);
  fmtptr = fmt1 = AllocateZeroPool ((Index + 1) * sizeof (char));
  CopyMem (fmt1, fmt, (Index + 1) * sizeof (char));

  while (*fmt1 != '\0') {
    if (*fmt1 == '%' && *(fmt1 + 1) == 's') {
      *(fmt1 + 1) = 'a';
    }
    fmt1++;
  }

  VA_START(Marker, fmt);
  NumberOfPrinted = (int)AsciiVSPrint (str, sizeOfBuffer, fmtptr, Marker);
  VA_END (Marker);
  FreePool (fmtptr);

  return NumberOfPrinted;
}

int
usleep (
  UINT32 Microseconds
  )
{
    MicroSecondDelay (Microseconds);
    return (0);
}

char *
strcasestr (
  const char *String, 
  const char *SearchString
  )
{
  const char *FirstMatch;
  const char *SearchStringTmp;
  char Src;
  char Dst;

  if (*SearchString == '\0') {
      return (char *) String;
   }

   while (*String != '\0') {
      SearchStringTmp = SearchString;
      FirstMatch = String;

      while ((*SearchStringTmp != '\0')
              && (*String != '\0')) {
          Src = *String;
          Dst = *SearchStringTmp;

      if ((Src >= 'A') && (Src <= 'Z')) {
          Src += ('a' - 'A');
      }

      if ((Dst >= 'A') && (Dst <= 'Z')) {
          Dst += ('a' - 'A');
      }

      if (Src != Dst) {
          break;
      }

      String++;
      SearchStringTmp++;
  }

  if (*SearchStringTmp == '\0') {
      return (char *) FirstMatch;
  }

  String = FirstMatch + 1;
  }

  return NULL;
}

int
strerror_r (
  int errnum, 
  char *buf,
  int buflen
  ) 
{
  return 0;
}

const char
*spdk_strerror (
  int errnum
  )
{
  return " ";
}