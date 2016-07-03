#	from:	@(#)Makefile.inc	5.1 (Berkeley) 2/24/91
#	386BSD:	$Id$
#
.PATH: ${.CURDIR}/db
NONSTDINC+= ${INCDB}

.include "${.CURDIR}/db/btree/Makefile.inc"
.include "${.CURDIR}/db/db/Makefile.inc"
.include "${.CURDIR}/db/hash/Makefile.inc"
.include "${.CURDIR}/db/man/Makefile.inc"
.include "${.CURDIR}/db/mpool/Makefile.inc"
.include "${.CURDIR}/db/recno/Makefile.inc"
