#!./perl -P

# $Header: cpp.t,v 4.0 91/03/20 01:50:05 lwall Locked $

print "1..3\n";

#this is a comment
#define MESS "ok 1\n"
print MESS;

#If you capitalize, it's a comment.
#ifdef MESS
	print "ok 2\n";
#else
	print "not ok 2\n";
#endif

open(TRY,">Comp.cpp.tmp") || die "Can't open temp perl file.";

($prog = <<'END') =~ s/X//g;
X$ok = "not ok 3\n";
X#include "Comp.cpp.inc"
X#ifdef OK
X$ok = OK;
X#endif
Xprint $ok;
END
print TRY $prog;
close TRY;

open(TRY,">Comp.cpp.inc") || (die "Can't open temp include file.");
print TRY '#define OK "ok 3\n"' . "\n";
close TRY;

$pwd=`pwd`;
$pwd =~ s/\n//;
$x = `./perl -P Comp.cpp.tmp`;
print $x;
unlink "Comp.cpp.tmp", "Comp.cpp.inc";
