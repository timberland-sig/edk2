#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0+
# Copyright (C) 2023 John Meneghini <jmeneghi@redhat.com> All rights reserved.
#
# vim: set tabstop=4 shiftwidth=4 expandtab :
#
make -C BaseTools clean
rm -rf Build
rm -f build.log
make -C BaseTools
source edksetup.sh
build -t GCC -a X64 -p OvmfPkg/OvmfPkgX64.dsc
