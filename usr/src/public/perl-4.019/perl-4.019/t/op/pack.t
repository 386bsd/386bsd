#!./perl

# $Header: pack.t,v 4.0 91/03/20 01:53:57 lwall Locked $

print "1..3\n";

$format = "c2x5CCxsdila6";
# Need the expression in here to force ary[5] to be numeric.  This avoids
# test2 failing because ary2 goes str->numeric->str and ary doesn't.
@ary = (1,-100,127,128,32767,987.654321098 / 100.0,12345,123456,"abcdef");
$foo = pack($format,@ary);
@ary2 = unpack($format,$foo);

print ($#ary == $#ary2 ? "ok 1\n" : "not ok 1\n");

$out1=join(':',@ary);
$out2=join(':',@ary2);
print ($out1 eq $out2 ? "ok 2\n" : "not ok 2\n");

print ($foo =~ /def/ ? "ok 3\n" : "not ok 3\n");
