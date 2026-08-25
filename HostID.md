## Branch Comparison: `timberland_upstream-dev-full_hostid-endian-fix_rebase` vs `rel/timberland_1_0_1`

Both branches diverge from a common ancestor (commit `b03a21a63e`, an upstream edk2 commit). They share the same base NVMe-over-Fabrics feature work but have diverged significantly.

### Shared Foundation (same logical changes, different commit hashes)
Both branches contain the core NVMe-oF upstreaming patches (NBFT table definitions, device path nodes, NvmeOfDxe driver, DxeSpdkLib, NvmeOfCli, IPv6 support, DHCPv6, configuration forms, Controller ID support, etc.) — but as **rebased/rewritten commits** with different SHAs.

### What `rel/timberland_1_0_1` has that the current branch does NOT (11 extra commits):
1. **SPDK shim rework cycle** — a disable/rework/re-enable sequence:
   - `13999e6ec9` — Disable NVMeoF feature for rework
   - `89fe4e6e4c` — OvmfPkg: Disable NVMeoF feature for rework
   - `ae585e706e` — Update SPDK submodule
   - `517a619266` — **Rework for SPDK shim** (major refactor of DxeSpdkLib)
   - `afcd0af0c4` — Update NvmeOfDxe for DxeSpdkLib changes
   - `b20d5b1c1d` — Update NvmeOfCli for DxeSpdkLib changes
   - `8901918aa2` / `fe881024ed` — Re-enable NVMeoF feature after porting
2. **DHCP4 Option 61 support** (`801d30fc65`)
3. **NBFT population issue fixes** (`778bfd5be8`)
4. **Build fix**: force SPDK to compile with `-std=gnu11` (`83c1cd7078`)
5. **Cleanup**: remove unused `LineNo` variable from NvmeOfCli (`869390c473`)

### What the current branch has that `rel/timberland_1_0_1` does NOT (1 extra commit):
1. **`5b49eeb041` — HostID conversion from EFI_GUID** — this is the endian fix referenced in the branch name.

### Net effect of the diff (47 files, ~240 additions, ~16,500 deletions):
The current branch is **much leaner** — it removes ~16,500 lines because it does **not** include the SPDK shim rework. Specifically:
- **Deleted**: Entire copies of SPDK source files (`nvme.c`, `nvme_ctrlr.c`, `nvme_fabric.c`, `nvme_qpair.c`, `nvme_tcp.c`, `nvme_transport.c`, `nvme_tcp.h`, `sock.h`) and EDK2 shim wrappers (`edk_nvme.c`, `edk_nvme_ctrlr.c`, `edk_nvme_fabric.c`, `dif_shim.c`, etc.) that `rel/timberland_1_0_1` carries.
- **Different SPDK submodule pointer** — the `spdk` submodule points to a different commit.
- Various smaller differences in NvmeOfDxe source files (`NvmeOfNbft.c`, `NvmeOfDhcp.c`, `NvmeOfSpdk.c`, `NvmeOfConfig.c`, etc.) reflecting the pre-rework vs post-rework state.

### Summary
The current branch (`timberland_upstream-dev-full_hostid-endian-fix_rebase`) is essentially a **clean rebase of the original NVMe-oF patches plus the HostID endian fix**, without the SPDK shim rework or the DHCP4/NBFT fixes that landed on `rel/timberland_1_0_1`. The `rel/timberland_1_0_1` branch has gone through a full SPDK porting rework cycle and includes additional bug fixes, but lacks the HostID endian conversion patch.
