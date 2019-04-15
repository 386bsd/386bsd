sortlist 192.168.100.0, 192.168.1.0

directory	/etc/namedb

forwarders	192.168.1.1

; type    domain		source host/file		backup file

cache	.							root.cache
primary	 0.0.127.IN-ADDR.ARPA	localhost.rev

primary	odysseus.vm	vmhosts
primary 100.168.192.IN-ADDR.ARPA vmhosts.rev
