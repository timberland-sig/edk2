# SPDK Files Used by EDK2

This document lists all files from the SPDK library (v26.01.x) that are
required to compile the EDK2 NVMe-oF TCP stack (`DxeSpdkLib`).

The SPDK submodule at `NetworkPkg/Library/DxeSpdkLib/spdk/` contains the full
upstream library (~206MB), but EDK2 only uses 55 files (~2MB). These files
have been copied into `NetworkPkg/Library/DxeSpdkLib/spdk_import/` so that
the build no longer depends on the submodule.

## Migration Summary

The EDK2 build references to the SPDK submodule were in two places:

1. **`NetworkPkg/Library/DxeSpdkLib/DxeSpdkLib.inf`** `[Sources]` section —
   14 source files changed from `spdk/lib/...` to `spdk_import/lib/...`

2. **`NetworkPkg/NetworkPkg.dec`** `[Includes.Common.Private]` section —
   2 include paths changed:
   - `Library/DxeSpdkLib/spdk/include/` → `Library/DxeSpdkLib/spdk_import/include/`
   - `Library/DxeSpdkLib/spdk/lib/nvme/` → `Library/DxeSpdkLib/spdk_import/lib/nvme/`

The SpdkShim files (`SpdkShim/spdk/lib/nvme/*.c` and `SpdkShim/spdk/*.h`)
are NOT affected — they are modified copies maintained separately, not part
of the submodule.

## Directory Structure

```
NetworkPkg/Library/DxeSpdkLib/spdk_import/
├── include/
│   ├── spdk/                 (33 public headers)
│   └── spdk_internal/        (4 internal headers)
└── lib/
    ├── nvme/                 (8 source files + 2 private headers)
    ├── sock/                 (1 source file)
    └── util/                 (5 source files + 2 private headers)
```

## File List (55 files)

### Source Files Compiled by EDK2 (14 files)

These are listed in `DxeSpdkLib.inf` `[Sources]` and compiled directly.

#### spdk_import/lib/util/ (5 files)

| File | Description |
|---|---|
| `crc16.c` | CRC-16 computation |
| `crc32.c` | CRC-32 (IEEE) computation |
| `crc32c.c` | CRC-32C (Castagnoli) computation |
| `bit_array.c` | Bit array utilities |
| `string.c` | SPDK string utilities |

#### spdk_import/lib/nvme/ (8 files)

| File | Description |
|---|---|
| `nvme_ctrlr_cmd.c` | NVMe controller admin commands |
| `nvme_ctrlr_ocssd_cmd.c` | Open-Channel SSD controller commands |
| `nvme_io_msg.c` | NVMe I/O message passing |
| `nvme_ns.c` | NVMe namespace operations |
| `nvme_ns_cmd.c` | NVMe namespace I/O commands |
| `nvme_ns_ocssd_cmd.c` | Open-Channel SSD namespace commands |
| `nvme_poll_group.c` | NVMe poll group management |
| `nvme_quirks.c` | NVMe device quirk detection |

#### spdk_import/lib/sock/ (1 file)

| File | Description |
|---|---|
| `sock.c` | SPDK socket abstraction layer |

### Public Headers (33 files)

Located in `spdk_import/include/spdk/`. These are included via
`#include "spdk/xxx.h"` by both the upstream source files and the SpdkShim.

| Header | Purpose |
|---|---|
| `assert.h` | SPDK assertion macros |
| `bit_array.h` | Bit array API |
| `bit_pool.h` | Bit pool API |
| `cpuset.h` | CPU set abstraction (via thread.h) |
| `crc16.h` | CRC-16 API |
| `crc32.h` | CRC-32 and CRC-32C API |
| `dif.h` | Data Integrity Field (T10-DIF) |
| `dma.h` | DMA memory management (via nvme.h) |
| `endian.h` | Endian conversion utilities |
| `env.h` | Environment abstraction (memory, PCI) |
| `fd.h` | File descriptor utilities (via util.h) |
| `fd_group.h` | File descriptor group/poll abstraction |
| `json.h` | JSON parser (via sock.h, keyring.h) |
| `keyring.h` | Key management for TLS PSK (via nvme.h) |
| `log.h` | Logging infrastructure |
| `memory.h` | Memory registration |
| `mmio.h` | Memory-mapped I/O |
| `nvme.h` | NVMe driver public API |
| `nvme_intel.h` | Intel NVMe extensions |
| `nvme_ocssd.h` | Open-Channel SSD API |
| `nvme_ocssd_spec.h` | Open-Channel SSD specification types |
| `nvme_spec.h` | NVMe specification types and constants |
| `nvmf_spec.h` | NVMe over Fabrics specification types |
| `pci_ids.h` | PCI vendor/device IDs |
| `queue.h` | BSD queue macros (TAILQ, STAILQ, etc.) |
| `queue_extras.h` | Additional queue macros |
| `sock.h` | Socket abstraction API |
| `string.h` | SPDK string utility API |
| `thread.h` | SPDK thread abstraction |
| `trace.h` | Tracing infrastructure |
| `tree.h` | Red-black tree macros |
| `util.h` | General utility macros and functions |
| `uuid.h` | UUID type and operations |

### Internal Headers (4 files)

Located in `spdk_import/include/spdk_internal/`. These are included via
`#include "spdk_internal/xxx.h"`.

| Header | Purpose |
|---|---|
| `assert.h` | Internal assertion helpers |
| `sgl.h` | Scatter-gather list helpers |
| `sock_module.h` | Socket module registration API |
| `trace_defs.h` | Trace point definitions |

### Library-Private Headers (4 files)

These headers are in the same directory as the source files and are found
via the `-I spdk_import/lib/nvme/` or relative include paths.

#### spdk_import/lib/nvme/ (2 files)

| Header | Purpose |
|---|---|
| `nvme_internal.h` | NVMe driver internal structures and functions |
| `nvme_io_msg.h` | I/O message internal definitions |

#### spdk_import/lib/util/ (2 files)

| Header | Purpose |
|---|---|
| `crc_internal.h` | CRC computation internals |
| `util_internal.h` | Utility internals |

## Headers NOT Copied (Provided by SpdkShim)

These upstream headers are overridden by the SpdkShim layer and are NOT
included from `spdk_import/`. The shim versions adapt the SPDK headers
for the UEFI build environment.

| Upstream Header | Shim Override |
|---|---|
| `spdk/barrier.h` | `SpdkShim/spdk/barrier.h` |
| `spdk/likely.h` | `SpdkShim/spdk/likely.h` |
| `spdk/stdinc.h` | `SpdkShim/spdk/stdinc.h` |
| `spdk/config.h` | `SpdkShim/spdk/config.h` (no upstream equivalent) |
| `spdk_internal/nvme_tcp.h` | `SpdkShim/spdk/spdk_internal/nvme_tcp.h` |
| `spdk_internal/sock.h` | `SpdkShim/spdk/spdk_internal/sock.h` |

## Include Path Resolution Order

The EDK2 build uses these include paths (from `NetworkPkg.dec`):

1. `Library/DxeSpdkLib/SpdkShim/` — shim headers (highest priority)
2. `Library/DxeSpdkLib/SpdkShim/sys/` — POSIX system header stubs
3. `Library/DxeSpdkLib/SpdkShim/spdk/` — shim overrides for SPDK headers
4. `Library/DxeSpdkLib/spdk_import/include/` — upstream SPDK public headers
5. `Library/DxeSpdkLib/spdk_import/lib/nvme/` — NVMe internal headers

The shim directories (1-3) are searched before `spdk_import/` (4-5), so
shim overrides take effect for headers like `barrier.h`, `likely.h`,
`stdinc.h`, `config.h`, `nvme_tcp.h`, and `sock.h`.

## SpdkShim Source Files (Not Part of spdk_import)

For completeness, these are the modified copies and EDK2-specific files
maintained in `SpdkShim/` — they are NOT from the submodule:

### Modified SPDK Copies (in SpdkShim/spdk/lib/nvme/)
- `nvme.c` — completion wait functions, ctrlr lookup
- `nvme_ctrlr.c` — controller lifecycle, admin commands
- `nvme_fabric.c` — NVMe-oF fabric connect/discovery
- `nvme_qpair.c` — queue pair management
- `nvme_tcp.c` — TCP transport
- `nvme_transport.c` — transport abstraction

### EDK2 Integration Files (in SpdkShim/)
- `edk_nvme.c`, `edk_nvme_ctrlr.c`, `edk_nvme_fabric.c` — EDK2 NVMe API
- `edk_sock.c` — EDK2 socket implementation
- `edk_log.c` — EDK2 logging bridge
- `sys_types.c`, `std_string.c` — POSIX stub implementations
- `env.c` — SPDK environment abstraction for UEFI
- `dif_shim.c` — DIF shim
- `uuid.c` — UUID implementation
- `json_write.c` — JSON writer
- `pthread_shim.c` — pthread stub
