#!/bin/bash -x

./make-swap.sh
rm test.c*
wget 10.0.2.2:8000/test-idle-page-swapped.c
wget 10.0.2.2:8000/test-manual-set-swap.c
# wget 10.0.2.2:8000/test.c
gcc -o tips test-idle-page-swapped.c
gcc -o tmss test-manual-set-swap.c
./tips
./tmss

