extproc perl -x
#!perl

# Poor man's perl shell.

# Simply type two carriage returns every time you want to evaluate.
# Note that it must be a complete perl statement--don't type double
#  carriage return in the middle of a loop.

print "Perl shell\n> ";

$/ = '';	# set paragraph mode
$SHlinesep = "\n";

while ($SHcmd = <>) {
    $/ = $SHlinesep;
    eval $SHcmd; print $@ || "\n> ";
    $SHlinesep = $/; $/ = '';
}
