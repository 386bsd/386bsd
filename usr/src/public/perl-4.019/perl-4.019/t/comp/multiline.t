#!./perl

# $Header: multiline.t,v 4.0 91/03/20 01:50:15 lwall Locked $

print "1..5\n";

open(try,'>Comp.try') || (die "Can't open temp file.");

$x = 'now is the time
for all good men
to come to.
';

$y = 'now is the time' . "\n" .
'for all good men' . "\n" .
'to come to.' . "\n";

if ($x eq $y) {print "ok 1\n";} else {print "not ok 1\n";}

print try $x;
close try;

open(try,'Comp.try') || (die "Can't reopen temp file.");
$count = 0;
$z = '';
while (<try>) {
    $z .= $_;
    $count = $count + 1;
}

if ($z eq $y) {print "ok 2\n";} else {print "not ok 2\n";}

if ($count == 3) {print "ok 3\n";} else {print "not ok 3\n";}

$_ = `cat Comp.try`;

if (/.*\n.*\n.*\n$/) {print "ok 4\n";} else {print "not ok 4\n";}
`/bin/rm -f Comp.try`;

if ($_ eq $y) {print "ok 5\n";} else {print "not ok 5\n";}
