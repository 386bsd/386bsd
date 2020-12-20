#	@(#)Makefile	5.4.1.1 (Berkeley) 5/7/91

SUBDIR=	badsect clri disklabel dmesg dump dumpfs fastboot fsck \
	halt ifconfig init mknod mount \
	mountd newfs nfsd nfsiod ping reboot restore route \
	routed savecore shutdown slattach swapon tunefs umount

.if ${MACHINE} == "i386"
SUBDIR+= cmos
.endif
.include <bsd.subdir.mk>
