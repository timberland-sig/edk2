/** @file
  Helper Functions to print command usage for NVMeOF CLI Application

  Copyright (c) 2020, Dell EMC All rights reserved
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/UefiLib.h>


/**
  Prints Usage when no specific sub command is given
**/
VOID
PrintUsageUtility ()
{
  Print (L"\nCopyright (c) 2020, Dell EMC All rights reserved");
  Print (L"\nNvmeOfCli.efi is a utility for diagnostics of NVMeOF connection\n");
  Print (L"The utility works primarily in 2 modes, Passthrough protocol and BlockIO\n\n");

  Print (L"Block mode commands:\n");
  Print (L"listattempt    Lists all namespace created on system with BlockIO\n");
  Print (L"readwritesync  Perform Read-Write Synchronous test on a NVMeOF Block device\n");
  Print (L"readwriteasync Perform Read-Write Asynchronous test on a NVMeOF Block device\n\n");

  Print (L"Passthrough mode commands:\n");
  Print (L"list           Lists all namespace connected using Passthrough protocol\n");
  Print (L"read           Perform Read operation on devices\n");
  Print (L"write          Perform Write operation on devices\n");
  Print (L"reset          Resets the controller\n");
  Print (L"connect        Connect to a NVMeOF controller\n");
  Print (L"disconnect     Disconnect NVMeOF controller\n");
  Print (L"identify       Identify command for device\n\n");

  Print (L"Other commands:\n");
  Print (L"setattempt     Utility to set Attempt UEFI variable for NVMeOF Driver to-\n");
  Print (L"               consume\n");
  Print (L"version        Shows the program version\n\n");
  Print (L"To get individual command help type 'NvmeOfCli.efi help <command name>'\n");
}

/**
  Prints Usage for list command
**/
VOID
PrintUsageList ()
{
  Print (L"\nLists all namespace connected using Passthrough protocol\n");
  Print (L"NvmeOfCli.efi list\n");
}

/**
  Prints Usage for listattempt command
**/
VOID
PrintUsageListConnect ()
{
  Print (L"\nLists all namespace created on system with BlockIO\n");
  Print (L"NvmeOfCli.efi listattempt\n");
}

/**
  Prints Usage for Read command
**/
VOID
PrintUsageRead ()
{
  Print (L"\nPerform Read operation on devices created with Passthrough protocol\n");
  Print (L"NvmeOfCli.efi read <device>\n");
  Print (L"[--start-block=<slba> | -s <slba>]\n");
  Print (L"[--block-count=<nlb> | -c <nlb>]\n");
  Print (L"[--data=<data-file> | -d <data-file>]\n");
  Print (L"eg: NvmeOfCli.efi read nvme1n1 --start-block 0 --block-count 20 --data <readfilename>\n");
}

/**
  Prints Usage for Write command
**/
VOID
PrintUsageWrite ()
{
  Print (L"\nPerform Write operation on devices created with Passthrough protocol\n");
  Print (L"NvmeOfCli.efi write <device>\n");
  Print (L"[--start-block=<size> | -s <size>]\n");
  Print (L"[--data=<data-file> | -d <data-file>]\n");
  Print (L"eg: NvmeOfCli.efi write nvme1n1 --start-block 0 --data <inputfilename>\n");
}

/**
  Prints Usage for Reset command
**/
VOID
PrintUsageReset ()
{
  Print (L"NvmeOfCli.efi reset <device>\n");
  Print (L"eg: NvmeOfCli.efi reset nvme1n1\n");
}

/**
  Prints Usage for Connect command
**/
VOID
PrintUsageConnect ()
{
  Print (L"\n Connect to a NVMeOF controller using Passthrough protocol\n");
  Print (L"NvmeOfCli.efi connect\n");
  Print (L"[--transport <trtype>     | -t <trtype>] optional param defaults to tcp\n");
  Print (L"[--nqn <subnqn>           | -n <subnqn>]\n");
  Print (L"[--traddr <traddr>        | -a <traddr>]\n");
  Print (L"[--trsvcid <trsvcid>      | -s <trsvcid>]\n");
  Print (L"[--hostnqn <hostnqn>      | -q <hostnqn>] optional param\n");
  Print (L"--mac <mac address>\n");
  Print (L"--ipmode <0 for IPv4 and 1 for IPv6>\n");
  Print (L"--localip\n");
  Print (L"--subnetmask\n");
  Print (L"--gateway\n\n");
  Print (L"IPv4 eg: NvmeOfCli.efi connect -n nvmet-test -t tcp -a 192.168.100.29 -s\n");
  Print (L"4420 --mac 1A:2B:3C:4D:5E:6F --ipmode 0 --localip 192.168.100.11\n");
  Print (L"--subnetmask 255.255.252.0 --gateway 192.168.100.1\n\n");
  Print (L"IPv6 eg: NvmeOfCli.efi connect -n nvmet-test-v6_5000 -t tcp\n");
  Print (L"-a 6000::10 -s 4425 --mac 1A:2B:3C:4D:5E:6F --ipmode 1 --localip 6000::2\n");
  Print (L"--subnetmask 6000::2/64 --gateway 6000::10\n");
}

/**
  Prints Usage for Disconnect command
**/
VOID
PrintUsageDisconnect ()
{
  Print (L"\nDisconnect NVMeOF controller using Passthrough protocol\n");
  Print (L"NvmeOfCli.efi disconnect <device>\n");
  Print (L"eg: NvmeOfCli.efi disconnect nvme1n1\n");
}

/**
   Prints Usage for SetAttempt command
 **/
VOID
PrintUsageSetAttempt ()
{
  Print (L"\nUtility to set Attempt UEFI variable for NVMeOF Driver to consume\n");
  Print (L"NvmeOfCli.efi setattempt <config filename>\n");
  Print (L"Use blank(zero bytes) config file to set zero attempt\n");
}

/**
  Prints Usage for Identify command
**/
VOID
PrintUsageIdentify ()
{
  Print (L"\nIdentify command for device created using Passthrough protocol\n");
  Print (L"NvmeOfCli.efi identify <device>\n");
  Print (L"eg: NvmeOfCli.efi identify nvme1n1\n");
}

/**
  Prints Usage for Version command
**/
VOID
PrintUsageVersion ()
{
  Print (L"NvmeOfCli.efi version\n");
}

/**
  Prints Usage for ReadWrite sync command command
**/
VOID
PrintUsageReadWriteSync ()
{
  Print (L"\nPerform Read-Write Synchronous test on a NVMeOF Block device\n");
  Print (L"NvmeOfCli.efi readwritesync \n");
  Print (L"[--device-id <device> | -d <device>]\n");
  Print (L"[--start-lba <nlb> | -s <nlb>]\n");
  Print (L"[--numberof-block <nlb> | -n <nlb>]\n");
  Print (L"[--pattern=<hexnumber> | -p <hexnumber>], this is optional\n");
  Print (L"eg: nvmeOfcli.efi readwritesync -d BLK0 -s 2 -n 2 -p 0xfa \n");
}

/**
  Prints Usage for Read Write Async command
**/
VOID
PrintUsageReadWriteAsync ()
{
  Print (L"\nPerform Read-Write Asynchronous test on a NVMeOF Block device\n");
  Print (L"NvmeOfCli.efi readwriteasync \n");
  Print (L"[--device-id <device> | -d <device>]\n");
  Print (L"[--start-lba <nlb> | -s <nlb>]\n");
  Print (L"[--numberof-block <nlb> | -n <nlb>]\n");
  Print (L"[--pattern=<hexnumber> | -p <hexnumber>]\n");
  Print (L"eg: nvmeOfcli.efi readwriteasync -d BLK0 -s 2 -n 2 -p 0xfa \n");
}

/**
  Prints list of all supported commands
**/
VOID
PrintUsageHelp ()
{
  Print (L"\nCopyright (c) 2020, Dell EMC All rights reserved");
  Print (L"\nNvmeOfCli.efi is a utility for diagnostics of NVMeOF connection\n");
  Print (L"The utility works primarily in 2 modes, Passthrough protocol and BlockIO\n\n");

  Print (L"Block mode commands:\n");
  Print (L"listattempt    Lists all namespace created on system with BlockIO\n");
  Print (L"readwritesync  Perform Read-Write Synchronous test on a NVMeOF Block device\n");
  Print (L"readwriteasync Perform Read-Write Asynchronous test on a NVMeOF Block device\n\n");

  Print (L"Passthrough mode commands:\n");
  Print (L"list           Lists all namespace connected using Passthrough protocol\n");
  Print (L"read           Perform Read operation on devices\n");
  Print (L"write          Perform Write operation on devices\n");
  Print (L"reset          Resets the controller\n");
  Print (L"connect        Connect to a NVMeOF controller\n");
  Print (L"disconnect     Disconnect NVMeOF controller\n");
  Print (L"identify       Identify command for device\n\n");

  Print (L"Other commands:\n");
  Print (L"setattempt     Utility to set Attempt UEFI variable for NVMeOF Driver to-\n");
  Print (L"               consume\n");
  Print (L"version        Shows the program version\n\n");
  Print (L"To get individual command help type 'NvmeOfCli.efi help <command name>'\n");
}
