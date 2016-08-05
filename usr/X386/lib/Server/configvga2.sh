#!/bin/sh

# $XFree86: mit/server/ddx/x386/vga2/configvga2.sh,v 1.3 1993/03/27 09:04:02 dawes Exp $
#
# This script generates vga2Conf.c
#
# usage: configvga.sh driver1 driver2 ...
#

VGACONF=./vga2Conf.c

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
