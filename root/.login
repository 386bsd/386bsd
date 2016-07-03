stty dec -decctlq
unset noglob
set path=(/sbin /usr/sbin /bin /usr/bin /usr/local/bin /usr/contrib/bin . /usr/new/bin)
umask 2
echo "Don't login as root, use su"
