#	@(#)Makefile	6.3 (Berkeley) 4/30/91

DEVICE=		ps
FONTFILES=	$(FONTS) $(SUPPORT) DESC
FONTS=		AB ABI AI AR BMB BMBI BMI BMR CB CBI CI CR HB HBI \
		HI HNB HNBI HNI HNR HR NB NBI NI NR PB PBI PI PR \
		S SS TB TBI TI TR ZCMI ZD ZDR
SUPPORT=        download prologue symbolsl.ps text.enc \
		zapfdr.ps

clean cleandir:

prologue: prologue.ps
	-rm -f $@
	sed -f $(.CURDIR)/psstrip.sed $(.CURDIR)/$? >$@

DESC: DESC.in
	-rm -f $@
	cat $(.CURDIR)/DESC.in >DESC
	echo broken $(BROKEN_SPOOLER_FLAGS) >>DESC
	if test "$(PAGE)" = A4; then \
	  echo "paperlength 841890" >>DESC; \
	else \
	  echo "paperlength 792000" >>DESC; \
	fi
	test -z '$(PSPRINT)' ||	echo print '$(PSPRINT)' >>DESC

.include "../Makefile.dev"
