#!/bin/bash -x
# scripts copies the bare minimum files to do out of tree kernel
# build. Files are in hplist. Also copies a test module.

export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-

rm -rf /tmp/ttt/* && for f in $(cat hplist-arm64); do find $f; done | cpio -pd /tmp/ttt/

rm -rf /tmp/ttt/scripts/
find scripts -executable  | cpio -pd /tmp/ttt/
find scripts -name "Makefile*" | cpio -pd /tmp/ttt/
find scripts -name "Kbuild*" | cpio -pd /tmp/ttt/
find scripts -name "subarch*" | cpio -pd /tmp/ttt/
find scripts -name "*lds" | cpio -pd /tmp/ttt/

# Also build a test module
rm -rf /tmp/testmod/
cp -r testmod /tmp/

cd /tmp/testmod
make clean
make
