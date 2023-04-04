
NVMeOF Driver
--------------

The NVMeOF Driver Dxe supports NVMeOF TCP protocol with nBFT specification 0.32.

It is required that the user sets attempt before loading the driver by using the NVMeOF CLI application. Attempt is a mechanism in which user specifices a configuration set which contains NVMeOF TCP specific parameter like the MacID, Host IP, Target IP, Target Port etc for specific instance and written to memory in form of UEFI volatile variable which is pickedup by the NVMeOF Driver in Supported and Start. The user has to enter a custom attempt configuration in a text based configuration file . Refer to "Sampleconfigs" directory in NVMeoFCLI Application source , it contains sample configurations like multiple attempts, IPv6, VLAN etc.


Windows:
--------

In EmulatorPkg.dsc enable NETWORK_NVMEOF_ENABLE by setting it to TRUE and perform the steps as mentioned in edk2 compilation and invoke the winhost.exe

After compilation copy CLI sample config file NetworkPkg/Application/NvmeOfCli/Config to Build\EmulatorX64\DEBUG_VS2017\X64.This file is for setting attempts which will be processed by NVMeOFDxe in its supported and start.

Now jump to steps for Invoking NVMeOF driver


Linux:
------
System requirement 

- Ubuntu 20.04 Desktop (i.e with Ubuntu GUI)

No changes required in dsc file as NETWORK_NVMEOF_ENABLE is enabled by default in OvmfX64Pkg for compilation.Perform the compilation steps as mentioned in edk2 compilation.
To use the driver and cli application it has to be attached to QEMU using qcow2 image.Refer to below section on how to setup QEMU Networking followed by create qcow2 image.

Invoking QEMU:
To invoke the QEMU use the following command in Linux GUI shell prompt (Gnome terminal/Konsole)


::

  cd <path to edk2>/Build/OvmfX64/DEBUG_GCC5/FV
  
  sudo qemu-system-x86_64 --bios OVMF.fd -enable-kvm -net nic -net bridge,br=virbr0 -netdev tap,id=tap0,ifname=tap0,script=no,downscript=no -device e1000,netdev=tap0 -net nic -net bridge,br=virbr0 -drive file=<path to qcow>/test.qcow2


Now jump to steps for Invoking NVMeOF driver


Setting QEMU Networking
------------------------

1. Install “sudo apt install qemu-kvm libvirt-daemon-system libvirt-clients bridge-utils”

2. Create a file folder using command “mkdir /etc/qemu”

3. Create “bridge.conf” inside etc/qemu.

::

4. write "allow virbr0"  

::

 echo "allow virbr0" > bridge.conf


5. Create tap device named "tap0" using below commands

::

 sudo ip tuntap add tap0 mode tap
 sudo ifconfig tap0 up
 sudo usermod -a -G kvm $USER


Creating qcow2 image with NVMeOF driver and CLI
------------------------------------------------

1.	Create a qcow2 image of 256 MB (from the linux host):

::

  qemu-img create -f qcow2 /path/to/the/image/test.qcow2 256M
	
2.	Now start NBD driver on the host and connect the qcow2 image as a network block device (from the linux host):

::

  modprobe nbd max_part=8
  qemu-nbd --connect=/dev/nbd0 /path/to/the/image/test.qcow2


3.	Create EFI partition and format:
Create GPT (from the linux host):

::

  gdisk
  /dev/nbd0
  w
  y

::

4.	Create EFI partition (from the linux host):

::

  gdisk
  /dev/nbd0
  n
  1
  <press Enter to accept default value>
  <press Enter to accept default value>
  EF00
  w
  y



5.	Format the EFI partition as FAT32 (from the linux host):

::

  mkfs.fat -F 32 /dev/nbd0p1


6.	Create a folder for the mount point (from the linux host): 

::

  mkdir /path/to/the/mount/point/


7.	Mount the EFI partition (from the linux host):


::

  mount /dev/nbd0p1 /path/to/the/mount/point/


8.	Create the mnt folder on the mounted partition (from the linux host):

::

  mkdir /path/to/the/mount/point/mnt/


9.	Copy the Driver, CLI and config file in /path/to/the/mount/point/mnt/ (from the linux host):
At edk2 level 

::

  cp Build/OvmfX64/DEBUG_GCC5/X64/NvmeOfDxe.efi /path/to/the/mount/point/mnt/
  cp Build/OvmfX64/DEBUG_GCC5/X64/NvmeOfCli.efi /path/to/the/mount/point/mnt/

Now Open Config file in an editor and change the parameters as required and then copy it to the mount point

::

  cp NetworkPkg/Application/NvmeOfCli/Config /path/to/the/mount/point/mnt/



10.	Unmount the EFI partition (from the linux host):

::

  umount /path/to/the/mount/point/


11.	Disconnect nbd0 (from the linux host):

::

  qemu-nbd --disconnect /dev/nbd0
  
  

Invoking NVMeOF Driver
----------------------

These steps are common to Windows (Emulator) and Linux (Qemu).

- set IP to network adapter (for static ,use a free IP closer to host IP series)

::

 ifconfig -s eth0 static 192.168.100.10 255.255.252.0 192.168.100.1

- Invoke NVMeOF CLI to set attempt UEFI variable based on configuration file

::

 NvmeOfCli.efi setattempt Config
 reconnect -r
 map -r


The NVMeOF block device/FS devices will be visible in output of 'map -r'


Troubleshooting
---------------

1. Unable to see any NVMeOF TCP device in map -r

- Check the attempt configuration file if the NVMEOF TCP parameters are given correctly and there is a CRLF/LF after $end tag

- Check if the target is pingable from UEFI Shell and no packet drops are observed . If unable to ping , check if the host is pingable

- In case of QEMU (Linux) try to setup the target in the host machine itself. Manually run discovery command on target using Linux NVMe cli commands to verify if its rechable from host

2. Able to see Block device in 'map -r' but no FS object is created

   Check if the target file system is not corrupted




