setenv PATH	:/usr/sbin:/sbin:/usr/bin:/usr/local/bin:/bin:.
set path=(/usr/sbin /sbin /usr/bin /usr/local/bin /bin .)
setenv EXINIT	'set prompt magic '
setenv SHELL	/bin/csh
set time=20
umask 2
stty crt tabs
# if( "$TERM" == "dialup" ) then
#	tset -Q -e^H  -m dialup:\?f100-v
# else
#	tset -Q -e^H
# endif
