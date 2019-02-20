#!/bin/bash
set -x
# SPDX-License-Identifier: GPL-2.0

spath="$(dirname "$(readlink -f "$0")")"
tarfile=$1

# Can we skip generation?
files_md5="$(find ${@:2} -type f ! -name modules.order | xargs ls -lR | md5sum | cut -d ' ' -f1)"
if [ -f $tarfile ]; then tarfile_md5="$(md5sum $tarfile | cut -d ' ' -f1)"; fi
if [ -f ikheaders.md5 ] &&
	[ "$(cat ikheaders.md5|head -1)" == "$files_md5" ] &&
	[ "$(cat ikheaders.md5|tail -1)" == "$tarfile_md5" ]; then
		exit
fi

rm -rf $tarfile.tmp
mkdir $tarfile.tmp

for f in "${@:2}";
	do find "$f" ! -name "*.c" ! -name "*.o" ! -name "*.cmd" ! -name ".*";
done | cpio -pd $tarfile.tmp

find  $tarfile.tmp -type f -print0 | xargs -0 -P4 -n1 -I {} sh -c "$spath/strip-comments.pl {}"

mkdir /tmp/tt
cp -r $tarfile.tmp/ /tmp/tt/

tar -Jcf $tarfile -C $tarfile.tmp/ . > /dev/null

echo "$files_md5" > ikheaders.md5
echo "$(md5sum $tarfile | cut -d ' ' -f1)" >> ikheaders.md5

rm -rf $tarfile.tmp
