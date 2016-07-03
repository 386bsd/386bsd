#!/bin/sh

# $XFree86: mit/fonts/lib/font/fontfile/configrend.sh,v 1.2 1993/03/27 08:58:56 dawes Exp $
#
# This script generates rendererConf.c
#
# usage: configrend.sh driver1 driver2 ...
#

RENDCONF=./rendererConf.c

cat > $RENDCONF <<EOF
/*
 * This file is generated automatically -- DO NOT EDIT
 */

FontFileRegisterFontFileFunctions ()
{
    BitmapRegisterFontFileFunctions ();
EOF
for i in $*; do
  echo "    ${i}RegisterFontFileFunctions ();" >> $RENDCONF
done
echo '}' >> $RENDCONF
