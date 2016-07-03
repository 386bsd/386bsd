#	@(#)Makefile	6.2 (Berkeley) 2/2/91

PROG=		cc1
BINDIR=		/usr/libexec
SRCS=		$(CINSN) aux-output.c c-aux-info.c c-common.c \
		c-convert.c c-decl.c c-lang.c c-lex.c \
		c-iterate.c c-pragma.c c-typeck.c \
		caller-save.c calls.c convert.c combine.c cse.c dbxout.c emit-rtl.c \
		explow.c expmed.c expr.c final.c flow.c fold-const.c \
		function.c getpwd.c global.c integrate.c jump.c local-alloc.c \
		loop.c obstack.c optabs.c print-rtl.c print-tree.c real.c recog.c \
		reg-stack.c regclass.c reload.c reload1.c reorg.c rtl.c \
		rtlanal.c sched.c stmt.c stor-layout.c stupid.c \
		toplev.c tree.c unroll.c varasm.c version.c
OBJS+=		c-parse.o
CFLAGS+=	-DIN_GCC -DUSE_COLLECT2 -I. -I$(.CURDIR) -I$(.CURDIR)/config
NOMAN=		noman
CLEANFILES+=	$(HINSN) $(CINSN) c-parse.c c-parse.h y.tab.h

$(.CURDIR)/c-lex.c:	c-parse.h

c-parse.h c-parse.c:
#	${YACC} -d ${.IMPSRC} ?
	${YACC} -d $(.CURDIR)/c-parse.y
	mv y.tab.c c-parse.c
	mv y.tab.h c-parse.h

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
	genextract.c genflags.c genoutput.c genpeep.c genrecog.c genopinit.c

$(CINSN):	md rtl.o rtlanal.o obstack.o $(GEN)
	for f in $(HINSN) $(CINSN1); do \
		p=`expr $$f : "insn-\(.*\)\.[ch]"`; \
		$(CC) $(CFLAGS) $(.CURDIR)/gen$$p.c rtl.o obstack.o -o gen; \
		./gen md > $$f; \
	done
	$(CC) $(CFLAGS) $(.CURDIR)/../../gcc/cc1/genattrtab.c \
		rtl.o rtlanal.o obstack.o -o gen
	 ./gen md > insn-attrtab.c
	rm -f gen

.include <bsd.prog.mk>
.include "../Makefile.symlinks"
