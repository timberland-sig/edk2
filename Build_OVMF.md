# Building OvmfPkgX64 with Network Configuration Flags

## Overview

The `build_ovmf.sh` script builds the OvmfPkgX64 platform with full control over
the `NETWORK_*` compile flags defined in `NetworkPkg/NetworkDefines.dsc.inc`.

## Network Flag Architecture

The network flags are resolved in layers:

1. **Command line** (`-D FLAG=VALUE`) — highest priority
2. **Platform DSC** (`OvmfPkg/OvmfPkgX64.dsc`) — overrides per-platform
3. **NetworkDefines.dsc.inc** — provides defaults for any flag not already defined

The `build_ovmf.sh` script passes all flags explicitly via `-D`, so command-line
values always win regardless of what the DSC files contain.

## Available Flags

| Flag | Script Option | Default | Description |
|------|---------------|---------|-------------|
| `NETWORK_ENABLE` | `--network on\|off` | `TRUE` | Enable/disable entire network stack |
| `NETWORK_SNP_ENABLE` | `--snp on\|off` | `TRUE` | Common SNP (Simple Network Protocol) driver |
| `NETWORK_IP4_ENABLE` | `--ip4 on\|off` | `TRUE` | IPv4 stack |
| `NETWORK_IP6_ENABLE` | `--ip6 on\|off` | `TRUE` | IPv6 stack |
| `NETWORK_TLS_ENABLE` | `--tls on\|off` | `FALSE` | TLS support (requires OpenSSL) |
| `NETWORK_HTTP_ENABLE` | `--http on\|off` | `FALSE` | General HTTP(S) protocol |
| `NETWORK_HTTP_BOOT_ENABLE` | `--http-boot on\|off` | `FALSE` | HTTP(S) boot |
| `NETWORK_ALLOW_HTTP_CONNECTIONS` | `--allow-http on\|off` | `TRUE` | Allow plaintext HTTP (not just HTTPS) |
| `NETWORK_ISCSI_ENABLE` | `--iscsi on\|off` | `TRUE` | iSCSI support |
| `NETWORK_ISCSI_MD5_ENABLE` | `--iscsi-md5 on\|off` | `TRUE` | MD5 in iSCSI CHAP authentication |
| `NETWORK_NVMEOF_ENABLE` | `--nvmeof on\|off` | `TRUE` | NVMe-oF over TCP |
| `NETWORK_VLAN_ENABLE` | `--vlan on\|off` | `TRUE` | VLAN support |
| `NETWORK_PXE_BOOT_ENABLE` | `--pxe on\|off` | `TRUE` | PXE boot |

Default values reflect what `OvmfPkgX64.dsc` sets, with remaining flags
inheriting from `NetworkDefines.dsc.inc`.

## Build Constraints

The following constraints are enforced by `NetworkDefines.dsc.inc` at build time:

- If `NETWORK_ENABLE=TRUE`, at least one of `NETWORK_IP4_ENABLE` or
  `NETWORK_IP6_ENABLE` must be `TRUE`.
- If `NETWORK_HTTP_BOOT_ENABLE=TRUE` or `NETWORK_HTTP_ENABLE=TRUE`, then either
  `NETWORK_TLS_ENABLE=TRUE` or `NETWORK_ALLOW_HTTP_CONNECTIONS=TRUE` must be set.
  Otherwise there is no usable HTTP transport and the build will fail.

## Usage

```bash
./build_ovmf.sh [OPTIONS]
```

### Build Options

| Option | Description |
|--------|-------------|
| `-b DEBUG\|RELEASE\|NOOPT` | Build target (default: `DEBUG`) |
| `-t TOOLCHAIN` | Toolchain tag (default: `GCC` on Linux) |
| `-n THREADS` | Number of build threads (default: `0` = auto-detect) |
| `--clean` | Clean the build output directory (does not rebuild) |
| `-h, --help` | Show help |

### Examples

```bash
# Default build (DEBUG, all current flags)
./build_ovmf.sh

# Release build with NVMe-oF and iSCSI disabled
./build_ovmf.sh -b RELEASE --nvmeof off --iscsi off

# Disable the entire network stack
./build_ovmf.sh --network off

# Enable TLS and HTTP boot, disable PXE
./build_ovmf.sh --tls on --http-boot on --pxe off

# Clean the build directory
./build_ovmf.sh --clean

# NOOPT build for debugging, NVMe-oF only (no iSCSI, no PXE)
./build_ovmf.sh -b NOOPT --iscsi off --pxe off
```

## Output

The firmware image is written to:

```
Build/OvmfX64/<BUILDTARGET>_<TOOLCHAIN>/FV/OVMF.fd
```

For example, a default DEBUG build with GCC produces:

```
Build/OvmfX64/DEBUG_GCC/FV/OVMF.fd
```

## Prerequisites

- GCC 8 or later (on Linux)
- EDK2 submodules initialized (`git submodule update --init --recursive`)
- BaseTools built (the script handles this automatically if needed)
