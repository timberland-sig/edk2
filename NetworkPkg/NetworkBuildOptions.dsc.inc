## @file
# Network DSC include file for [BuildOptions] sections of all Architectures.
#
# This file can be included in the [BuildOptions*] section(s) of a platform DSC file
# by using "!include NetworkPkg/NetworkBuildOptions.dsc.inc", to specify the C language
# feature test macros (eg., API deprecation macros) according to the flags described
# in "NetworkDefines.dsc.inc".
#
# Supported tool chain families: "GCC", "INTEL", "MSFT".
#
# Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
#
#    SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

!if $(NETWORK_ISCSI_ENABLE) == TRUE && $(NETWORK_ISCSI_MD5_ENABLE) == TRUE
  MSFT:*_*_*_CC_FLAGS = /D ENABLE_MD5_DEPRECATED_INTERFACES
  INTEL:*_*_*_CC_FLAGS = /D ENABLE_MD5_DEPRECATED_INTERFACES
  GCC:*_*_*_CC_FLAGS = -D ENABLE_MD5_DEPRECATED_INTERFACES
!endif

!if $(NETWORK_NVMEOF_ENABLE) == TRUE
  MSFT:DEBUG_*_*_CC_FLAGS = /Od /Oy-
  MSFT:NOOPT_*_*_CC_FLAGS = /Od /Oy-
  GCC:DEBUG_CLANGPDB_*_CC_FLAGS =-O0 -Wno-unused-command-line-argument -Wno-incompatible-pointer-types -Wno-enum-conversion -Wno-incompatible-pointer-types -Wno-sometimes-uninitialized -Wno-constant-conversion -Wno-main-return-type

  MSFT:*_*_*_DLINK_FLAGS     = /ALIGN:4096 /FILEALIGN:4096 /SUBSYSTEM:CONSOLE
  MSFT:DEBUG_*_*_DLINK_FLAGS = /EXPORT:InitializeDriver=$(IMAGE_ENTRY_POINT) /BASE:0x10000
  MSFT:NOOPT_*_*_DLINK_FLAGS = /EXPORT:InitializeDriver=$(IMAGE_ENTRY_POINT) /BASE:0x10000
!endif
