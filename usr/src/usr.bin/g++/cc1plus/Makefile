#	@(#)Makefile	6.4 (Berkeley) 5/6/91

PROG=		cc1plus
BINDIR=		/usr/libexec
SRCS=		$(CINSN) aux-output.c c-common.c \
		caller-save.c calls.c combine.c convert.c \
		cp-parse.c \
		cp-call.c cp-class.c cp-cvt.c cp-decl.c cp-decl2.c \
		cp-edsel.c cp-except.c cp-expr.c cp-gc.c cp-init.c \
		cp-lex.c cp-method.c cp-pt.c cp-ptree.c cp-search.c \
		cp-spew.c cp-tree.c cp-type2.c cp-typeck.c cp-xref.c \
		cse.c dbxout.c emit-rtl.c \
		explow.c expmed.c expr.c final.c flow.c fold-const.c \
		function.c getpwd.c global.c integrate.c jump.c local-alloc.c \
		loop.c obstack.c optabs.c print-rtl.c print-tree.c real.c \
		recog.c \
		reg-stack.c regclass.c reload.c reload1.c reorg.c rtl.c \
		rtlanal.c sched.c stmt.c stor-layout.c stupid.c \
		toplev.c tree.c unroll.c varasm.c version.c

#OBJS+=		cp-parse.o
CFLAGS+=	-DIN_GCC -DUSE_COLLECT2 -I- -I. -I$(.CURDIR) -I$(.CURDIR)/../../gcc/cc1 \
		-I$(.CURDIR)/../../gcc/cc1/config
NOMAN=		noman
#CLEANFILES+=	$(HINSN) $(CINSN) cp-parse.c cp-parse.h y.tab.h
CLEANFILES+=	$(HINSN) $(CINSN)
.PATH:		$(.CURDIR)/../../gcc/cc1

#$(.CURDIR)/cp-lex.c:	cp-parse.h

#cp-parse.h cp-parse.c:
#	${YACC} -d ${.IMPSRC} ?
#	${YACC} -d $(.CURDIR)/cp-parse.y
#	mv y.tab.c cp-parse.c
#	mv y.tab.h cp-parse.h
#
# Some machine-dependent source files are generated from
# the machine description file.
#

HINSN=	insn-attr.h insn-codes.h insn-config.h insn-flags.h
CINSN1=	insn-emit.c insn-extract.c insn-output.c \
	insn-peep.c insn-recog.c insn-opinit.c
CINSN2=	insn-attrtab.c
CINSN=	$(CINSN1) $(CINSN2)
GEN=	genattr.c genattrtab.c gencodes.c genconfig.c genemit.c \
	genextract.c genflags.c genoutput.c genpeep.c genrecog.c \
	genopinit.c

$(CINSN):	md rtl.o rtlanal.o obstack.o $(GEN)
	for f in $(HINSN) $(CINSN1); do \
		p=`expr $$f : "insn-\(.*\)\.[ch]"`; \
		$(CC) $(CFLAGS) $(.CURDIR)/../../gcc/cc1/gen$$p.c rtl.o obstack.o -o gen; \
		./gen md > $$f; \
	done
	$(CC) $(CFLAGS) $(.CURDIR)/../../gcc/cc1/genattrtab.c \
		rtl.o rtlanal.o obstack.o -o gen
	 ./gen md > insn-attrtab.c
	rm -f gen

.include <bsd.prog.mk>
.include "../Makefile.symlinks"
