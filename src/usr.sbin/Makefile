# $Id$

SUBDIR=	arp chat chown chroot dbsym dev_mkdb diskpart \
	inetd kgmon kvm_mkdb lpr portmap pppd pppstats mtree \
	named pwd_mkdb rmt sendmail sliplogin syslogd timed \
	traceroute update vipw

.if   make(clean) || make(cleandir)
SUBDIR+=bad144
.elif ${MACHINE} == "i386"
SUBDIR+=bad144 
.endif

.include <bsd.subdir.mk>
