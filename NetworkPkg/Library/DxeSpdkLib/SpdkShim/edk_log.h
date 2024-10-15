/** @file
  edk_log.h - Implements shim for SPDK log.

Copyright (c) 2022 - 2024, Dell Inc. or its subsidiaries. All Rights Reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef EDK_LOG_H
#define EDK_LOG_H

#define SPDK_LOG_H  /* To avoid spdk's log.h */
#define SPDK_LOG_REGISTER_COMPONENT(FLAG)
#define SPDK_DEBUGLOG_FLAG_ENABLED(name)  FALSE

#define SPDK_LOG_DEPRECATION_REGISTER(tag, desc, release, rate)
#define SPDK_LOG_DEPRECATED(tag)

void
EFIAPI
spdk_logger (
  UINTN  log_level,
  CHAR8  *format,
  ...
  );

#define SPDK_NOTICELOG(...) \
  do {                                            \
    DEBUG ((DEBUG_INFO, "NVMeOFLog:%d:%a:", __LINE__, __func__)); \
    spdk_logger(DEBUG_INFO, ##__VA_ARGS__);     \
  } while (0)
#define SPDK_WARNLOG(...) \
  do {                                            \
    DEBUG ((DEBUG_WARN, "NVMeOFLog:%d:%a:", __LINE__, __func__)); \
    spdk_logger(DEBUG_WARN, ##__VA_ARGS__);     \
  } while (0)
#define SPDK_ERRLOG(...) \
  do {                                            \
    DEBUG ((DEBUG_ERROR, "NVMeOFLog:%d:%a:", __LINE__, __func__)); \
    spdk_logger (DEBUG_ERROR, ##__VA_ARGS__);        \
  } while (0)
#define SPDK_PRINTF(...) \
  do {                                            \
    DEBUG ((DEBUG_INFO, "NVMeOFLog:%d:%a:", __LINE__, __func__)); \
    spdk_logger (DEBUG_INFO, ##__VA_ARGS__);     \
  } while (0)
#define SPDK_INFOLOG(FLAG, ...)                                 \
  do {                                            \
    DEBUG ((DEBUG_INFO, "NVMeOFLog:%d:%a:", __LINE__, __func__)); \
    spdk_logger (DEBUG_INFO, ##__VA_ARGS__);     \
  } while (0)
#define SPDK_DEBUGLOG(FLAG, ...)                                \
  do {                                            \
    DEBUG ((DEBUG_VERBOSE, "NVMeOFLog:%d:%a:", __LINE__, __func__)); \
    spdk_logger (DEBUG_VERBOSE, ##__VA_ARGS__);      \
  } while (0)
#define SPDK_LOGDUMP(FLAG, ...)

#endif
