unset histchars
set histchars=',;%'
set histchars='!^%'
if ($?prompt) then
	set mail=/usr/spool/mail/$USER
	switch  ($prompt:q)
		case '# ':
		set prompt="`hostname -s` \! # "
		breaksw
		default:
		set prompt="`hostname -s` \! % "
	endsw
	set	cdpath=(/sys ~/working /usr/src)
	set	ignoreeof
	set	history=60
	alias tset 'set xx=$histchars; set histchars=""; unset history; /usr/ucb/tset -s -Q \!*  > /tmp/tset$$ ; source /tmp/tset$$ ; rm /tmp/tset$$; set history=60; set histchars=$xx; unset xx'
	alias a alias
	a	ls	'ls -F'
	alias	setup	'sccs admin -i \!:1 < \!:1'
	a . 'cd; cd'
	a mail Mail
	a pwd 'echo $cwd'
	a so source
	a j jobs
	a h history
	a pd pushd
	a p popd
	a tag 'vi -ta \!*'
	a nd	'nm -nr \!* | grep " [DBdb] " | more'
	a reset	'reset ; stty -tabs'
	a RESET reset
	a xi	'xterm -e vi \!* &'
endif
