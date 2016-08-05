#!./perl

# $RCSfile: subval.t,v $$Revision: 4.0.1.1 $$Date: 91/11/05 18:42:31 $

sub foo1 {
    'true1';
    if ($_[0]) { 'true2'; }
}

sub foo2 {
    'true1';
    if ($_[0]) { return 'true2'; } else { return 'true3'; }
    'true0';
}

sub foo3 {
    'true1';
    unless ($_[0]) { 'true2'; }
}

sub foo4 {
    'true1';
    unless ($_[0]) { 'true2'; } else { 'true3'; }
}

sub foo5 {
    'true1';
    'true2' if $_[0];
}

sub foo6 {
    'true1';
    'true2' unless $_[0];
}

print "1..34\n";

if (do foo1(0) eq '0') {print "ok 1\n";} else {print "not ok 1 $foo\n";}
if (do foo1(1) eq 'true2') {print "ok 2\n";} else {print "not ok 2\n";}
if (do foo2(0) eq 'true3') {print "ok 3\n";} else {print "not ok 3\n";}
if (do foo2(1) eq 'true2') {print "ok 4\n";} else {print "not ok 4\n";}

if (do foo3(0) eq 'true2') {print "ok 5\n";} else {print "not ok 5\n";}
if (do foo3(1) eq '1') {print "ok 6\n";} else {print "not ok 6\n";}
if (do foo4(0) eq 'true2') {print "ok 7\n";} else {print "not ok 7\n";}
if (do foo4(1) eq 'true3') {print "ok 8\n";} else {print "not ok 8\n";}

if (do foo5(0) eq '0') {print "ok 9\n";} else {print "not ok 9\n";}
if (do foo5(1) eq 'true2') {print "ok 10\n";} else {print "not ok 10\n";}
if (do foo6(0) eq 'true2') {print "ok 11\n";} else {print "not ok 11\n";}
if (do foo6(1) eq '1') {print "ok 12\n";} else {print "not ok 12 $x\n";}

# Now test to see that recursion works using a Fibonacci number generator

sub fib {
    local($arg) = @_;
    local($foo);
    $level++;
    if ($arg <= 2) {
	$foo = 1;
    }
    else {
	$foo = do fib($arg-1) + do fib($arg-2);
    }
    $level--;
    $foo;
}

@good = (0,1,1,2,3,5,8,13,21,34,55,89);

for ($i = 1; $i <= 10; $i++) {
    $foo = $i + 12;
    if (do fib($i) == $good[$i]) {
	print "ok $foo\n";
    }
    else {
	print "not ok $foo\n";
    }
}

sub ary1 {
    (1,2,3);
}

print &ary1 eq 3 ? "ok 23\n" : "not ok 23\n";

print join(':',&ary1) eq '1:2:3' ? "ok 24\n" : "not ok 24\n";

sub ary2 {
    do {
	return (1,2,3);
	(3,2,1);
    };
    0;
}

print &ary2 eq 3 ? "ok 25\n" : "not ok 25\n";

$x = join(':',&ary2);
print $x eq '1:2:3' ? "ok 26\n" : "not ok 26 $x\n";

sub somesub {
    local($num,$P,$F,$L) = @_;
    ($p,$f,$l) = caller;
    print "$p:$f:$l" eq "$P:$F:$L" ? "ok $num\n" : "not ok $num $p:$f:$l ne $P:$F:$L\n";
}

&somesub(27, 'main', __FILE__, __LINE__);

package foo;
&main'somesub(28, 'foo', __FILE__, __LINE__);

package main;
$i = 28;
open(FOO,">Cmd_subval.tmp");
print FOO "blah blah\n";
close FOO;

&file_main(*F);
close F;
&info_main;

&file_package(*F);
close F;
&info_package;

unlink 'Cmd_subval.tmp';

sub file_main {
        local(*F) = @_;

        open(F, 'Cmd_subval.tmp') || die "can't open\n";
	$i++;
        eof F ? print "not ok $i\n" : print "ok $i\n";
}

sub info_main {
        local(*F);

        open(F, 'Cmd_subval.tmp') || die "test: can't open\n";
	$i++;
        eof F ? print "not ok $i\n" : print "ok $i\n";
        &iseof(*F);
	close F;
}

sub iseof {
        local(*UNIQ) = @_;

	$i++;
        eof UNIQ ? print "(not ok $i)\n" : print "ok $i\n";
}

{package foo;

 sub main'file_package {
        local(*F) = @_;

        open(F, 'Cmd_subval.tmp') || die "can't open\n";
	$main'i++;
        eof F ? print "not ok $main'i\n" : print "ok $main'i\n";
 }

 sub main'info_package {
        local(*F);

        open(F, 'Cmd_subval.tmp') || die "can't open\n";
	$main'i++;
        eof F ? print "not ok $main'i\n" : print "ok $main'i\n";
        &iseof(*F);
 }

 sub iseof {
        local(*UNIQ) = @_;

	$main'i++;
        eof UNIQ ? print "not ok $main'i\n" : print "ok $main'i\n";
 }
}
