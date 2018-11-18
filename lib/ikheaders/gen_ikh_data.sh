#!/bin/bash

echo "${@:2}" > $1.list

rm -rf $1.tmp
mkdir $1.tmp

for f in "${@:2}";
	do find "$f" ! -name "*.c" ! -name "*.o" ! -name "*.cmd" ! -name ".*";
done | cpio -pd $1.tmp

tar -jcf $1 -C $1.tmp/ . > /dev/null
rm -rf $1.tmp
