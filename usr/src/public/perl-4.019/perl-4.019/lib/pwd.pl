;# pwd.pl - keeps track of current working directory in PWD environment var
;#
;# $Header: pwd.pl,v 4.0 91/03/20 01:26:03 lwall Locked $
;#
;# $Log:	pwd.pl,v $
;# Revision 4.0  91/03/20  01:26:03  lwall
;# 4.0 baseline.
;# 
;# Revision 3.0.1.2  91/01/11  18:09:24  lwall
;# patch42: some .pl files were missing their trailing 1;
;# 
;# Revision 3.0.1.1  90/08/09  04:01:24  lwall
;# patch19: Initial revision
;# 
;#
;# Usage:
;#	require "pwd.pl";
;#	&initpwd;
;#	...
;#	&chdir($newdir);

package pwd;

sub main'initpwd {
    if ($ENV{'PWD'}) {
	local($dd,$di) = stat('.');
	local($pd,$pi) = stat($ENV{'PWD'});
	return if $di == $pi && $dd == $pd;
    }
    chop($ENV{'PWD'} = `pwd`);
}

sub main'chdir {
    local($newdir) = shift;
    if (chdir $newdir) {
	if ($newdir =~ m#^/#) {
	    $ENV{'PWD'} = $newdir;
	}
	else {
	    local(@curdir) = split(m#/#,$ENV{'PWD'});
	    @curdir = '' unless @curdir;
	    foreach $component (split(m#/#, $newdir)) {
		next if $component eq '.';
		pop(@curdir),next if $component eq '..';
		push(@curdir,$component);
	    }
	    $ENV{'PWD'} = join('/',@curdir) || '/';
	}
    }
    else {
	0;
    }
}

1;
