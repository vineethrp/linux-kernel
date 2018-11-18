#!/bin/bash
# scripts copies the bare minimum files to do out of tree kernel
# build. Files are in hplist. Also copies a test module.

# tool/objtool/objtool dependency in hplist can be made
# conditional on CONFIG_STACK_VALIDATION

rm -rf /tmp/ttt/* && for f in $(cat hplist); do find "$f" ! -name "*.c" ! -name "*.o" ! -name "*.cmd" ! -name ".*"; done | cpio -pd /tmp/ttt/

du -s /tmp/ttt/

# Also build a test module
rm -rf /tmp/testmod/
cp -r testmod /tmp/

cd /tmp/testmod
make clean
make

du -s /tmp/ttt/

tar -jcvf /tmp/ttt.tgz /tmp/ttt > /dev/null
echo "size of compressed kernel artifacts: "
du -sh /tmp/ttt.tgz
