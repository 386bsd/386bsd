#!./perl

# $Header: pipe.t,v 4.0 91/03/20 01:51:02 lwall Locked $

$| = 1;
print "1..8\n";

open(PIPE, "|-") || (exec 'tr', '[A-Z]', '[a-z]');
print PIPE "OK 1\n";
print PIPE "ok 2\n";
close PIPE;

if (open(PIPE, "-|")) {
    while(<PIPE>) {
	s/^not //;
	print;
    }
}
else {
    print STDOUT "not ok 3\n";
    exec 'echo', 'not ok 4';
}

pipe(READER,WRITER) || die "Can't open pipe";

if ($pid = fork) {
    close WRITER;
    while(<READER>) {
	s/^not //;
	y/A-Z/a-z/;
	print;
    }
}
else {
    die "Couldn't fork" unless defined $pid;
    close READER;
    print WRITER "not ok 5\n";
    open(STDOUT,">&WRITER") || die "Can't dup WRITER to STDOUT";
    close WRITER;
    exec 'echo', 'not ok 6';
}


pipe(READER,WRITER) || die "Can't open pipe";
close READER;

$SIG{'PIPE'} = 'broken_pipe';

sub broken_pipe {
    print "ok 7\n";
}

print WRITER "not ok 7\n";
close WRITER;

print "ok 8\n";
