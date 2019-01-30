#!/bin/bash
# scripts copies the bare minimum files to do out of tree kernel
# build. Files are in hplist. Also copies a test module.

# tool/objtool/objtool dependency in hplist can be made
# conditional on CONFIG_STACK_VALIDATION

rm -rf /tmp/ttt/* && for f in $(cat hplist); do find "$f" ! -name "*.c" ! -name "*.o" ! -name "*.cmd" ! -name ".*"; done | cpio -pd /tmp/ttt/

tar -Jcvf /tmp/ttt.tgz /tmp/ttt > /dev/null
echo "size of compressed kernel artifacts without header strip: "
du -sh /tmp/ttt.tgz

for f in $(find /tmp/ttt -type f); do
	./replace.pl $f
done

tar -Jcvf /tmp/ttt.tgz /tmp/ttt > /dev/null
echo "size of compressed kernel artifacts WITH header strip: "
du -sh /tmp/ttt.tgz

# Also build a test module
rm -rf /tmp/testmod/
cp -r testmod /tmp/

cd /tmp/testmod
make clean
make
