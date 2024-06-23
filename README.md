Attempt to create a minimal EDK2 for the OrangePi Zero3.

## Status
Boots to UEFI Shell.

### Building
Tested on Ubuntu 22.04.

First, clone EDK2.

```
cd ..
git clone https://github.com/tianocore/edk2 --recursive
git clone https://github.com/tianocore/edk2-platforms.git
```

You should have all three directories side by side.

Next, install dependencies:

22.04:

```
sudo apt install build-essential uuid-dev iasl git nasm python3-distutils gcc-aarch64-linux-gnu
```

Also see [EDK2 website](https://github.com/tianocore/tianocore.github.io/wiki/Using-EDK-II-with-Native-GCC#Install_required_software_from_apt)

## Tutorials

First run ./firstrun.sh

Then, ./build.sh.

This should make a boot.tar image to be booted by U-Boot.

# Booting UEFI
Prerequisites:

- A 1.5GB OrangePi Zero3
- An SD Card that has had U-Boot Flashed (Personally i flash the OrangePi OS and add an extra FAT32 partition to the SD Card to store the UEFI on, which this guide will assume you will also do)
- UART access for U-Boot

in terms of loading this, its fairly simple, first place the UEFI on a spare FAT32 partition on the SD Card, then insert the SD Card into the OrangePi and turn it on.

On your computer interrupt the normal boot by pressing space when promted to do so by U-Boot.

then load the UEFI image into RAM by doing

```
fatload mmc 0:3 0x40080000 UEFI
```

finally, jump to the region in memory where the UEFI is loaded by

```
go 0x40080000
```

# Credits

SimpleFbDxe screen driver is from imbushuo's [Lumia950XLPkg](https://github.com/WOA-Project/Lumia950XLPkg).

Zhuowei for making edk2-pixel3

All the people in ``EDKII pain and misery, struggles and disappointment`` on Discord.
