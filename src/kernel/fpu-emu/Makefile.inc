.PATH:	$S/fpu-emu
.SUFFIXES: .S
#
#               Makefile for wm-FPU-emu
#

DEV_SRCS += \
	div_small.S poly_div.S poly_mul64.S polynomial.S reg_div.S \
	reg_norm.S reg_round.S reg_u_add.S reg_u_div.S reg_u_mul.S \
	reg_u_sub.S wm_shrx.S wm_sqrt.S \
	errors.c fpu_arith.c fpu_aux.c fpu_entry.c fpu_etc.c fpu_trig.c \
	get_address.c load_store.c poly_2xm1.c poly_atan.c poly_l2.c \
	poly_sin.c poly_tan.c reg_add_sub.c reg_compare.c reg_constant.c \
	reg_ld_str.c reg_mul.c
