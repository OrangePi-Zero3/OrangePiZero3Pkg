cd BootShim
make UEFI_BASE=0x40080000 UEFI_SIZE=0x00200000
rm BootShim.elf
cd ..
