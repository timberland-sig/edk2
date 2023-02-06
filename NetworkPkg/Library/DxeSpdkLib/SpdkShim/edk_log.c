/*-
 * edk_log.h - Implements shim for SPDK log

 * Copyright (c) 2020, Dell EMC All rights reserved
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
 
#include "edk_log.h"
#include "nvme_internal.h"

VOID
EFIAPI
spdk_logger (
  UINTN  log_level,
  CHAR8  *format,
  ...
)
{
  va_list ap;
  CHAR8   *ptr1 = format;
  UINTN   Index = 0;
  CHAR8   *format1;
  CHAR8   *ptr2;
  CHAR8   buf[1024];

  if ((log_level & PcdGet32 (PcdDebugPrintErrorLevel)) == 0) {
    return;
  }

  if (format == NULL) {
    return;
  }

  for (Index = 0; format[Index] != '\0'; Index++);
  ptr2 = format1 = AllocateZeroPool ((Index + 1) * sizeof (CHAR8));
  CopyMem (format1, ptr1, (Index + 1) * sizeof (CHAR8));

  while (*format1 != '\0') {
    if (*format1 == '%' && *(format1 + 1) == 's') {
      *(format1 + 1) = 'a';
    }
    format1++;
  }
 
  va_start (ap, format);
  vsnprintf (buf, sizeof (buf), ptr2, ap);
  va_end (ap);
  DEBUG ((log_level, buf));
  FreePool (ptr2);
}
