#  Copyright (c) 1988-1990 The Regents of the University of California.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that: (1) source code distributions
#  retain the above copyright notice and this paragraph in its entirety, (2)
#  distributions including binary code include the above copyright notice and
#  this paragraph in its entirety in the documentation or other materials
#  provided with the distribution, and (3) all advertising materials mentioning
#  features or use of this software display the following acknowledgement:
#  ``This product includes software developed by the University of California,
#  Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
#  the University nor the names of its contributors may be used to endorse
#  or promote products derived from this software without specific prior
#  written permission.
#  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
#  WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
#  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
# @(#) $Header: Makefile,v 1.85 91/06/17 16:43:42 mccanne Exp $ (LBL)

# Possible Options:
#  -DCSLIP	include support for kernel slip interface
#  -DPPP	include support for point to point protocol
#  -DIBMRTPC	enable the MERIT additions to the Stanford Enetfilter
#  -DNOVFPRINTF	simulate vfprintf() on systems that don't have it
#
# CSLIP and PPP work only under BPF.
#
DEFINES = -DCSLIP -DPPP
CCOPT = -O
INCLUDES = -I.. -I/usr/include/nonstd/bsd

# The installed binary is owned by this group.
GROUP = operator

# Full pathname of where to install the binary
BINDEST = /usr/sbin/tcpdump
# Full pathname of where to install the manual entry
MANDEST = /usr/man/man8/tcpdump.8

# Standard CFLAGS
STANDARD_CFLAGS = $(CCOPT) $(DEFINES) $(INCLUDES)

# CFLAGS definitions per machine vs. os
CFLAGS = $(STANDARD_CFLAGS)
#mips-ultrix#CFLAGS = $(STANDARD_CFLAGS)
#rt-bsd#CFLAGS = $(STANDARD_CFLAGS) -U__STDC__
#sun3-sunos3#CFLAGS = $(STANDARD_CFLAGS) 
#sun3-sunos4#CFLAGS = $(STANDARD_CFLAGS)
#sun4-sunos4#CFLAGS = $(STANDARD_CFLAGS)
#tahoe-bsd#CFLAGS = $(STANDARD_CFLAGS)
#vax-bsd#CFLAGS = $(STANDARD_CFLAGS)
#vax-ultrix#CFLAGS = $(STANDARD_CFLAGS)

SUBDIRS = hp300-bsd mips-ultrix sun?-sunos? tahoe-bsd vax-bsd vax-ultrix rt-bsd

# Change these to cc/lex/yacc if you don't have gcc, flex and/or bison.
CC = cc -g
LEX = lex
YACC = yacc
MAKE = make

# Script (or program) that returns the machine and os types,
# or jsut edit in the name yourself.
MD=i386
# `./mdtype`
OS=bsd
# `./ostype`

# Explicitly define compiliation rule since SunOS 4's make doesn't like gcc.
# Also, gcc does not remove the .o before forking 'as', which can be a
# problem if you don't own the file but can write to the directory.
.c.o:
	rm -f $@; $(CC) $(CFLAGS) -c $*.c

CSRC =	tcpdump.c addrtoname.c \
	print-ether.c print-ip.c print-arp.c print-tcp.c print-udp.c \
	print-atalk.c print-domain.c print-tftp.c print-bootp.c print-nfs.c \
	print-icmp.c nametoaddr.c print-sl.c print-ppp.c print-rip.c \
	print-snmp.c print-ntp.c \
	savefile.c util.c etherent.c inet.c gencode.c optimize.c \
	bpf_dump.c bpf_filter.c bpf_image.c

SRC =	$(CSRC) tcpgram.y tcplex.l

MSRC =	md-hp300.c md-mips.c md-sun3.c md-sun4.c md-vax.c md-rt.c

PSRC =	pcap-nit.c pcap-pf.c pcap-snit.c pcap-bpf.c pcap-enet.c

OSRC =	os-sunos3.c os-sunos4.c os-bsd.c os-ultrix.c

OBJ =	tcpdump.o tcpgram.o tcplex.o addrtoname.o \
	print-ether.o print-ip.o print-arp.o print-tcp.o print-udp.o \
	print-atalk.o print-domain.o print-tftp.o print-bootp.o print-nfs.o \
	print-icmp.o nametoaddr.o print-sl.o print-ppp.o print-rip.o \
	print-snmp.o print-ntp.o \
	pcap.o md.o os.o \
	savefile.o util.o etherent.o inet.o gencode.o optimize.o \
	bpf_dump.o bpf_filter.o bpf_image.o

HDR =	interface.h appletalk.h addrtoname.h \
	bootp.h mib.h savefile.h etherproto.h nametoaddr.h \
	etherent.h ntp.h gencode.h extract.h

MHDR =	md-hp300.h md-sun3.h md-sun4.h md-mips.h md-vax.h md-rt.h

OHDR =	os-bsd.h os-sunos3.h os-sunos4.h os-ultrix.h

TAGHDR=	/usr/include/netinet/in.h \
	/usr/include/netinet/udp.h \
	/usr/include/netinet/tcp.h \
	/usr/include/arpa/tftp.h \
	/usr/include/netinet/if_ether.h

# These files are automatically generated.
#
GEN =	tokdefs.h tcpgram.c tcplex.c

AWKS =	atime.awk packetdat.awk send-ack.awk stime.awk

# "all" must come before tcpdump so we can delete it with sed

tcpdump: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LIB)

submake: force
	-@dir=$(MD)-$(OS); \
	if [ ! -d $$dir ]; then ${MAKE} ${MFLAGS} config; fi; \
	set -x; \
	cd $$dir; ${MAKE} ${MFLAGS}

# N.B.- symbolic links are used in the subdirectory rather than VPATH
# because at least one Sun cc compiler puts the .o in the wrong place
# when using VPATH and it's almost impossible to get "make depend" to
# do the right thing.

config:
	-@md=$(MD); os=$(OS); dir=$$md-$$os; list=; \
	if [ -f /dev/bpf0 ] ; then pcap="bpf"; \
	elif [ $$os = "sunos3" ] ; then pcap="nit"; \
	elif [ $$os = "sunos4" ] ; then pcap="snit"; \
	elif [ $$os = "ultrix" ] ; then pcap="pf"; \
	elif [ -f /usr/include/net/enet.h ] ; then pcap="enet"; \
	else pcap="bpf"; fi; \
	for i in $(SRC) $(HDR); do list="../$$i $$list"; done; \
	set -x; mkdir $$dir; chmod ug+w $$dir; ln -s $$list $$dir; \
	ln -s ../md-$$md.c $$dir/md.c; ln -s ../md-$$md.h $$dir/md.h; \
	ln -s ../os-$$os.c $$dir/os.c; ln -s ../os-$$os.h $$dir/os.h; \
	ln -s ../pcap-$$pcap.c $$dir/pcap.c; \
	sed -e "/^all:/d" -e "s/^#$$dir#//" Makefile >$$dir/Makefile; \
	chmod ug+w $$dir/Makefile; \
	cd $$dir ; ${MAKE} ${MFLAGS} depend

tcplex.c: tcplex.l
	rm -f $@
	$(LEX) $<
	mv -f lex.yy.c tcplex.c

tokdefs.h: tcpgram.c
tcpgram.c: tcpgram.y
	rm -f tcpgram.c tokdefs.h
	$(YACC) -d $<
	mv y.tab.c tcpgram.c
	mv y.tab.h tokdefs.h

install: force
	@dir=$(MD)-$(OS); set -x; \
	install -m 750 -g $(GROUP) $$dir/tcpdump $(DESTDIR)$(BINDEST); \
	install -c tcpdump.1 $(DESTDIR)$(MANDEST)

lint:	force
	lint -hbxn $(CSRC) | \
	    grep -v 'struct/union .* never defined' | \
	    grep -v 'possible pointer alignment problem'

# This might be futile
lint.ultrix: force
	lint $(CFLAGS_ULTRIX) $(CSRC) tcplex.c tcpgram.c

clean:
	rm -f *.o tcpdump tcpdump.shar $(GEN) ; \
	rm -rf $(SUBDIRS)

TAGFILES = $(SRC) md.c os.c pcap.c $(HDR) md.h os.h $(TAGHDR) 

tags:	$(TAGFILES)
	ctags -wt $(TAGFILES)

TARNAME = /tmp/tcpdump-2.1beta.tar

tar:	force
	tar cf $(TARNAME) README INSTALL Makefile tcpdump.1 \
		mdtype ostype ostype.awk makemib \
		$(SRC) $(MSRC) $(OSRC) $(PSRC) \
		$(HDR) $(MHDR) $(OHDR) \
		$(AWKS) \
		SUNOS4 net
	tar ufhFF $(TARNAME) bpf
	rm -f $(TARNAME).Z
	compress $(TARNAME)

force:	/tmp

depend:	force
	@set -x ; \
	rm -f $(GEN) ; \
	grep '^#include' tcpgram.y > tcpgram.c ; \
	grep '^#include' tcplex.l > tcplex.c ; \
	touch tokdefs.h
	-mkdep $(DEFINES) $(INCLUDES) $(CSRC) tcpgram.c tcplex.c
	rm -f $(GEN)
