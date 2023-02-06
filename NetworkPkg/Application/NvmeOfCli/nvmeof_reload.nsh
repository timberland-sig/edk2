drivers -sfo > drivers.txt
NvmeOfCli.efi dhfilter drivers.txt > unload.nsh
unload.nsh
rm unload.nsh
load NvmeOfDxe.efi
