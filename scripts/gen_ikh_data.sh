#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
set -e

spath="$(dirname "$(readlink -f "$0")")"
kroot="$spath/.."
outdir="$(pwd)"
tarfile=$1
cpio_dir=$outdir/$tarfile.tmp

file_list=${@:2}

src_file_list=""
for f in $file_list; do
	if [ ! -f "$kroot/$f" ] && [ ! -d "$kroot/$f" ]; then continue; fi
	src_file_list="$src_file_list $(echo $f | grep -v OBJDIR)"
done

obj_file_list=""
for f in $file_list; do
	f=$(echo $f | grep OBJDIR | sed -e 's/OBJDIR\///g')
	if [ ! -f $f ] && [ ! -d $f ]; then continue; fi
	obj_file_list="$obj_file_list $f";
done

# Support incremental builds by skipping archive generation
# if timestamps of files being archived are not changed.

# This block is useful for debugging the incremental builds.
# Uncomment it for debugging.
# iter=1
# if [ ! -f /tmp/iter ]; then echo 1 > /tmp/iter;
# else; 	iter=$(($(cat /tmp/iter) + 1)); fi
# find $src_file_list -type f | xargs ls -lR > /tmp/src-ls-$iter
# find $obj_file_list -type f | xargs ls -lR > /tmp/obj-ls-$iter

# modules.order and include/generated/compile.h are ignored because these are
# touched even when none of the source files changed. This causes pointless
# regeneration, so let us ignore them for md5 calculation.
pushd $kroot > /dev/null
src_files_md5="$(find $src_file_list -type f ! -name modules.order |
		grep -v "include/generated/compile.h"		   |
		xargs ls -lR | md5sum | cut -d ' ' -f1)"
popd > /dev/null
obj_files_md5="$(find $obj_file_list -type f ! -name modules.order |
		grep -v "include/generated/compile.h"		   |
		xargs ls -lR | md5sum | cut -d ' ' -f1)"

if [ -f $tarfile ]; then tarfile_md5="$(md5sum $tarfile | cut -d ' ' -f1)"; fi
if [ -f kernel/kheaders.md5 ] &&
	[ "$(cat kernel/kheaders.md5|head -1)" == "$src_files_md5" ] &&
	[ "$(cat kernel/kheaders.md5|head -2|tail -1)" == "$obj_files_md5" ] &&
	[ "$(cat kernel/kheaders.md5|tail -1)" == "$tarfile_md5" ]; then
		exit
fi

rm -rf $cpio_dir
mkdir $cpio_dir

pushd $kroot > /dev/null
for f in $src_file_list;
	do find "$f" ! -name "*.c" ! -name "*.o" ! -name "*.cmd" ! -name ".*";
done | cpio --quiet -pd $cpio_dir
popd > /dev/null

# The second CPIO can complain if files already exist which can
# happen with out of tree builds. Just silence CPIO for now.
for f in $obj_file_list;
	do find "$f" ! -name "*.c" ! -name "*.o" ! -name "*.cmd" ! -name ".*";
done | cpio --quiet -pd $cpio_dir >/dev/null 2>&1

find  $cpio_dir -type f -print0 |
	xargs -0 -P8 -n1 perl -pi -e 'BEGIN {undef $/;}; s/\/\*((?!SPDX).)*?\*\///smg;'

tar -Jcf $tarfile -C $cpio_dir/ . > /dev/null

echo "$src_files_md5" > kernel/kheaders.md5
echo "$obj_files_md5" >> kernel/kheaders.md5
echo "$(md5sum $tarfile | cut -d ' ' -f1)" >> kernel/kheaders.md5

rm -rf $cpio_dir
