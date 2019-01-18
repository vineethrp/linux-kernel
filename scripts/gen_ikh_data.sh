#!/bin/bash
# SPDX-License-Identifier: GPL-2.0

spath="$(dirname "$(readlink -f "$0")")"

rm -rf $1.tmp
mkdir $1.tmp

for f in "${@:2}";
	do find "$f" ! -name "*.c" ! -name "*.o" ! -name "*.cmd" ! -name ".*";
done | cpio -pd $1.tmp

for f in $(find $1.tmp); do
	$spath/strip-comments.pl $f
done

tar -Jcf $1 -C $1.tmp/ . > /dev/null

rm -rf $1.tmp
