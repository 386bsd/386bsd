#!/bin/sh
# Guess values for system-dependent variables and create Makefiles.
# Generated automatically using autoconf.
# Copyright (C) 1991, 1992, 1993 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

# Usage: configure [--srcdir=DIR] [--host=HOST] [--gas] [--nfp] [--no-create]
#        [--prefix=PREFIX] [--exec-prefix=PREFIX] [--with-PACKAGE] [TARGET]
# Ignores all args except --srcdir, --prefix, --exec-prefix, --no-create, and
# --with-PACKAGE unless this script has special code to handle it.


for arg
do
  # Handle --exec-prefix with a space before the argument.
  if test x$next_exec_prefix = xyes; then exec_prefix=$arg; next_exec_prefix=
  # Handle --host with a space before the argument.
  elif test x$next_host = xyes; then next_host=
  # Handle --prefix with a space before the argument.
  elif test x$next_prefix = xyes; then prefix=$arg; next_prefix=
  # Handle --srcdir with a space before the argument.
  elif test x$next_srcdir = xyes; then srcdir=$arg; next_srcdir=
  else
    case $arg in
     # For backward compatibility, also recognize exact --exec_prefix.
     -exec-prefix=* | --exec_prefix=* | --exec-prefix=* | --exec-prefi=* | --exec-pref=* | --exec-pre=* | --exec-pr=* | --exec-p=* | --exec-=* | --exec=* | --exe=* | --ex=* | --e=*)
	exec_prefix=`echo $arg | sed 's/[-a-z_]*=//'` ;;
     -exec-prefix | --exec_prefix | --exec-prefix | --exec-prefi | --exec-pref | --exec-pre | --exec-pr | --exec-p | --exec- | --exec | --exe | --ex | --e)
	next_exec_prefix=yes ;;

     -gas | --gas | --ga | --g) ;;

     -host=* | --host=* | --hos=* | --ho=* | --h=*) ;;
     -host | --host | --hos | --ho | --h)
	next_host=yes ;;

     -nfp | --nfp | --nf) ;;

     -no-create | --no-create | --no-creat | --no-crea | --no-cre | --no-cr | --no-c | --no- | --no)
        no_create=1 ;;

     -prefix=* | --prefix=* | --prefi=* | --pref=* | --pre=* | --pr=* | --p=*)
	prefix=`echo $arg | sed 's/[-a-z_]*=//'` ;;
     -prefix | --prefix | --prefi | --pref | --pre | --pr | --p)
	next_prefix=yes ;;

     -srcdir=* | --srcdir=* | --srcdi=* | --srcd=* | --src=* | --sr=* | --s=*)
	srcdir=`echo $arg | sed 's/[-a-z_]*=//'` ;;
     -srcdir | --srcdir | --srcdi | --srcd | --src | --sr | --s)
	next_srcdir=yes ;;

     -with-* | --with-*)
       package=`echo $arg|sed 's/-*with-//'`
       # Delete all the valid chars; see if any are left.
       if test -n "`echo $package|sed 's/[-a-zA-Z0-9_]*//g'`"; then
         echo "configure: $package: invalid package name" >&2; exit 1
       fi
       eval "with_`echo $package|sed s/-/_/g`=1" ;;

     *) ;;
    esac
  fi
done

trap 'rm -f conftest* core; exit 1' 1 3 15

rm -f conftest*
compile='${CC-cc} $CFLAGS $DEFS conftest.c -o conftest $LIBS >/dev/null 2>&1'

# A filename unique to this package, relative to the directory that
# configure is in, which we can look for to find out if srcdir is correct.
unique_file=tar.h

# Find the source files, if location was not specified.
if test -z "$srcdir"; then
  srcdirdefaulted=yes
  # Try the directory containing this script, then `..'.
  prog=$0
  confdir=`echo $prog|sed 's%/[^/][^/]*$%%'`
  test "X$confdir" = "X$prog" && confdir=.
  srcdir=$confdir
  if test ! -r $srcdir/$unique_file; then
    srcdir=..
  fi
fi
if test ! -r $srcdir/$unique_file; then
  if test x$srcdirdefaulted = xyes; then
    echo "configure: Can not find sources in \`${confdir}' or \`..'." 1>&2
  else
    echo "configure: Can not find sources in \`${srcdir}'." 1>&2
  fi
  exit 1
fi
# Preserve a srcdir of `.' to avoid automounter screwups with pwd.
# But we can't avoid them for `..', to make subdirectories work.
case $srcdir in
  .|/*|~*) ;;
  *) srcdir=`cd $srcdir; pwd` ;; # Make relative path absolute.
esac

PROGS="tar"
if test -z "$CC"; then
  echo checking for gcc
  saveifs="$IFS"; IFS="${IFS}:"
  for dir in $PATH; do
    test -z "$dir" && dir=.
    if test -f $dir/gcc; then
      CC="gcc"
      break
    fi
  done
  IFS="$saveifs"
fi
test -z "$CC" && CC="cc"

# Find out if we are using GNU C, under whatever name.
cat > conftest.c <<EOF
#ifdef __GNUC__
  yes
#endif
EOF
${CC-cc} -E conftest.c > conftest.out 2>&1
if egrep yes conftest.out >/dev/null 2>&1; then
  GCC=1 # For later tests.
fi
rm -f conftest*

echo checking how to run the C preprocessor
if test -z "$CPP"; then
  CPP='${CC-cc} -E'
  cat > conftest.c <<EOF
#include <stdio.h>
EOF
err=`eval "$CPP $DEFS conftest.c 2>&1 >/dev/null"`
if test -z "$err"; then
  :
else
  CPP=/lib/cpp
fi
rm -f conftest*
fi

if test -n "$GCC"; then
  echo checking whether -traditional is needed
  pattern="Autoconf.*'x'"
  prog='#include <sgtty.h>
Autoconf TIOCGETP'
  cat > conftest.c <<EOF
$prog
EOF
eval "$CPP $DEFS conftest.c > conftest.out 2>&1"
if egrep "$pattern" conftest.out >/dev/null 2>&1; then
  need_trad=1
fi
rm -f conftest*


  if test -z "$need_trad"; then
    prog='#include <termio.h>
Autoconf TCGETA'
    cat > conftest.c <<EOF
$prog
EOF
eval "$CPP $DEFS conftest.c > conftest.out 2>&1"
if egrep "$pattern" conftest.out >/dev/null 2>&1; then
  need_trad=1
fi
rm -f conftest*

  fi
  test -n "$need_trad" && CC="$CC -traditional"
fi

# Make sure to not get the incompatible SysV /etc/install and
# /usr/sbin/install, which might be in PATH before a BSD-like install,
# or the SunOS /usr/etc/install directory, or the AIX /bin/install,
# or the AFS install, which mishandles nonexistent args.  (Sigh.)
if test -z "$INSTALL"; then
  echo checking for install
  saveifs="$IFS"; IFS="${IFS}:"
  for dir in $PATH; do
    test -z "$dir" && dir=.
    case $dir in
    /etc|/usr/sbin|/usr/etc|/usr/afsws/bin) ;;
    *)
      if test -f $dir/install; then
	if grep dspmsg $dir/install >/dev/null 2>&1; then
	  : # AIX
	else
	  INSTALL="$dir/install -c"
	  INSTALL_PROGRAM='$(INSTALL)'
	  INSTALL_DATA='$(INSTALL) -m 644'
	  break
	fi
      fi
      ;;
    esac
  done
  IFS="$saveifs"
fi
INSTALL=${INSTALL-cp}
INSTALL_PROGRAM=${INSTALL_PROGRAM-'$(INSTALL)'}
INSTALL_DATA=${INSTALL_DATA-'$(INSTALL)'}

if test -z "$YACC"; then
  echo checking for bison
  saveifs="$IFS"; IFS="${IFS}:"
  for dir in $PATH; do
    test -z "$dir" && dir=.
    if test -f $dir/bison; then
      YACC="bison -y"
      break
    fi
  done
  IFS="$saveifs"
fi
test -z "$YACC" && YACC=""

if test -z "$YACC"; then
  echo checking for byacc
  saveifs="$IFS"; IFS="${IFS}:"
  for dir in $PATH; do
    test -z "$dir" && dir=.
    if test -f $dir/byacc; then
      YACC="byacc"
      break
    fi
  done
  IFS="$saveifs"
fi
test -z "$YACC" && YACC="yacc"


echo checking for AIX
cat > conftest.c <<EOF
#ifdef _AIX
  yes
#endif

EOF
eval "$CPP $DEFS conftest.c > conftest.out 2>&1"
if egrep "yes" conftest.out >/dev/null 2>&1; then
  DEFS="$DEFS -D_ALL_SOURCE=1"
fi
rm -f conftest*


echo checking for minix/config.h
cat > conftest.c <<EOF
#include <minix/config.h>
EOF
err=`eval "$CPP $DEFS conftest.c 2>&1 >/dev/null"`
if test -z "$err"; then
  MINIX=1
fi
rm -f conftest*

# The Minix shell can't assign to the same variable on the same line!
if test -n "$MINIX"; then
  DEFS="$DEFS -D_POSIX_SOURCE=1"
  DEFS="$DEFS -D_POSIX_1_SOURCE=2"
  DEFS="$DEFS -D_MINIX=1"
fi

echo checking for POSIXized ISC
if test -d /etc/conf/kconfig.d &&
  grep _POSIX_VERSION /usr/include/sys/unistd.h >/dev/null 2>&1
then
  ISC=1 # If later tests want to check for ISC.
  DEFS="$DEFS -D_POSIX_SOURCE=1"
  if test -n "$GCC"; then
    CC="$CC -posix"
  else
    CC="$CC -Xp"
  fi
fi

echo checking for return type of signal handlers
cat > conftest.c <<EOF
#include <sys/types.h>
#include <signal.h>
#ifdef signal
#undef signal
#endif
extern void (*signal ()) ();
main() { exit(0); } 
t() { int i; }
EOF
if eval $compile; then
  DEFS="$DEFS -DRETSIGTYPE=void"
else
  DEFS="$DEFS -DRETSIGTYPE=int"
fi
rm -f conftest*


echo checking for size_t in sys/types.h
echo '#include <sys/types.h>' > conftest.c
eval "$CPP $DEFS conftest.c > conftest.out 2>&1"
if egrep "size_t" conftest.out >/dev/null 2>&1; then
  :
else 
  DEFS="$DEFS -Dsize_t=unsigned"
fi
rm -f conftest*

echo checking for major, minor and makedev header
cat > conftest.c <<EOF
#include <sys/types.h>
main() { exit(0); } 
t() { return makedev(0, 0); }
EOF
if eval $compile; then
  makedev=1
fi
rm -f conftest*

if test -z "$makedev"; then
echo checking for sys/mkdev.h
cat > conftest.c <<EOF
#include <sys/mkdev.h>
EOF
err=`eval "$CPP $DEFS conftest.c 2>&1 >/dev/null"`
if test -z "$err"; then
  DEFS="$DEFS -DMAJOR_IN_MKDEV=1" makedev=1
fi
rm -f conftest*

fi
if test -z "$makedev"; then
echo checking for sys/sysmacros.h
cat > conftest.c <<EOF
#include <sys/sysmacros.h>
EOF
err=`eval "$CPP $DEFS conftest.c 2>&1 >/dev/null"`
if test -z "$err"; then
  DEFS="$DEFS -DMAJOR_IN_SYSMACROS=1"
fi
rm -f conftest*

fi

echo checking for directory library header
echo checking for dirent.h
cat > conftest.c <<EOF
#include <dirent.h>
EOF
err=`eval "$CPP $DEFS conftest.c 2>&1 >/dev/null"`
if test -z "$err"; then
  DEFS="$DEFS -DDIRENT=1" dirheader=dirent.h
fi
rm -f conftest*

if test -z "$dirheader"; then
echo checking for sys/ndir.h
cat > conftest.c <<EOF
#include <sys/ndir.h>
EOF
err=`eval "$CPP $DEFS conftest.c 2>&1 >/dev/null"`
if test -z "$err"; then
  DEFS="$DEFS -DSYSNDIR=1" dirheader=sys/ndir.h
fi
rm -f conftest*

fi
if test -z "$dirheader"; then
echo checking for sys/dir.h
cat > conftest.c <<EOF
#include <sys/dir.h>
EOF
err=`eval "$CPP $DEFS conftest.c 2>&1 >/dev/null"`
if test -z "$err"; then
  DEFS="$DEFS -DSYSDIR=1" dirheader=sys/dir.h
fi
rm -f conftest*

fi
if test -z "$dirheader"; then
echo checking for ndir.h
cat > conftest.c <<EOF
#include <ndir.h>
EOF
err=`eval "$CPP $DEFS conftest.c 2>&1 >/dev/null"`
if test -z "$err"; then
  DEFS="$DEFS -DNDIR=1" dirheader=ndir.h
fi
rm -f conftest*

fi

echo checking for closedir return value
cat > conftest.c <<EOF
#include <sys/types.h>
#include <$dirheader>
int closedir(); main() { exit(0); }
EOF
eval $compile
if test -s conftest && (./conftest; exit) 2>/dev/null; then
  :
else
  DEFS="$DEFS -DVOID_CLOSEDIR=1"
fi
rm -f conftest*

# The 3-argument open happens to go along with the O_* defines,
# which are easier to check for.
echo checking for fcntl.h
cat > conftest.c <<EOF
#include <fcntl.h>
EOF
err=`eval "$CPP $DEFS conftest.c 2>&1 >/dev/null"`
if test -z "$err"; then
  open_header=fcntl.h
else
  open_header=sys/file.h
fi
rm -f conftest*

echo checking for 3-argument open
cat > conftest.c <<EOF
#include <$open_header>
main() { exit(0); } 
t() { int x = O_RDONLY; }
EOF
if eval $compile; then
  :
else
  DEFS="$DEFS -DEMUL_OPEN3=1"
fi
rm -f conftest*

echo checking for remote tape and socket header files
echo checking for sys/mtio.h
cat > conftest.c <<EOF
#include <sys/mtio.h>
EOF
err=`eval "$CPP $DEFS conftest.c 2>&1 >/dev/null"`
if test -z "$err"; then
  DEFS="$DEFS -DHAVE_SYS_MTIO_H=1" have_mtio=1
fi
rm -f conftest*

if test -n "$have_mtio"; then
cat > conftest.c <<EOF
#include <sgtty.h>
#include <sys/socket.h>
EOF
err=`eval "$CPP $DEFS conftest.c 2>&1 >/dev/null"`
if test -z "$err"; then
  PROGS="$PROGS rmt"
fi
rm -f conftest*
fi

echo checking for remote shell
if test -f /usr/ucb/rsh || test -f /usr/bin/remsh || test -f /usr/bin/rsh ||
  test -f /usr/bsd/rsh || test -f /usr/bin/nsh; then
  RTAPELIB=rtapelib.o
else
  echo checking for netdb.h
cat > conftest.c <<EOF
#include <netdb.h>
EOF
err=`eval "$CPP $DEFS conftest.c 2>&1 >/dev/null"`
if test -z "$err"; then
  DEFS="$DEFS -DHAVE_NETDB_H=1" RTAPELIB=rtapelib.o
else
  DEFS="$DEFS -DNO_REMOTE=1"
fi
rm -f conftest*

fi

echo checking for ANSI C header files
cat > conftest.c <<EOF
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>
EOF
err=`eval "$CPP $DEFS conftest.c 2>&1 >/dev/null"`
if test -z "$err"; then
  # SunOS string.h does not declare mem*, contrary to ANSI.
echo '#include <string.h>' > conftest.c
eval "$CPP $DEFS conftest.c > conftest.out 2>&1"
if egrep "memchr" conftest.out >/dev/null 2>&1; then
  # SGI's /bin/cc from Irix-4.0.5 gets non-ANSI ctype macros unless using -ansi.
cat > conftest.c <<EOF
#include <ctype.h>
#define ISLOWER(c) ('a' <= (c) && (c) <= 'z')
#define TOUPPER(c) (ISLOWER(c) ? 'A' + ((c) - 'a') : (c))
#define XOR(e,f) (((e) && !(f)) || (!(e) && (f)))
int main () { int i; for (i = 0; i < 256; i++)
if (XOR (islower (i), ISLOWER (i)) || toupper (i) != TOUPPER (i)) exit(2);
exit (0); }

EOF
eval $compile
if test -s conftest && (./conftest; exit) 2>/dev/null; then
  DEFS="$DEFS -DSTDC_HEADERS=1"
fi
rm -f conftest*
fi
rm -f conftest*

fi
rm -f conftest*

echo checking for unistd.h
cat > conftest.c <<EOF
#include <unistd.h>
EOF
err=`eval "$CPP $DEFS conftest.c 2>&1 >/dev/null"`
if test -z "$err"; then
  DEFS="$DEFS -DHAVE_UNISTD_H=1"
fi
rm -f conftest*

echo checking for getgrgid declaration
echo '#include <grp.h>' > conftest.c
eval "$CPP $DEFS conftest.c > conftest.out 2>&1"
if egrep "getgrgid" conftest.out >/dev/null 2>&1; then
  DEFS="$DEFS -DHAVE_GETGRGID=1"
fi
rm -f conftest*

echo checking for getpwuid declaration
echo '#include <pwd.h>' > conftest.c
eval "$CPP $DEFS conftest.c > conftest.out 2>&1"
if egrep "getpwuid" conftest.out >/dev/null 2>&1; then
  DEFS="$DEFS -DHAVE_GETPWUID=1"
fi
rm -f conftest*

for hdr in string.h limits.h
do
trhdr=HAVE_`echo $hdr | tr '[a-z]./' '[A-Z]__'`
echo checking for ${hdr}
cat > conftest.c <<EOF
#include <${hdr}>
EOF
err=`eval "$CPP $DEFS conftest.c 2>&1 >/dev/null"`
if test -z "$err"; then
  DEFS="$DEFS -D${trhdr}=1"
fi
rm -f conftest*
done

echo checking default archive
# This might guess wrong, but it's not very important.
for dev in rmt8 rmt0 rmt0h rct0 rst0 tape rct/c7d0s2
do
  if test -n "`ls /dev/$dev 2>/dev/null`"; then
    DEF_AR_FILE=/dev/$dev
    break
  fi
done
if test -z "$DEF_AR_FILE"; then
  DEF_AR_FILE=-
fi

for func in strstr valloc mkdir mknod rename ftruncate ftime getcwd
do
trfunc=HAVE_`echo $func | tr '[a-z]' '[A-Z]'`
echo checking for ${func}
cat > conftest.c <<EOF
#include <stdio.h>
main() { exit(0); } 
t() { 
#ifdef __stub_${func}
choke me
#else
/* Override any gcc2 internal prototype to avoid an error.  */
extern char ${func}(); ${func}();
#endif
 }
EOF
if eval $compile; then
  DEFS="$DEFS -D${trfunc}=1"
fi
rm -f conftest*
#endif
done

echo checking for vprintf
cat > conftest.c <<EOF

main() { exit(0); } 
t() { vprintf(); }
EOF
if eval $compile; then
  DEFS="$DEFS -DHAVE_VPRINTF=1"
else
  vprintf_missing=1
fi
rm -f conftest*

if test -n "$vprintf_missing"; then
echo checking for _doprnt
cat > conftest.c <<EOF

main() { exit(0); } 
t() { _doprnt(); }
EOF
if eval $compile; then
  DEFS="$DEFS -DHAVE_DOPRNT=1"
fi
rm -f conftest*

fi

# The Ultrix 4.2 mips builtin alloca declared by alloca.h only works
# for constant arguments.  Useless!
echo checking for working alloca.h
cat > conftest.c <<EOF
#include <alloca.h>
main() { exit(0); } 
t() { char *p = alloca(2 * sizeof(int)); }
EOF
if eval $compile; then
  DEFS="$DEFS -DHAVE_ALLOCA_H=1"
fi
rm -f conftest*

decl="#ifdef __GNUC__
#define alloca __builtin_alloca
#else
#if HAVE_ALLOCA_H
#include <alloca.h>
#else
#ifdef _AIX
 #pragma alloca
#else
char *alloca ();
#endif
#endif
#endif
"
echo checking for alloca
cat > conftest.c <<EOF
$decl
main() { exit(0); } 
t() { char *p = (char *) alloca(1); }
EOF
if eval $compile; then
  :
else
  alloca_missing=1
fi
rm -f conftest*

if test -n "$alloca_missing"; then
  # The SVR3 libPW and SVR4 libucb both contain incompatible functions
  # that cause trouble.  Some versions do not even contain alloca or
  # contain a buggy version.  If you still want to use their alloca,
  # use ar to extract alloca.o from them instead of compiling alloca.c.
  ALLOCA=alloca.o
fi

echo checking for BSD
( test -f /vmunix || test -f /sdmach || test -f /../../mach ) && DEFS="$DEFS -DBSD42=1"
echo checking for HP-UX
test -f /hp-ux && test ! -f /vmunix && MALLOC=malloc.o

echo checking for Xenix
cat > conftest.c <<EOF
#if defined(M_XENIX) && !defined(M_UNIX)
  yes
#endif

EOF
eval "$CPP $DEFS conftest.c > conftest.out 2>&1"
if egrep "yes" conftest.out >/dev/null 2>&1; then
  XENIX=1
fi
rm -f conftest*

if test -n "$XENIX"; then
  DEFS="$DEFS -DVOID_CLOSEDIR=1"
  LIBS="$LIBS -lx"
  case "$DEFS" in
  *SYSNDIR*) ;;
  *) LIBS="-ldir $LIBS" ;; # Make sure -ldir precedes any -lx.
  esac
fi

libname=`echo "socket" | sed 's/lib\([^\.]*\)\.a/\1/;s/-l//'`
LIBS_save="${LIBS}"
LIBS="${LIBS} -l${libname}"
have_lib=""
echo checking for -l${libname}
cat > conftest.c <<EOF

main() { exit(0); } 
t() { main(); }
EOF
if eval $compile; then
  have_lib="1"
fi
rm -f conftest*
LIBS="${LIBS_save}"
if test -n "${have_lib}"; then
   :; LIBS="$LIBS -lsocket"
else
   :; 
fi

libname=`echo "nsl" | sed 's/lib\([^\.]*\)\.a/\1/;s/-l//'`
LIBS_save="${LIBS}"
LIBS="${LIBS} -l${libname}"
have_lib=""
echo checking for -l${libname}
cat > conftest.c <<EOF

main() { exit(0); } 
t() { main(); }
EOF
if eval $compile; then
  have_lib="1"
fi
rm -f conftest*
LIBS="${LIBS_save}"
if test -n "${have_lib}"; then
   :; LIBS="$LIBS -lnsl"
else
   :; 
fi

if test -n "$prefix"; then
  test -z "$exec_prefix" && exec_prefix='${prefix}'
  prsub="s%^prefix\\([ 	]*\\)=\\([ 	]*\\).*$%prefix\\1=\\2$prefix%"
fi
if test -n "$exec_prefix"; then
  prsub="$prsub
s%^exec_prefix\\([ 	]*\\)=\\([ 	]*\\).*$%\
exec_prefix\\1=\\2$exec_prefix%"
fi

trap 'rm -f config.status; exit 1' 1 3 15
echo creating config.status
rm -f config.status
cat > config.status <<EOF
#!/bin/sh
# Generated automatically by configure.
# Run this file to recreate the current configuration.
# This directory was configured as follows,
# on host `(hostname || uname -n) 2>/dev/null`:
#
# $0 $*

for arg
do
  case "\$arg" in
    -recheck | --recheck | --rechec | --reche | --rech | --rec | --re | --r)
    exec /bin/sh $0 $* ;;
    *) echo "Usage: config.status --recheck" 2>&1; exit 1 ;;
  esac
done

trap 'rm -f Makefile; exit 1' 1 3 15
PROGS='$PROGS'
CC='$CC'
CPP='$CPP'
INSTALL='$INSTALL'
INSTALL_PROGRAM='$INSTALL_PROGRAM'
INSTALL_DATA='$INSTALL_DATA'
YACC='$YACC'
RTAPELIB='$RTAPELIB'
DEF_AR_FILE='$DEF_AR_FILE'
ALLOCA='$ALLOCA'
MALLOC='$MALLOC'
LIBS='$LIBS'
srcdir='$srcdir'
DEFS='$DEFS'
prefix='$prefix'
exec_prefix='$exec_prefix'
prsub='$prsub'
EOF
cat >> config.status <<\EOF

top_srcdir=$srcdir
for file in .. Makefile; do if [ "x$file" != "x.." ]; then
  srcdir=$top_srcdir
  # Remove last slash and all that follows it.  Not all systems have dirname.
  dir=`echo $file|sed 's%/[^/][^/]*$%%'`
  if test "$dir" != "$file"; then
    test "$top_srcdir" != . && srcdir=$top_srcdir/$dir
    test ! -d $dir && mkdir $dir
  fi
  echo creating $file
  rm -f $file
  echo "# Generated automatically from `echo $file|sed 's|.*/||'`.in by configure." > $file
  sed -e "
$prsub
s%@PROGS@%$PROGS%g
s%@CC@%$CC%g
s%@CPP@%$CPP%g
s%@INSTALL@%$INSTALL%g
s%@INSTALL_PROGRAM@%$INSTALL_PROGRAM%g
s%@INSTALL_DATA@%$INSTALL_DATA%g
s%@YACC@%$YACC%g
s%@RTAPELIB@%$RTAPELIB%g
s%@DEF_AR_FILE@%$DEF_AR_FILE%g
s%@ALLOCA@%$ALLOCA%g
s%@MALLOC@%$MALLOC%g
s%@LIBS@%$LIBS%g
s%@srcdir@%$srcdir%g
s%@DEFS@%$DEFS%
" $top_srcdir/${file}.in >> $file
fi; done

exit 0
EOF
chmod +x config.status
test -n "$no_create" || ./config.status

