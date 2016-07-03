#!/bin/sh

# $XFree86: mit/server/ddx/x386/accel/s3/confS3.sh,v 2.0 1993/09/04 16:21:44 dawes Exp $
#
# This script generates s3Conf.c
#
# usage: confS3.sh driver1 driver2 ...
#

S3CONF=./s3Conf.c

cat > $S3CONF <<EOF
/*
 * This file is generated automatically -- DO NOT EDIT
 */

#include "s3.h"

extern s3VideoChipRec
EOF
Args="`echo $* | tr '[a-z]' '[A-Z]'`"
set - $Args
while [ $# -gt 1 ]; do
  echo "        $1," >> $S3CONF
  shift
done
echo "        $1;" >> $S3CONF
cat >> $S3CONF <<EOF

s3VideoChipPtr s3Drivers[] =
{
EOF
for i in $Args; do
  echo "        &$i," >> $S3CONF
done
echo "        NULL" >> $S3CONF
echo "};" >> $S3CONF
