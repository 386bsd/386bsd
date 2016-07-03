# MODULE=ddb
.PATH:	$S/ddb/${MACHINE} $S/ddb
KERN_SRCS+= \
	db_access.c db_aout.c db_break.c db_command.c db_examine.c \
	db_expr.c db_input.c db_lex.c db_output.c db_print.c db_run.c \
	db_sym.c db_trap.c db_variables.c db_watch.c db_write_cmd.c
MACH_SRCS+= \
	db_disasm.c db_interface.c db_trace.c
MACH_SRCS_S+= \
	db_setjmp.s
MAN4+=	ddb.0
DBSYM=	dbsym
DEBUGSYM=	-X
IDENT+=	-DDDB	# XXX -- eliminate conditional compilation dependancies

# .include <bsd.kernel.module>
