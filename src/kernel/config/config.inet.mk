#
# INTERNET protocols, NFS, and usual complement of pseudo devices.
# 

# inet network protocol  (BSD communications domain)
#.include "$S/inet/Makefile.inc"

# network filesystem
#.include "$S/nfs/Makefile.inc"

# pseudo devices (for use with network)
.include "$S/pty/Makefile.inc"
.include "$S/route/Makefile.inc"
#.include "$S/slip/Makefile.inc"
#.include "$S/ppp/Makefile.inc"
.include "$S/loop/Makefile.inc"
