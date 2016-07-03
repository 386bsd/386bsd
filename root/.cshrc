alias mail Mail
set history=100
# directory stuff: cdpath/cd/back
set	cdpath=(/usr/src/{bin,sbin,etc,usr.bin,lib,usr.lib,lib/libc,libexec} /sys)
alias	cd	'set old=$cwd; chdir \!*'
alias	h	history
alias	j	jobs -l
alias	ls	ls -F
alias	back	'set back=$old; set old=$cwd; cd $back; unset back; dirs'
alias	z		suspend
alias	area	'grep \!* /usr/games/lib/quiz.k/areas'
alias	x	exit
alias	pd	pushd
alias	pd2	pushd +2
alias	pd3	pushd +3
alias	pd4	pushd +4
alias	l	"echo \!*{*}"
set path=(/sbin /usr/sbin /bin /usr/bin /usr/local/bin /usr/contrib/bin . /usr/new/bin)
if ($?prompt) then
	set prompt="`hostname -s ` # "
endif
