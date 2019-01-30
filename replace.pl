#!/usr/bin/perl -pi
# Usage:
# cp test-file.h.bak test-file.h
# ./replace.pl test-file.h
#
use strict;
use warnings;

BEGIN {undef $/;}

# Strip /* */ comments except if it contains SPDX
s/\/\*((?!SPDX).)*?\*\///smg;

# Stripping // style comments doesn't save much
# s/\/\/.*?[\r\n]//smg;
