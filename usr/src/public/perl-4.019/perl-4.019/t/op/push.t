#!./perl

# $Header: push.t,v 4.0 91/03/20 01:54:07 lwall Locked $

@tests = split(/\n/, <<EOF);
0 3,			0 1 2,		3 4 5 6 7
0 0 a b c,		,		a b c 0 1 2 3 4 5 6 7
8 0 a b c,		,		0 1 2 3 4 5 6 7 a b c
7 0 6.5,		,		0 1 2 3 4 5 6 6.5 7
1 0 a b c d e f g h i j,,		0 a b c d e f g h i j 1 2 3 4 5 6 7
0 1 a,			0,		a 1 2 3 4 5 6 7
1 6 x y z,		1 2 3 4 5 6,	0 x y z 7
0 7 x y z,		0 1 2 3 4 5 6,	x y z 7
1 7 x y z,		1 2 3 4 5 6 7,	0 x y z
4,			4 5 6 7,	0 1 2 3
-4,			4 5 6 7,	0 1 2 3
EOF

print "1..", 2 + @tests, "\n";
die "blech" unless @tests;

@x = (1,2,3);
push(@x,@x);
if (join(':',@x) eq '1:2:3:1:2:3') {print "ok 1\n";} else {print "not ok 1\n";}
push(x,4);
if (join(':',@x) eq '1:2:3:1:2:3:4') {print "ok 2\n";} else {print "not ok 2\n";}

$test = 3;
foreach $line (@tests) {
    ($list,$get,$leave) = split(/,\t*/,$line);
    @list = split(' ',$list);
    @get = split(' ',$get);
    @leave = split(' ',$leave);
    @x = (0,1,2,3,4,5,6,7);
    @got = splice(@x,@list);
    if (join(':',@got) eq join(':',@get) &&
	join(':',@x) eq join(':',@leave)) {
	print "ok ",$test++,"\n";
    }
    else {
	print "not ok ",$test++," got: @got == @get left: @x == @leave\n";
    }
}

