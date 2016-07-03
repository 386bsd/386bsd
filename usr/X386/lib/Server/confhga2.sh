#!/bin/sh

# $XFree86: mit/server/ddx/x386/hga2/confhga2.sh,v 1.2 1993/03/27 09:32:03 dawes Exp $
#
# This script generates hga2Conf.c
#
# usage: confighga.sh driver1 driver2 ...
#

HGACONF=./hga2Conf.c

cat > $HGACONF <<EOF
/*
 * This file is generated automatically -- DO NOT EDIT
 */

#include "hga.h"

extern hgaVideoChipRec
EOF
Args="`echo $* | tr '[a-z]' '[A-Z]'`"
set - $Args
while [ $# -gt 1 ]; do
  echo "        $1," >> $HGACONF
  shift
done
echo "        $1;" >> $HGACONF
cat >> $HGACONF <<EOF

hgaVideoChipPtr hgaDrivers[] =
{
EOF
for i in $Args; do
  echo "        &$i," >> $HGACONF
done
echo "        NULL" >> $HGACONF
echo "};" >> $HGACONF
