# SPDK File Usage in DxeSpdkLib Build

This document describes which files from the `NetworkPkg/Library/DxeSpdkLib/spdk`
submodule (upstream SPDK v23.01.x, commit `186986c`) are used to build the
`DxeSpdkLib` library, and how the `SpdkShim` directory provides EDK2-specific
replacements that take precedence over their upstream counterparts.

All source files are listed in `NetworkPkg/Library/DxeSpdkLib/DxeSpdkLib.inf`
under the `[Sources]` section.  The build is driven by
`OvmfPkg/OvmfPkgX64.dsc` (via `build.sh`) with `NETWORK_NVMEOF_ENABLE = TRUE`.

## Include Path Order (from NetworkPkg.dec)

The include path order determines which header wins when the same relative path
exists in both SpdkShim and the upstream submodule.  SpdkShim directories are
listed first, so they take precedence:

1. `Library/DxeSpdkLib/SpdkShim/`
2. `Library/DxeSpdkLib/SpdkShim/sys/`
3. `Library/DxeSpdkLib/SpdkShim/spdk/`
4. `Library/DxeSpdkLib/spdk/include/`
5. `Library/DxeSpdkLib/spdk/lib/nvme/`

## Compiled Source Files (.c)

The DxeSpdkLib.inf compiles 32 `.c` files total: 14 from the upstream SPDK
submodule and 18 from the SpdkShim directory.

### Upstream SPDK Source Files (14 files)

These `.c` files are compiled directly from the `spdk/` submodule:

| # | File | Purpose |
|---|------|---------|
| 1 | `spdk/lib/util/crc16.c` | CRC-16 calculation |
| 2 | `spdk/lib/util/crc32.c` | CRC-32 calculation |
| 3 | `spdk/lib/util/crc32c.c` | CRC-32C (Castagnoli) calculation |
| 4 | `spdk/lib/util/bit_array.c` | Bit array / bit pool utilities |
| 5 | `spdk/lib/nvme/nvme_ctrlr_cmd.c` | NVMe controller admin commands |
| 6 | `spdk/lib/nvme/nvme_ctrlr_ocssd_cmd.c` | NVMe OCSSD controller commands |
| 7 | `spdk/lib/nvme/nvme_io_msg.c` | NVMe I/O message passing |
| 8 | `spdk/lib/nvme/nvme_ns.c` | NVMe namespace management |
| 9 | `spdk/lib/nvme/nvme_ns_cmd.c` | NVMe namespace I/O commands |
| 10 | `spdk/lib/nvme/nvme_ns_ocssd_cmd.c` | NVMe OCSSD namespace commands |
| 11 | `spdk/lib/nvme/nvme_poll_group.c` | NVMe poll group management |
| 12 | `spdk/lib/nvme/nvme_quirks.c` | NVMe device quirks database |
| 13 | `spdk/lib/sock/sock.c` | Socket abstraction layer |
| 14 | `spdk/lib/util/string.c` | String utility functions |

### SpdkShim Source Files (18 files)

These files are EDK2-specific implementations. Six of them (`SpdkShim/spdk/lib/nvme/*.c`)
are modified versions of upstream SPDK files that replace their upstream counterparts;
the remaining twelve are new EDK2-only shim code.

#### Shimmed Replacements for Upstream SPDK Files (6 files)

These files exist in both `SpdkShim/spdk/lib/nvme/` and `spdk/lib/nvme/`.
The SpdkShim version is compiled instead of the upstream version:

| # | Compiled (SpdkShim) | Replaced Upstream File |
|---|---------------------|----------------------|
| 1 | `SpdkShim/spdk/lib/nvme/nvme.c` | `spdk/lib/nvme/nvme.c` |
| 2 | `SpdkShim/spdk/lib/nvme/nvme_ctrlr.c` | `spdk/lib/nvme/nvme_ctrlr.c` |
| 3 | `SpdkShim/spdk/lib/nvme/nvme_fabric.c` | `spdk/lib/nvme/nvme_fabric.c` |
| 4 | `SpdkShim/spdk/lib/nvme/nvme_qpair.c` | `spdk/lib/nvme/nvme_qpair.c` |
| 5 | `SpdkShim/spdk/lib/nvme/nvme_transport.c` | `spdk/lib/nvme/nvme_transport.c` |
| 6 | `SpdkShim/spdk/lib/nvme/nvme_tcp.c` | `spdk/lib/nvme/nvme_tcp.c` |

#### EDK2-Only Shim Source Files (12 files)

These files have no upstream counterpart; they provide UEFI/EDK2 environment
adaptation, POSIX-to-UEFI translation, and NVMe-oF connection management:

| # | File | Purpose |
|---|------|---------|
| 1 | `SpdkShim/sys_types.c` | POSIX type definitions for UEFI |
| 2 | `SpdkShim/edk_nvme.c` | EDK2 NVMe driver interface |
| 3 | `SpdkShim/pthread_shim.c` | pthread stubs for single-threaded UEFI |
| 4 | `SpdkShim/json_write.c` | JSON serialization for NVMe-oF |
| 5 | `SpdkShim/edk_sock.c` | EDK2 socket layer (wraps TcpIo) |
| 6 | `SpdkShim/env.c` | SPDK environment shim (memory, etc.) |
| 7 | `SpdkShim/dif_shim.c` | Data integrity field stubs |
| 8 | `SpdkShim/uuid.c` | UUID generation for UEFI |
| 9 | `SpdkShim/std_string.c` | Standard C string functions for UEFI |
| 10 | `SpdkShim/edk_log.c` | Logging bridge to EDK2 DebugLib |
| 11 | `SpdkShim/edk_nvme_ctrlr.c` | EDK2 NVMe controller management |
| 12 | `SpdkShim/edk_nvme_fabric.c` | EDK2 NVMe-oF fabric operations |

## Header Files

### Upstream SPDK Headers Used from `spdk/include/spdk/` (27 files)

These public SPDK headers are included (directly or transitively) during
compilation and are resolved from the upstream `spdk/include/` directory:

| # | Header | Included By |
|---|--------|-------------|
| 1 | `spdk/assert.h` | `sock.h` |
| 2 | `spdk/bit_array.h` | `bit_array.c`, `nvme_internal.h` |
| 3 | `spdk/bit_pool.h` | `bit_array.c` |
| 4 | `spdk/crc16.h` | `crc16.c` |
| 5 | `spdk/crc32.h` | `crc32.c`, `crc32c.c` |
| 6 | `spdk/dif.h` | `spdk_internal/nvme_tcp.h` |
| 7 | `spdk/endian.h` | `nvme_spec.h` |
| 8 | `spdk/env.h` | `bit_array.c`, `sock.c`, `nvme.h` |
| 9 | `spdk/json.h` | `sock.h` |
| 10 | `spdk/log.h` | `sock.c`, `nvme_internal.h` |
| 11 | `spdk/memory.h` | `nvme_internal.h` |
| 12 | `spdk/mmio.h` | `nvme_internal.h` |
| 13 | `spdk/nvme.h` | `nvme_ctrlr_cmd.c`, `nvme_internal.h` |
| 14 | `spdk/nvme_intel.h` | `nvme_internal.h` |
| 15 | `spdk/nvme_ocssd.h` | `nvme_ctrlr_ocssd_cmd.c`, `nvme_ns_ocssd_cmd.c` |
| 16 | `spdk/nvme_ocssd_spec.h` | `nvme_ocssd.h` |
| 17 | `spdk/nvme_spec.h` | `nvme.h` |
| 18 | `spdk/nvmf_spec.h` | `nvme.h`, `nvme_internal.h` |
| 19 | `spdk/pci_ids.h` | `nvme_internal.h` |
| 20 | `spdk/queue.h` | `nvme_internal.h`, `spdk_internal/sock.h` |
| 21 | `spdk/queue_extras.h` | `nvme_internal.h` |
| 22 | `spdk/sock.h` | `sock.c`, `spdk_internal/sock.h`, `spdk_internal/nvme_tcp.h` |
| 23 | `spdk/string.h` | `string.c` |
| 24 | `spdk/thread.h` | SpdkShim sources |
| 25 | `spdk/trace.h` | SpdkShim sources |
| 26 | `spdk/tree.h` | `nvme_internal.h` |
| 27 | `spdk/util.h` | `bit_array.c`, `sock.c`, `nvme_internal.h` |
| 28 | `spdk/uuid.h` | `nvme_internal.h` |

### SpdkShim Headers That Override Upstream (4 `spdk/` + 2 `spdk_internal/`)

These SpdkShim headers shadow their upstream equivalents because
`SpdkShim/spdk/` appears before `spdk/include/` in the include path:

| # | SpdkShim Header | Overrides Upstream |
|---|----------------|-------------------|
| 1 | `SpdkShim/spdk/barrier.h` | `spdk/include/spdk/barrier.h` |
| 2 | `SpdkShim/spdk/config.h` | `spdk/include/spdk/config.h` |
| 3 | `SpdkShim/spdk/likely.h` | `spdk/include/spdk/likely.h` |
| 4 | `SpdkShim/spdk/stdinc.h` | `spdk/include/spdk/stdinc.h` |
| 5 | `SpdkShim/spdk/spdk_internal/nvme_tcp.h` | `spdk/include/spdk_internal/nvme_tcp.h` |
| 6 | `SpdkShim/spdk/spdk_internal/sock.h` | `spdk/include/spdk_internal/sock.h` |

### Upstream Internal Headers from `spdk/lib/nvme/` (3 files)

These internal headers are resolved via the `spdk/lib/nvme/` include path:

| # | Header | Used By |
|---|--------|---------|
| 1 | `spdk/lib/nvme/nvme_internal.h` | All NVMe `.c` files |
| 2 | `spdk/lib/nvme/nvme_io_msg.h` | `nvme_io_msg.c` and shimmed files |
| 3 | `spdk/lib/util/util_internal.h` | `crc32.c`, `crc32c.c` |

### Upstream `spdk_internal/` Headers Used (2 files)

These are resolved from `spdk/include/` (not overridden by SpdkShim):

| # | Header |
|---|--------|
| 1 | `spdk/include/spdk_internal/assert.h` |
| 2 | `spdk/include/spdk_internal/sgl.h` |

### SpdkShim-Only Headers (17 files)

Headers unique to the SpdkShim layer with no upstream counterpart:

| # | Header | Purpose |
|---|--------|---------|
| 1 | `SpdkShim/edk_log.h` | EDK2 logging interface |
| 2 | `SpdkShim/edk_nvme.h` | EDK2 NVMe driver interface |
| 3 | `SpdkShim/edk_nvme_ctrlr.h` | EDK2 NVMe controller interface |
| 4 | `SpdkShim/edk_nvme_internal.h` | EDK2 NVMe internal definitions |
| 5 | `SpdkShim/edk_nvme_tcp.h` | EDK2 NVMe TCP definitions |
| 6 | `SpdkShim/edk_sock.h` | EDK2 socket interface |
| 7 | `SpdkShim/json_write.h` | JSON serialization interface |
| 8 | `SpdkShim/pthread_shim.h` | pthread stubs |
| 9 | `SpdkShim/std_error.h` | Standard error definitions |
| 10 | `SpdkShim/std_socket.h` | Socket type definitions |
| 11 | `SpdkShim/std_string.h` | String function declarations |
| 12 | `SpdkShim/stdlib_types.h` | Standard library type stubs |
| 13 | `SpdkShim/sys_types.h` | POSIX sys/types for UEFI |
| 14 | `SpdkShim/sys/cdefs.h` | BSD compiler definitions |
| 15 | `SpdkShim/sys/epoll.h` | epoll stubs |
| 16 | `SpdkShim/sys/queue.h` | BSD queue macros |
| 17 | `SpdkShim/sys/x86intrin.h` | x86 intrinsics stubs |

## Summary

### File Counts

| Category | Count |
|----------|-------|
| Upstream SPDK `.c` files compiled | 14 |
| SpdkShim `.c` files compiled (replacing upstream) | 6 |
| SpdkShim `.c` files compiled (EDK2-only) | 12 |
| **Total compiled `.c` files** | **32** |
| Upstream `spdk/include/spdk/*.h` headers used | 28 |
| Upstream `spdk/include/spdk_internal/*.h` headers used | 2 |
| Upstream `spdk/lib/` internal headers used | 3 |
| SpdkShim headers overriding upstream | 6 |
| SpdkShim-only headers | 17 |
| **Total upstream files used (`.c` + `.h`)** | **47** |
| **Total SpdkShim files** | **41** |

### Upstream SPDK Files NOT Used

The upstream `spdk/` submodule contains hundreds of files. Only the files listed
above are used in the DxeSpdkLib build. The following upstream NVMe `.c` files
are **not** compiled because their shimmed replacements are used instead:

- `spdk/lib/nvme/nvme.c` (replaced by `SpdkShim/spdk/lib/nvme/nvme.c`)
- `spdk/lib/nvme/nvme_ctrlr.c` (replaced by `SpdkShim/spdk/lib/nvme/nvme_ctrlr.c`)
- `spdk/lib/nvme/nvme_fabric.c` (replaced by `SpdkShim/spdk/lib/nvme/nvme_fabric.c`)
- `spdk/lib/nvme/nvme_qpair.c` (replaced by `SpdkShim/spdk/lib/nvme/nvme_qpair.c`)
- `spdk/lib/nvme/nvme_transport.c` (replaced by `SpdkShim/spdk/lib/nvme/nvme_transport.c`)
- `spdk/lib/nvme/nvme_tcp.c` (replaced by `SpdkShim/spdk/lib/nvme/nvme_tcp.c`)

### Build Verification

Build verified successfully using:
```
source edksetup.sh
build -t GCC -a X64 -p OvmfPkg/OvmfPkgX64.dsc
```
All 32 `.c` files compile to `.obj` files under
`Build/OvmfX64/DEBUG_GCC/X64/NetworkPkg/Library/DxeSpdkLib/DxeSpdkLib/OUTPUT/`.
