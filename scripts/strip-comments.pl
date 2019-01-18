#!/usr/bin/perl -pi
# SPDX-License-Identifier: GPL-2.0

# This script removes /**/ comments from a file, unless such comments
# contain "SPDX". It is used when building compressed in-kernel headers.

BEGIN {undef $/;}
s/\/\*((?!SPDX).)*?\*\///smg;
