.SUFFIXES:
.SUFFIXES: .c .s .S .o
#
#               Makefile for wm-FPU-emu
#

#DEBUG	= -DDEBUGGING
DEBUG	=
CFLAGS	:= $(CFLAGS) -DPARANOID -I../include -DKERNEL $(DEBUG) -fno-builtin
CC=cc

.c.o:
	$(CC) $(CFLAGS) $(MATH_EMULATION) -c $<

.S.o:
	$(CC) $(CFLAGS) -D__ASSEMBLER__ -c $<

.s.o:
	$(CC) -c $<

OBJS =	fpu_entry.o

#ifdef CONFIG_MATH_EMULATION

OBJS := $(OBJS) div_small.o errors.o \
	fpu_arith.o fpu_aux.o fpu_etc.o fpu_trig.o \
	load_store.o get_address.o \
	poly_atan.o poly_l2.o poly_2xm1.o poly_sin.o poly_tan.o \
	poly_div.o poly_mul64.o polynomial.o \
	reg_add_sub.o reg_compare.o reg_constant.o reg_ld_str.o \
	reg_div.o reg_mul.o reg_norm.o \
	reg_u_add.o reg_u_div.o reg_u_mul.o reg_u_sub.o \
	reg_round.o \
	wm_shrx.o wm_sqrt.o

#endif

math.a: $(OBJS)
	rm -f math.a
	$(AR) rc math.a $(OBJS)
	sync

clean:
	rm -f core *.o *.a *.s

dep:
	$(CPP) -M *.c *.S > .depend

proto:
	cproto -e -DMAKING_PROTO *.c >fpu_proto.h

dummy:

#
# include a dependency file if one exists
#
#ifeq (.depend,$(wildcard .depend))
#include .depend
#endif
