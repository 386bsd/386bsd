#!./perl

# $Header: magic.t,v 4.0 91/03/20 01:53:35 lwall Locked $

$| = 1;		# command buffering

print "1..5\n";

eval '$ENV{"foo"} = "hi there";';	# check that ENV is inited inside eval
if (`echo \$foo` eq "hi there\n") {print "ok 1\n";} else {print "not ok 1\n";}

unlink 'ajslkdfpqjsjfk';
$! = 0;
open(foo,'ajslkdfpqjsjfk');
if ($! == 2) {print "ok 2\n";} else {print "not ok 2\n";}

# the next tests are embedded inside system simply because sh spits out
# a newline onto stderr when a child process kills itself with SIGINT.

system './perl',
'-e', '$| = 1;		# command buffering',

'-e', '$SIG{"INT"} = "ok3"; kill 2,$$;',
'-e', '$SIG{"INT"} = "IGNORE"; kill 2,$$; print "ok 4\n";',
'-e', '$SIG{"INT"} = "DEFAULT"; kill 2,$$; print "not ok\n";',

'-e', 'sub ok3 { print "ok 3\n" if pop(@_) eq "INT"; }';

@val1 = @ENV{keys(%ENV)};	# can we slice ENV?
@val2 = values(%ENV);

print join(':',@val1) eq join(':',@val2) ? "ok 5\n" : "not ok 5\n";
