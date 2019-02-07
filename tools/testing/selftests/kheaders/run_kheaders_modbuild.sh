#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

HEADERS_XZ=/proc/kheaders.tar.xz
TMP_DIR_HEADERS=$(mktemp -d)
TMP_DIR_MODULE=$(mktemp -d)
SPATH="$(dirname "$(readlink -f "$0")")"

tar -xvf $HEADERS_XZ -C $TMP_DIR_HEADERS > /dev/null

cp -r $SPATH/testmod $TMP_DIR_MODULE/

pushd $TMP_DIR_MODULE/testmod > /dev/null
make -C $TMP_DIR_HEADERS M=$(pwd) modules
popd > /dev/null

rm -rf $TMP_DIR_HEADERS
rm -rf $TMP_DIR_MODULE
