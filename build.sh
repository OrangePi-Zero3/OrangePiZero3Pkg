#!/bin/bash
# based on the instructions from edk2-platform
set -e
. build_common.sh
# not actually GCC5; it's GCC7 on Ubuntu 18.04.
GCC5_AARCH64_PREFIX=aarch64-linux-gnu- build -j$(nproc) -s -n 0 -a AARCH64 -t GCC5 -p OrangePiZero3Pkg/OrangePiZero3Pkg.dsc
cp workspace/Build/OrangePiZero3Pkg/DEBUG_GCC5/FV/ORANGEPIZERO3PKG_UEFI.fd workspace/UEFI
