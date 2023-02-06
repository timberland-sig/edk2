/*-
 * edk_log.h - Implements shim for SPDK log

 * Copyright (c) 2020, Dell EMC All rights reserved
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#ifndef EDK_LOG_H
#define EDK_LOG_H

#define SPDK_LOG_H /* To avoid spdk's log.h */
#define SPDK_LOG_REGISTER_COMPONENT(FLAG)

void 
EFIAPI  
spdk_logger (
  UINTN log_level, 
  CHAR8 *format, 
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