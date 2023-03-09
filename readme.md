# UEFI NVMe-oF Boot Implementation
## Introduction
This UEFI Nvme-oF Boot Implementation enables a Boot-from-SAN prototype that produces and consumes [ACPI](https://uefi.org/specifications) XSDT [NBFT](https://uefi.org/specs/ACPI/6.5/05_ACPI_Software_Programming_Model.html?highlight=nbft#description-header-signatures-for-tables-reserved-by-acpi) descriptors that are conformant to the [NVM Express Boot Specification 1.0](https://nvmexpress.org/specification/nvme-boot-specification/) and it’s interface with the [NVMe-oF messaging type](https://uefi.org/specs/UEFI/2.10/10_Protocols_Device_Path_Protocol.html#nvme-over-fabric-nvme-of-namespace-device-path) to [UEFI System Specification 2.10](https://uefi.org/specs/UEFI/2.10/).
 
Further implementation details relative to the UEFI NvmeOfDxe driver can be found in the [POC Readme](https://github.com/timberland-sig/edk2/blob/timberland_1.0_final/NetworkPkg/NvmeOfDxe/readme.md).
 
## Known Open Issues and Errata
 
- IPv6 still has general config and interop issues
- Hii Currently Missing; currently configuration is accomplished using NetworkPkg/Applications/NvmeOfCli.efi which may use a config file via efi-shell
- [ ] OVMF/QEMU: Live Network changes can cause hangs in NVMe-oF driver
- Sporadically, issues in the ExitBootServices code path have been observed
 
## Points of Contact
 
* Doug Farley \<Douglas_Farley@dell.com\>
* Richelle Ahlvers \<richelle.ahlvers@intel.com\>
 
## Contributors
 
Current and past key contributors:
 
* Naoyuki Mori \<naoyuki.mori@intel.com\>
* Amit Jain \<amit_jain9@dell.com\>
* Hrishikesh Gokhale \<Hrishikesh_Gokhale@dell.com\>
* Maciej Rabęda \<maciej.rabeda@intel.com\>
* Swamy Kadaba \<Swamy_Kadaba@Dell.com\>
* Murali Manohar Shanm \<Murali_Manohar_Shanm@Dell.com\>
* Doug Farley \<Douglas_Farley@dell.com\>
* Wei G Liu \<Wei_G_Liu@Dell.com\>
* Martin Wilck \<mwilck@suse.com\>
* Lenny Szubowicz \<lszubowi@redhat.com\>
* Peter Liu \<Peter_Liu@Dell.com\>
* Tony Rodriguez
