# SPDK Shim Update: v23.01.x to v26.01.x

This document describes the full series of patches applied to
`NetworkPkg/Library/DxeSpdkLib/SpdkShim/` and related files to reconcile
compile-time differences after updating the SPDK submodule from v23.01.x
to v26.01.x (commit 5e50e34aed).

**Critical constraint:** The files in `NetworkPkg/Library/DxeSpdkLib/SpdkShim`
define the EDK2 API. This API cannot change. The goal is to reconcile
differences between SPDK v23.01.x and v26.01.x while preserving EDK2 API
compatibility.

**Branch:** `timberland_1_0_1-john`
**Base:** `timberland_1.0_final`

---

## Patch 1: f5d824f8f9 — SpdkShim/edk_nvme: add hostnqn arg to nvme_get_ctrlr_by_trid_unsafe

**Files:** `SpdkShim/edk_nvme.c`

In v26.01.x, `nvme_get_ctrlr_by_trid_unsafe()` gained a `hostnqn` parameter.
Updated the call site in the EDK2 shim to pass the additional argument.

---

## Patch 2: f8b87929b1 — SpdkShim/edk_nvme_fabric: rename nvme_wait_for_completion to _poll

**Files:** `SpdkShim/edk_nvme_fabric.c`

In v26.01.x, `nvme_wait_for_completion()` was renamed to
`nvme_wait_for_completion_poll()`. Updated call sites in the EDK2 fabric
shim.

---

## Patch 3: a5ed3273f9 — SpdkShim/sock.h: replace is_zcopy with zcopy_idx and pending_zcopy

**Files:** `SpdkShim/spdk/spdk_internal/sock.h`

The `is_zcopy` boolean field in the sock structure was replaced with
`zcopy_idx` and `pending_zcopy` fields in v26.01.x to support the new
zero-copy tracking model.

---

## Patch 4: 9f6e6f840b — SpdkShim/nvme.c: add hostnqn to ctrlr_by_trid lookup functions

**Files:** `SpdkShim/spdk/lib/nvme/nvme.c`

Updated `nvme_get_ctrlr_by_trid()` and `nvme_get_ctrlr_by_trid_unsafe()` to
accept and pass through the new `hostnqn` parameter added in v26.01.x.

---

## Patch 5: c8c07ba487 — SpdkShim/nvme.c: replace psk array with tls_psk pointer field

**Files:** `SpdkShim/spdk/lib/nvme/nvme.c`

The `opts.psk` character array was replaced with `opts.tls_psk`, a
`struct spdk_key *` pointer in v26.01.x. Updated the shim's nvme.c to use
the new field name.

---

## Patch 6: ba37fb114f — SpdkShim/nvme_ctrlr.c: remove redefined NVME_CTRLR log macros

**Files:** `SpdkShim/spdk/lib/nvme/nvme_ctrlr.c`

Removed locally-defined `NVME_CTRLR_ERRLOG`, `NVME_CTRLR_DEBUGLOG`, and
`NVME_CTRLR_NOTICELOG` macros that conflicted with definitions now provided
by the upstream `nvme_internal.h` header in v26.01.x.

---

## Patch 7: fb145f6e78 — SpdkShim/nvme_ctrlr.c: replace psk array init with tls_psk pointer

**Files:** `SpdkShim/spdk/lib/nvme/nvme_ctrlr.c`

Updated PSK initialization in the controller code from `memcpy` into
`opts.psk[]` array to assignment of `opts.tls_psk` pointer, matching the
v26.01.x API change.

---

## Patch 8: ce33cf35be — SpdkShim/nvme_ctrlr.c: drop dnr arg from nvme_qpair_abort_all_queued_reqs

**Files:** `SpdkShim/spdk/lib/nvme/nvme_ctrlr.c`

In v26.01.x, the `dnr` (do not retry) parameter was moved from function
parameters to the `qpair->abort_dnr` field. Removed the extra `0` argument
from `nvme_qpair_abort_all_queued_reqs()` call sites.

---

## Patch 9: 719d612c22 — SpdkShim/nvme_ctrlr.c: use nvme_wait_for_adminq_completion

**Files:** `SpdkShim/spdk/lib/nvme/nvme_ctrlr.c`

Replaced calls to `nvme_wait_for_completion_robust_lock()` and
`nvme_wait_for_completion()` with the new v26.01.x function
`nvme_wait_for_adminq_completion(ctrlr, status, release)`.

The `release` parameter controls memory management:
- `release=true`: function frees status (if not timed_out); manual
  `free(status)` calls were removed to prevent double-free.
- `release=false`: caller must free; used when `status->cpl` fields are
  accessed after completion.

---

## Patch 10: 1c08b6873c — SpdkShim/nvme_ctrlr.c: add new controller state enum cases

**Files:** `SpdkShim/spdk/lib/nvme/nvme_ctrlr.c`

Added `case` entries for new controller state enum values introduced in
v26.01.x to switch statements, preventing `-Werror=switch` failures.

---

## Patch 11: 02a58013ff — SpdkShim/nvme_ctrlr.c: access ctratt bitfields through .bits member

**Files:** `SpdkShim/spdk/lib/nvme/nvme_ctrlr.c`

In v26.01.x, `spdk_nvme_cdata_ctratt` changed from a plain struct to a union
with a `.bits` member containing the bitfields. Updated
`ctrlr->cdata.ctratt.host_id_exhid_supported` to
`ctrlr->cdata.ctratt.bits.host_id_exhid_supported`.

---

## Patch 12: 3662380efc — SpdkShim/std_error.h: add ENOKEY error code for v26.01.x compat

**Files:** `SpdkShim/std_error.h`

Added `#define ENOKEY 126` (Key not available) after the existing `ETIMEDOUT`
definition. This error code is used by new TLS/PSK code paths in v26.01.x.

---

## Patch 13+14: 9281888cc6 — SpdkShim/nvme_ctrlr.c: update aer_completion type and abort_queued_reqs

**Files:** `SpdkShim/spdk/lib/nvme/nvme_ctrlr.c`

Two changes combined into one commit:
1. Renamed `spdk_nvme_ctrlr_aer_completion_list` to
   `spdk_nvme_ctrlr_aer_completion` (4 occurrences at lines 3751, 3774,
   3783, 4070) — struct renamed in v26.01.x.
2. Removed extra `0` argument from
   `nvme_qpair_abort_queued_reqs(ctrlr->adminq, 0)` at line 4617 — `dnr`
   parameter moved to `qpair->abort_dnr` field.

---

## Patch 15: d193fa0228 — SpdkShim/nvme_fabric.c: update wait functions and poll_status for v26.01.x

**Files:** `SpdkShim/spdk/lib/nvme/nvme_fabric.c`

Multiple related changes:
- `qpair->poll_status` renamed to `qpair->fabric_poll_status` (3 sites at
  lines 757, 772, 810).
- `nvme_wait_for_completion_robust_lock_timeout_poll(qpair, status, NULL)`
  replaced with `nvme_wait_for_completion_poll(qpair, status)` (line 774).
- `nvme_wait_for_completion_robust_lock(ctrlr->adminq, status,
  &ctrlr->ctrlr_lock)` replaced with
  `nvme_wait_for_adminq_completion(ctrlr, status, true)` in prop_set_cmd_sync
  (removed manual free(status)).
- `nvme_wait_for_completion_robust_lock(ctrlr->adminq, status,
  &ctrlr->ctrlr_lock)` replaced with
  `nvme_wait_for_adminq_completion(ctrlr, status, false)` in prop_get_cmd_sync
  (kept manual free since status->cpl is read after).
- `nvme_wait_for_completion(ctrlr->adminq, status)` replaced with
  `nvme_wait_for_adminq_completion(ctrlr, status, true)` in
  get_discovery_log_page (removed manual free).
- `nvme_wait_for_completion(discovery_ctrlr->adminq, status)` replaced with
  `nvme_wait_for_adminq_completion(discovery_ctrlr, status, true)` in
  fabric_ctrlr_scan (removed manual free).

---

## Patch 16: 6d4476cd73 — SpdkShim/nvme_qpair.c: remove dnr params and rename poll_status

**Files:** `SpdkShim/spdk/lib/nvme/nvme_qpair.c`

- `nvme_qpair_abort_queued_reqs` definition: removed `uint32_t dnr` param,
  changed body to use `qpair->abort_dnr` instead of `dnr`.
- `nvme_qpair_abort_all_queued_reqs` definition: removed `uint32_t dnr`
  param.
- Dropped extra `0` args from call sites (4 sites total).
- `qpair->poll_status = NULL` renamed to `qpair->fabric_poll_status = NULL`
  in nvme_qpair_init.

---

## Patch 17: d8a7211322 — SpdkShim/nvme_tcp.c: remove accel CRC32c, fix PSK, add epoll constants

**Files:** `SpdkShim/spdk/lib/nvme/nvme_tcp.c`, `SpdkShim/std_socket.h`

nvme_tcp.c changes:
- Removed `data_crc32_accel_done` callback function (19 lines).
- Removed `tcp_data_recv_crc32_done` callback function (40 lines).
- In `pdu_data_crc32_compute`: removed accel code path (lines 543-558),
  kept `tqpair` variable (still needed for `tqpair->flags.host_ddgst_enable`),
  removed unused `tgroup`. The CRC32c acceleration API changed from direct
  `submit_accel_crc32c` callback to sequence-based `append_crc32c` which
  requires infrastructure not available in UEFI.
- In `nvme_tcp_pdu_payload_handle`: removed accel code path (44 lines of
  accel + memcpy code), removed unused `tgroup` variable.
- `ctrlr->opts.psk[0] ? "ssl" : NULL` changed to
  `ctrlr->opts.tls_psk ? "ssl" : NULL`.
- Wrapped TLS impl_opts settings in `if (sock_impl_name)` block, removed
  `impl_opts.psk_key = ctrlr->opts.psk` and `impl_opts.psk_identity` lines.

std_socket.h additions:
- Added epoll constants before final `#endif`:
  `EPOLLIN 0x001`, `EPOLLOUT 0x004`, `EPOLLET (1U << 31)`.

---

## Patch 18: a607ea417f — SpdkShim/nvme_transport.c: update abort_reqs and remove optimal_poll_group

**Files:** `SpdkShim/spdk/lib/nvme/nvme_transport.c`

- `nvme_qpair_abort_all_queued_reqs(qpair, 0)` changed to
  `nvme_qpair_abort_all_queued_reqs(qpair)`.
- `nvme_transport_qpair_abort_reqs` definition: removed `uint32_t dnr` param,
  removed `assert(dnr <= 1)`, changed `qpair_abort_reqs(qpair, dnr)` to
  `qpair_abort_reqs(qpair, qpair->abort_dnr)` (2 sites).
- Removed entire `nvme_transport_qpair_get_optimal_poll_group()` function
  (12 lines) — deleted upstream in v26.01.x.

---

## Patch 19: 349cf74182 — SpdkShim: add POSIX stubs for upstream nvme_poll_group.c

**Files:** `SpdkShim/std_string.h`, `SpdkShim/sys_types.h`

std_string.h:
- Added `strerror()` function declaration.

sys_types.h:
- Added `EFD_NONBLOCK` and `EFD_CLOEXEC` constants.
- Added `eventfd()` stub function (returns -1).
- Added `write()` stub function (returns -1).
- Added `close()` stub function (returns 0).

These stubs are needed because upstream `nvme_poll_group.c` gained
Linux-specific API calls in v26.01.x that are not available in UEFI.

---

## (Non-shim) d74229bcfa — patch: turn on IPV6 and PXE in build_ovmf.sh

**Files:** `build_ovmf.sh`

Enabled IPV6 and PXE build flags in the OVMF build script.

---

## Patch 20: 4e0e71efb1 — SpdkShim: add POSIX socket stubs, new v26 NVMe functions, and fix ctratt

**Files:** `SpdkShim/std_socket.h`, `SpdkShim/sys_types.h`, `SpdkShim/sys_types.c`,
`SpdkShim/spdk/lib/nvme/nvme.c`, `SpdkShim/spdk/lib/nvme/nvme_qpair.c`,
`NetworkPkg/NvmeOfDxe/NvmeOfCliInterface.c`

### std_socket.h

Added POSIX socket constants needed by upstream `spdk/lib/sock/sock.c`:
- addrinfo flags: `AI_PASSIVE`, `AI_CANONNAME`, `AI_NUMERICHOST`,
  `AI_NUMERICSERV`
- Protocol family: `PF_UNSPEC`
- Protocol constants: `IPPROTO_TCP`, `IPPROTO_IPV6`
- TCP socket options: `TCP_NODELAY`, `TCP_USER_TIMEOUT`
- IPv6 socket options: `IPV6_V6ONLY`
- poll() infrastructure: `struct pollfd`, `nfds_t`, `POLLIN`, `POLLOUT`,
  `POLLERR`, `POLLHUP`, `POLLNVAL`

Added stub functions (all return -1, not functional in UEFI):
- `socket()`, `bind()`, `connect()`, `setsockopt()`, `getsockopt()`,
  `poll()`, `listen()`, `accept()`

### sys_types.h / sys_types.c

- Fixed `gai_strerror()` return type from `int` to `const char *` — upstream
  code uses the return value with `%s` format specifier (pointer/integer type
  mismatch error). Definition updated to return `""` instead of `0`.
- Added `#include "spdk/fd_group.h"` to sys_types.c.
- Added `spdk_fd_group_add_ext()` stub (returns -1) and
  `spdk_fd_group_remove()` stub — fd_group interrupt infrastructure used by
  `nvme_poll_group.c` is not available in UEFI.

### SpdkShim/spdk/lib/nvme/nvme.c

Added two new v26.01.x completion wait functions:

**`nvme_wait_for_completion_poll(qpair, status)`** — Replaces the old
`nvme_wait_for_completion_robust_lock_timeout_poll()` for callers. Polls the
qpair for a single completion round. Handles admin queue locking internally
via `nvme_ctrlr_lock()`/`nvme_ctrlr_unlock()` (replacing the external
robust_mutex parameter). Returns `-EAGAIN` if not yet done, `-EIO` on
completion error, `0` on success, `-ECANCELED` on timeout/transport error.

**`nvme_wait_for_adminq_completion(ctrlr, status, release)`** — New
high-level admin queue completion wait. Loops calling
`nvme_wait_for_completion_poll()` until done. Calculates timeout from
`ctrlr->opts.admin_timeout_ms`. When `release=true` and the command did not
time out, frees the status structure (callers must not call `free(status)`
themselves to avoid double-free).

### SpdkShim/spdk/lib/nvme/nvme_qpair.c

Added two new functions:

**`nvme_qpair_state_string(state)`** — Returns a human-readable string for
`enum nvme_qpair_state` values. Used in error/debug logging throughout
upstream `nvme_ns_cmd.c` and `nvme_tcp.c`.

**`spdk_nvme_qpair_get_fd(qpair, opts)`** — Stub returning -1. In upstream
SPDK this returns a file descriptor for the qpair (used for interrupt-driven
polling). Not applicable to UEFI where there are no POSIX file descriptors.

### NvmeOfCliInterface.c

Fixed `ReturnCdata->ctratt.host_id_exhid_supported` to
`ReturnCdata->ctratt.bits.host_id_exhid_supported` — same v26.01.x union
layout change as Patch 11, but in the NvmeOfDxe driver rather than the
SpdkShim.

---

## Summary of API Changes: SPDK v23.01.x to v26.01.x

### Renamed/Restructured Fields
| v23.01.x | v26.01.x |
|---|---|
| `ctratt.host_id_exhid_supported` | `ctratt.bits.host_id_exhid_supported` |
| `opts.psk` (char array) | `opts.tls_psk` (struct spdk_key * pointer) |
| `qpair->poll_status` | `qpair->fabric_poll_status` |
| `sock->is_zcopy` | `sock->zcopy_idx` + `sock->pending_zcopy` |
| `spdk_nvme_ctrlr_aer_completion_list` | `spdk_nvme_ctrlr_aer_completion` |

### Removed Parameters (dnr moved to qpair->abort_dnr)
| v23.01.x | v26.01.x |
|---|---|
| `nvme_qpair_abort_queued_reqs(qpair, dnr)` | `nvme_qpair_abort_queued_reqs(qpair)` |
| `nvme_qpair_abort_all_queued_reqs(qpair, dnr)` | `nvme_qpair_abort_all_queued_reqs(qpair)` |
| `nvme_transport_qpair_abort_reqs(qpair, dnr)` | `nvme_transport_qpair_abort_reqs(qpair)` |

### Replaced Wait Functions
| v23.01.x | v26.01.x |
|---|---|
| `nvme_wait_for_completion(qpair, status)` | `nvme_wait_for_completion_poll(qpair, status)` |
| `nvme_wait_for_completion_robust_lock(qpair, status, mutex)` | `nvme_wait_for_adminq_completion(ctrlr, status, release)` |
| `nvme_wait_for_completion_robust_lock_timeout_poll(qpair, status, mutex)` | `nvme_wait_for_completion_poll(qpair, status)` |

### New Functions Added to Shim
- `nvme_wait_for_completion_poll()`
- `nvme_wait_for_adminq_completion()`
- `nvme_qpair_state_string()`
- `spdk_nvme_qpair_get_fd()` (stub)
- `spdk_fd_group_add_ext()` (stub)
- `spdk_fd_group_remove()` (stub)

### Removed Functions
- `nvme_transport_qpair_get_optimal_poll_group()` — deleted upstream

### CRC32c Acceleration
The acceleration API changed from direct `submit_accel_crc32c` callback to
a sequence-based `append_crc32c` model. Since this requires SPDK acceleration
framework infrastructure not available in UEFI, the accel code paths were
removed entirely, keeping only the software CRC32c fallback.

### New POSIX Stubs for UEFI
The following POSIX functions/constants were added as stubs to allow upstream
SPDK files to compile in the EDK2 UEFI environment:
- Socket: `socket()`, `bind()`, `connect()`, `setsockopt()`, `getsockopt()`,
  `listen()`, `accept()`, `poll()`
- System: `eventfd()`, `write()`, `close()`, `strerror()`
- Constants: `AI_*`, `PF_UNSPEC`, `IPPROTO_*`, `TCP_*`, `IPV6_V6ONLY`,
  `POLL*`, `EPOLL*`, `EFD_*`, `ENOKEY`
