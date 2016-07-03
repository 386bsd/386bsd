#!/bin/sh

# $XFree86: mit/server/ddx/x386/vga16/confvga16.sh,v 2.0 1993/08/19 16:08:02 dawes Exp $
#
# This script generates vga16Conf.c
#
# usage: configvga.sh driver1 driver2 ...
#

VGACONF=./vga16Conf.c

cat > $VGACONF <<EOF
/*
 * This file is generated automatically -- DO NOT EDIT
 */

#include "vga.h"

extern vgaVideoChipRec
EOF
Args="`echo $* | tr '[a-z]' '[A-Z]'`"
set - $Args
while [ $# -gt 1 ]; do
  echo "        $1," >> $VGACONF
  shift
done
echo "        $1;" >> $VGACONF
cat >> $VGACONF <<EOF

vgaVideoChipPtr vgaDrivers[] =
{
EOF
for i in $Args; do
  echo "        &$i," >> $VGACONF
done
echo "        NULL" >> $VGACONF
echo "};" >> $VGACONF
