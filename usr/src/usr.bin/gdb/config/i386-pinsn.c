/* Print i386 instructions for GDB, the GNU debugger.
   Copyright (C) 1988, 1989 Free Software Foundation, Inc.

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * 80386 instruction printer by Pace Willisson (pace@prep.ai.mit.edu)
 * July 1988
 */

/*
 * The main tables describing the instructions is essentially a copy
 * of the "Opcode Map" chapter (Appendix A) of the Intel 80386
 * Programmers Manual.  Usually, there is a capital letter, followed
 * by a small letter.  The capital letter tell the addressing mode,
 * and the small letter tells about the operand size.  Refer to 
 * the Intel manual for details.
 */

#include <stdio.h>
#include <ctype.h>

#define Eb OP_E, b_mode
#define indirEb OP_indirE, b_mode
#define Gb OP_G, b_mode
#define Ev OP_E, v_mode
#define indirEv OP_indirE, v_mode
#define Ew OP_E, w_mode
#define Ma OP_E, v_mode
#define M OP_E, 0
#define Mp OP_E, 0		/* ? */
#define Gv OP_G, v_mode
#define Gw OP_G, w_mode
#define Rw OP_rm, w_mode
#define Rd OP_rm, d_mode
#define Ib OP_I, b_mode
#define sIb OP_sI, b_mode	/* sign extened byte */
#define Iv OP_I, v_mode
#define Iw OP_I, w_mode
#define Jb OP_J, b_mode
#define Jv OP_J, v_mode
#define ONE OP_ONE, 0
#define Cd OP_C, d_mode
#define Dd OP_D, d_mode
#define Td OP_T, d_mode

#define eAX OP_REG, eAX_reg
#define eBX OP_REG, eBX_reg
#define eCX OP_REG, eCX_reg
#define eDX OP_REG, eDX_reg
#define eSP OP_REG, eSP_reg
#define eBP OP_REG, eBP_reg
#define eSI OP_REG, eSI_reg
#define eDI OP_REG, eDI_reg
#define AL OP_REG, al_reg
#define CL OP_REG, cl_reg
#define DL OP_REG, dl_reg
#define BL OP_REG, bl_reg
#define AH OP_REG, ah_reg
#define CH OP_REG, ch_reg
#define DH OP_REG, dh_reg
#define BH OP_REG, bh_reg
#define AX OP_REG, ax_reg
#define DX OP_REG, dx_reg
#define indirDX OP_REG, indir_dx_reg

#define Sw OP_SEG, w_mode
#define Ap OP_DIR, lptr
#define Av OP_DIR, v_mode
#define Ob OP_OFF, b_mode
#define Ov OP_OFF, v_mode
#define Xb OP_DSSI, b_mode
#define Xv OP_DSSI, v_mode
#define Yb OP_ESDI, b_mode
#define Yv OP_ESDI, v_mode

#define es OP_REG, es_reg
#define ss OP_REG, ss_reg
#define cs OP_REG, cs_reg
#define ds OP_REG, ds_reg
#define fs OP_REG, fs_reg
#define gs OP_REG, gs_reg

int OP_E(), OP_indirE(), OP_G(), OP_I(), OP_sI(), OP_REG();
int OP_J(), OP_SEG();
int OP_DIR(), OP_OFF(), OP_DSSI(), OP_ESDI(), OP_ONE(), OP_C();
int OP_D(), OP_T(), OP_rm();


#define b_mode 1
#define v_mode 2
#define w_mode 3
#define d_mode 4

#define es_reg 100
#define cs_reg 101
#define ss_reg 102
#define ds_reg 103
#define fs_reg 104
#define gs_reg 105
#define eAX_reg 107
#define eCX_reg 108
#define eDX_reg 109
#define eBX_reg 110
#define eSP_reg 111
#define eBP_reg 112
#define eSI_reg 113
#define eDI_reg 114

#define lptr 115

#define al_reg 116
#define cl_reg 117
#define dl_reg 118
#define bl_reg 119
#define ah_reg 120
#define ch_reg 121
#define dh_reg 122
#define bh_reg 123

#define ax_reg 124
#define cx_reg 125
#define dx_reg 126
#define bx_reg 127
#define sp_reg 128
#define bp_reg 129
#define si_reg 130
#define di_reg 131

#define indir_dx_reg 150

#define GRP1b NULL, NULL, 0
#define GRP1S NULL, NULL, 1
#define GRP1Ss NULL, NULL, 2
#define GRP2b NULL, NULL, 3
#define GRP2S NULL, NULL, 4
#define GRP2b_one NULL, NULL, 5
#define GRP2S_one NULL, NULL, 6
#define GRP2b_cl NULL, NULL, 7
#define GRP2S_cl NULL, NULL, 8
#define GRP3b NULL, NULL, 9
#define GRP3S NULL, NULL, 10
#define GRP4  NULL, NULL, 11
#define GRP5  NULL, NULL, 12
#define GRP6  NULL, NULL, 13
#define GRP7 NULL, NULL, 14
#define GRP8 NULL, NULL, 15

#define FLOATCODE 50
#define FLOAT NULL, NULL, FLOATCODE

struct dis386 {
  char *name;
  int (*op1)();
  int bytemode1;
  int (*op2)();
  int bytemode2;
  int (*op3)();
  int bytemode3;
};

struct dis386 dis386[] = {
  /* 00 */
  { "addb",	Eb, Gb },
  { "addS",	Ev, Gv },
  { "addb",	Gb, Eb },
  { "addS",	Gv, Ev },
  { "addb",	AL, Ib },
  { "addS",	eAX, Iv },
  { "pushl",	es },
  { "popl",	es },
  /* 08 */
  { "orb",	Eb, Gb },
  { "orS",	Ev, Gv },
  { "orb",	Gb, Eb },
  { "orS",	Gv, Ev },
  { "orb",	AL, Ib },
  { "orS",	eAX, Iv },
  { "pushl",	cs },
  { "(bad)" },	/* 0x0f extended opcode escape */
  /* 10 */
  { "adcb",	Eb, Gb },
  { "adcS",	Ev, Gv },
  { "adcb",	Gb, Eb },
  { "adcS",	Gv, Ev },
  { "adcb",	AL, Ib },
  { "adcS",	eAX, Iv },
  { "pushl",	ss },
  { "popl",	ss },
  /* 18 */
  { "sbbb",	Eb, Gb },
  { "sbbS",	Ev, Gv },
  { "sbbb",	Gb, Eb },
  { "sbbS",	Gv, Ev },
  { "sbbb",	AL, Ib },
  { "sbbS",	eAX, Iv },
  { "pushl",	ds },
  { "popl",	ds },
  /* 20 */
  { "andb",	Eb, Gb },
  { "andS",	Ev, Gv },
  { "andb",	Gb, Eb },
  { "andS",	Gv, Ev },
  { "andb",	AL, Ib },
  { "andS",	eAX, Iv },
  { "(bad)" },			/* SEG ES prefix */
  { "daa" },
  /* 28 */
  { "subb",	Eb, Gb },
  { "subS",	Ev, Gv },
  { "subb",	Gb, Eb },
  { "subS",	Gv, Ev },
  { "subb",	AL, Ib },
  { "subS",	eAX, Iv },
  { "(bad)" },			/* SEG CS prefix */
  { "das" },
  /* 30 */
  { "xorb",	Eb, Gb },
  { "xorS",	Ev, Gv },
  { "xorb",	Gb, Eb },
  { "xorS",	Gv, Ev },
  { "xorb",	AL, Ib },
  { "xorS",	eAX, Iv },
  { "(bad)" },			/* SEG SS prefix */
  { "aaa" },
  /* 38 */
  { "cmpb",	Eb, Gb },
  { "cmpS",	Ev, Gv },
  { "cmpb",	Gb, Eb },
  { "cmpS",	Gv, Ev },
  { "cmpb",	AL, Ib },
  { "cmpS",	eAX, Iv },
  { "(bad)" },			/* SEG DS prefix */
  { "aas" },
  /* 40 */
  { "incS",	eAX },
  { "incS",	eCX },
  { "incS",	eDX },
  { "incS",	eBX },
  { "incS",	eSP },
  { "incS",	eBP },
  { "incS",	eSI },
  { "incS",	eDI },
  /* 48 */
  { "decS",	eAX },
  { "decS",	eCX },
  { "decS",	eDX },
  { "decS",	eBX },
  { "decS",	eSP },
  { "decS",	eBP },
  { "decS",	eSI },
  { "decS",	eDI },
  /* 50 */
  { "pushS",	eAX },
  { "pushS",	eCX },
  { "pushS",	eDX },
  { "pushS",	eBX },
  { "pushS",	eSP },
  { "pushS",	eBP },
  { "pushS",	eSI },
  { "pushS",	eDI },
  /* 58 */
  { "popS",	eAX },
  { "popS",	eCX },
  { "popS",	eDX },
  { "popS",	eBX },
  { "popS",	eSP },
  { "popS",	eBP },
  { "popS",	eSI },
  { "popS",	eDI },
  /* 60 */
  { "pusha" },
  { "popa" },
  { "boundS",	Gv, Ma },
  { "arpl",	Ew, Gw },
  { "(bad)" },			/* seg fs */
  { "(bad)" },			/* seg gs */
  { "(bad)" },			/* op size prefix */
  { "(bad)" },			/* adr size prefix */
  /* 68 */
  { "pushS",	Iv },		/* 386 book wrong */
  { "imulS",	Gv, Ev, Iv },
  { "pushl",	sIb },		/* push of byte really pushes 4 bytes */
  { "imulS",	Gv, Ev, Ib },
  { "insb",	Yb, indirDX },
  { "insS",	Yv, indirDX },
  { "outsb",	indirDX, Xb },
  { "outsS",	indirDX, Xv },
  /* 70 */
  { "jo",		Jb },
  { "jno",	Jb },
  { "jb",		Jb },
  { "jae",	Jb },
  { "je",		Jb },
  { "jne",	Jb },
  { "jbe",	Jb },
  { "ja",		Jb },
  /* 78 */
  { "js",		Jb },
  { "jns",	Jb },
  { "jp",		Jb },
  { "jnp",	Jb },
  { "jl",		Jb },
  { "jnl",	Jb },
  { "jle",	Jb },
  { "jg",		Jb },
  /* 80 */
  { GRP1b },
  { GRP1S },
  { "(bad)" },
  { GRP1Ss },
  { "testb",	Eb, Gb },
  { "testS",	Ev, Gv },
  { "xchgb",	Eb, Gb },
  { "xchgS",	Ev, Gv },
  /* 88 */
  { "movb",	Eb, Gb },
  { "movS",	Ev, Gv },
  { "movb",	Gb, Eb },
  { "movS",	Gv, Ev },
  { "movw",	Ew, Sw },
  { "leaS",	Gv, M },
  { "movw",	Sw, Ew },
  { "popS",	Ev },
  /* 90 */
  { "nop" },
  { "xchgS",	eCX, eAX },
  { "xchgS",	eDX, eAX },
  { "xchgS",	eBX, eAX },
  { "xchgS",	eSP, eAX },
  { "xchgS",	eBP, eAX },
  { "xchgS",	eSI, eAX },
  { "xchgS",	eDI, eAX },
  /* 98 */
  { "cwtl" },
  { "cltd" },
  { "lcall",	Ap },
  { "(bad)" },		/* fwait */
  { "pushf" },
  { "popf" },
  { "sahf" },
  { "lahf" },
  /* a0 */
  { "movb",	AL, Ob },
  { "movS",	eAX, Ov },
  { "movb",	Ob, AL },
  { "movS",	Ov, eAX },
  { "movsb",	Yb, Xb },
  { "movsS",	Yv, Xv },
  { "cmpsb",	Yb, Xb },
  { "cmpsS",	Yv, Xv },
  /* a8 */
  { "testb",	AL, Ib },
  { "testS",	eAX, Iv },
  { "stosb",	Yb, AL },
  { "stosS",	Yv, eAX },
  { "lodsb",	AL, Xb },
  { "lodsS",	eAX, Xv },
  { "scasb",	AL, Xb },
  { "scasS",	eAX, Xv },
  /* b0 */
  { "movb",	AL, Ib },
  { "movb",	CL, Ib },
  { "movb",	DL, Ib },
  { "movb",	BL, Ib },
  { "movb",	AH, Ib },
  { "movb",	CH, Ib },
  { "movb",	DH, Ib },
  { "movb",	BH, Ib },
  /* b8 */
  { "movS",	eAX, Iv },
  { "movS",	eCX, Iv },
  { "movS",	eDX, Iv },
  { "movS",	eBX, Iv },
  { "movS",	eSP, Iv },
  { "movS",	eBP, Iv },
  { "movS",	eSI, Iv },
  { "movS",	eDI, Iv },
  /* c0 */
  { GRP2b },
  { GRP2S },
  { "ret",	Iw },
  { "ret" },
  { "lesS",	Gv, Mp },
  { "ldsS",	Gv, Mp },
  { "movb",	Eb, Ib },
  { "movS",	Ev, Iv },
  /* c8 */
  { "enter",	Iw, Ib },
  { "leave" },
  { "lret",	Iw },
  { "lret" },
  { "int3" },
  { "int",	Ib },
  { "into" },
  { "iret" },
  /* d0 */
  { GRP2b_one },
  { GRP2S_one },
  { GRP2b_cl },
  { GRP2S_cl },
  { "aam",	Ib },
  { "aad",	Ib },
  { "(bad)" },
  { "xlat" },
  /* d8 */
  { FLOAT },
  { FLOAT },
  { FLOAT },
  { FLOAT },
  { FLOAT },
  { FLOAT },
  { FLOAT },
  { FLOAT },
  /* e0 */
  { "loopne",	Jb },
  { "loope",	Jb },
  { "loop",	Jb },
  { "jCcxz",	Jb },
  { "inb",	AL, Ib },
  { "inS",	eAX, Ib },
  { "outb",	Ib, AL },
  { "outS",	Ib, eAX },
  /* e8 */
  { "call",	Av },
  { "jmp",	Jv },
  { "ljmp",	Ap },
  { "jmp",	Jb },
  { "inb",	AL, indirDX },
  { "inS",	eAX, indirDX },
  { "outb",	indirDX, AL },
  { "outS",	indirDX, eAX },
  /* f0 */
  { "(bad)" },			/* lock prefix */
  { "(bad)" },
  { "(bad)" },			/* repne */
  { "(bad)" },			/* repz */
  { "hlt" },
  { "cmc" },
  { GRP3b },
  { GRP3S },
  /* f8 */
  { "clc" },
  { "stc" },
  { "cli" },
  { "sti" },
  { "cld" },
  { "std" },
  { GRP4 },
  { GRP5 },
};

struct dis386 dis386_twobyte[] = {
  /* 00 */
  { GRP6 },
  { GRP7 },
  { "larS", Gv, Ew },
  { "lslS", Gv, Ew },  
  { "(bad)" },
  { "(bad)" },
  { "clts" },
  { "(bad)" },  
  /* 08 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* 10 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* 18 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* 20 */
  /* these are all backward in appendix A of the intel book */
  { "movl", Rd, Cd },
  { "movl", Rd, Dd },
  { "movl", Cd, Rd },
  { "movl", Dd, Rd },  
  { "movl", Rd, Td },
  { "(bad)" },
  { "movl", Td, Rd },
  { "(bad)" },  
  /* 28 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* 30 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* 38 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* 40 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* 48 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* 50 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* 58 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* 60 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* 68 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* 70 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* 78 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* 80 */
  { "jo", Jv },
  { "jno", Jv },
  { "jb", Jv },
  { "jae", Jv },  
  { "je", Jv },
  { "jne", Jv },
  { "jbe", Jv },
  { "ja", Jv },  
  /* 88 */
  { "js", Jv },
  { "jns", Jv },
  { "jp", Jv },
  { "jnp", Jv },  
  { "jl", Jv },
  { "jge", Jv },
  { "jle", Jv },
  { "jg", Jv },  
  /* 90 */
  { "seto", Eb },
  { "setno", Eb },
  { "setb", Eb },
  { "setae", Eb },
  { "sete", Eb },
  { "setne", Eb },
  { "setbe", Eb },
  { "seta", Eb },
  /* 98 */
  { "sets", Eb },
  { "setns", Eb },
  { "setp", Eb },
  { "setnp", Eb },
  { "setl", Eb },
  { "setge", Eb },
  { "setle", Eb },
  { "setg", Eb },  
  /* a0 */
  { "pushl", fs },
  { "popl", fs },
  { "(bad)" },
  { "btS", Ev, Gv },  
  { "shldS", Ev, Gv, Ib },
  { "shldS", Ev, Gv, CL },
  { "(bad)" },
  { "(bad)" },  
  /* a8 */
  { "pushl", gs },
  { "popl", gs },
  { "(bad)" },
  { "btsS", Ev, Gv },  
  { "shrdS", Ev, Gv, Ib },
  { "shrdS", Ev, Gv, CL },
  { "(bad)" },
  { "imulS", Gv, Ev },  
  /* b0 */
  { "(bad)" },
  { "(bad)" },
  { "lssS", Gv, Mp },	/* 386 lists only Mp */
  { "btrS", Ev, Gv },  
  { "lfsS", Gv, Mp },	/* 386 lists only Mp */
  { "lgsS", Gv, Mp },	/* 386 lists only Mp */
  { "movzbS", Gv, Eb },
  { "movzwS", Gv, Ew },  
  /* b8 */
  { "(bad)" },
  { "(bad)" },
  { GRP8 },
  { "btcS", Ev, Gv },  
  { "bsfS", Gv, Ev },
  { "bsrS", Gv, Ev },
  { "movsbS", Gv, Eb },
  { "movswS", Gv, Ew },  
  /* c0 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* c8 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* d0 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* d8 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* e0 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* e8 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* f0 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  /* f8 */
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
  { "(bad)" },  { "(bad)" },  { "(bad)" },  { "(bad)" },  
};

static char obuf[100];
static char *obufp;
static char scratchbuf[100];
static unsigned char *start_codep;
static unsigned char *codep;
static int mod;
static int rm;
static int reg;

static char *names32[]={
  "%eax","%ecx","%edx","%ebx", "%esp","%ebp","%esi","%edi",
};
static char *names16[] = {
  "%ax","%cx","%dx","%bx","%sp","%bp","%si","%di",
};
static char *names8[] = {
  "%al","%cl","%dl","%bl","%ah","%ch","%dh","%bh",
};
static char *names_seg[] = {
  "%es","%cs","%ss","%ds","%fs","%gs","%?","%?",
};

struct dis386 grps[][8] = {
  /* GRP1b */
  {
    { "addb",	Eb, Ib },
    { "orb",	Eb, Ib },
    { "adcb",	Eb, Ib },
    { "sbbb",	Eb, Ib },
    { "andb",	Eb, Ib },
    { "subb",	Eb, Ib },
    { "xorb",	Eb, Ib },
    { "cmpb",	Eb, Ib }
  },
  /* GRP1S */
  {
    { "addS",	Ev, Iv },
    { "orS",	Ev, Iv },
    { "adcS",	Ev, Iv },
    { "sbbS",	Ev, Iv },
    { "andS",	Ev, Iv },
    { "subS",	Ev, Iv },
    { "xorS",	Ev, Iv },
    { "cmpS",	Ev, Iv }
  },
  /* GRP1Ss */
  {
    { "addS",	Ev, sIb },
    { "orS",	Ev, sIb },
    { "adcS",	Ev, sIb },
    { "sbbS",	Ev, sIb },
    { "andS",	Ev, sIb },
    { "subS",	Ev, sIb },
    { "xorS",	Ev, sIb },
    { "cmpS",	Ev, sIb }
  },
  /* GRP2b */
  {
    { "rolb",	Eb, Ib },
    { "rorb",	Eb, Ib },
    { "rclb",	Eb, Ib },
    { "rcrb",	Eb, Ib },
    { "shlb",	Eb, Ib },
    { "shrb",	Eb, Ib },
    { "(bad)" },
    { "sarb",	Eb, Ib },
  },
  /* GRP2S */
  {
    { "rolS",	Ev, Ib },
    { "rorS",	Ev, Ib },
    { "rclS",	Ev, Ib },
    { "rcrS",	Ev, Ib },
    { "shlS",	Ev, Ib },
    { "shrS",	Ev, Ib },
    { "(bad)" },
    { "sarS",	Ev, Ib },
  },
  /* GRP2b_one */
  {
    { "rolb",	Eb },
    { "rorb",	Eb },
    { "rclb",	Eb },
    { "rcrb",	Eb },
    { "shlb",	Eb },
    { "shrb",	Eb },
    { "(bad)" },
    { "sarb",	Eb },
  },
  /* GRP2S_one */
  {
    { "rolS",	Ev },
    { "rorS",	Ev },
    { "rclS",	Ev },
    { "rcrS",	Ev },
    { "shlS",	Ev },
    { "shrS",	Ev },
    { "(bad)" },
    { "sarS",	Ev },
  },
  /* GRP2b_cl */
  {
    { "rolb",	Eb, CL },
    { "rorb",	Eb, CL },
    { "rclb",	Eb, CL },
    { "rcrb",	Eb, CL },
    { "shlb",	Eb, CL },
    { "shrb",	Eb, CL },
    { "(bad)" },
    { "sarb",	Eb, CL },
  },
  /* GRP2S_cl */
  {
    { "rolS",	Ev, CL },
    { "rorS",	Ev, CL },
    { "rclS",	Ev, CL },
    { "rcrS",	Ev, CL },
    { "shlS",	Ev, CL },
    { "shrS",	Ev, CL },
    { "(bad)" },
    { "sarS",	Ev, CL }
  },
  /* GRP3b */
  {
    { "testb",	Eb, Ib },
    { "(bad)",	Eb },
    { "notb",	Eb },
    { "negb",	Eb },
    { "mulb",	AL, Eb },
    { "imulb",	AL, Eb },
    { "divb",	AL, Eb },
    { "idivb",	AL, Eb }
  },
  /* GRP3S */
  {
    { "testS",	Ev, Iv },
    { "(bad)" },
    { "notS",	Ev },
    { "negS",	Ev },
    { "mulS",	eAX, Ev },
    { "imulS",	eAX, Ev },
    { "divS",	eAX, Ev },
    { "idivS",	eAX, Ev },
  },
  /* GRP4 */
  {
    { "incb", Eb },
    { "decb", Eb },
    { "(bad)" },
    { "(bad)" },
    { "(bad)" },
    { "(bad)" },
    { "(bad)" },
    { "(bad)" },
  },
  /* GRP5 */
  {
    { "incS",	Ev },
    { "decS",	Ev },
    { "call",	indirEv },
    { "lcall",	indirEv },
    { "jmp",	indirEv },
    { "ljmp",	indirEv },
    { "pushS",	Ev },
    { "(bad)" },
  },
  /* GRP6 */
  {
    { "sldt",	Ew },
    { "str",	Ew },
    { "lldt",	Ew },
    { "ltr",	Ew },
    { "verr",	Ew },
    { "verw",	Ew },
    { "(bad)" },
    { "(bad)" }
  },
  /* GRP7 */
  {
    { "sgdt", Ew },
    { "sidt", Ew },
    { "lgdt", Ew },
    { "lidt", Ew },
    { "smsw", Ew },
    { "(bad)" },
    { "lmsw", Ew },
    { "(bad)" },
  },
  /* GRP8 */
  {
    { "(bad)" },
    { "(bad)" },
    { "(bad)" },
    { "(bad)" },
    { "btS",	Ev, Ib },
    { "btsS",	Ev, Ib },
    { "btrS",	Ev, Ib },
    { "btcS",	Ev, Ib },
  }
};

#define PREFIX_REPZ 1
#define PREFIX_REPNZ 2
#define PREFIX_LOCK 4
#define PREFIX_CS 8
#define PREFIX_SS 0x10
#define PREFIX_DS 0x20
#define PREFIX_ES 0x40
#define PREFIX_FS 0x80
#define PREFIX_GS 0x100
#define PREFIX_DATA 0x200
#define PREFIX_ADR 0x400
#define PREFIX_FWAIT 0x800

static int prefixes;

ckprefix ()
{
  prefixes = 0;
  while (1)
    {
      switch (*codep)
	{
	case 0xf3:
	  prefixes |= PREFIX_REPZ;
	  break;
	case 0xf2:
	  prefixes |= PREFIX_REPNZ;
	  break;
	case 0xf0:
	  prefixes |= PREFIX_LOCK;
	  break;
	case 0x2e:
	  prefixes |= PREFIX_CS;
	  break;
	case 0x36:
	  prefixes |= PREFIX_SS;
	  break;
	case 0x3e:
	  prefixes |= PREFIX_DS;
	  break;
	case 0x26:
	  prefixes |= PREFIX_ES;
	  break;
	case 0x64:
	  prefixes |= PREFIX_FS;
	  break;
	case 0x65:
	  prefixes |= PREFIX_GS;
	  break;
	case 0x66:
	  prefixes |= PREFIX_DATA;
	  break;
	case 0x67:
	  prefixes |= PREFIX_ADR;
	  break;
	case 0x9b:
	  prefixes |= PREFIX_FWAIT;
	  break;
	default:
	  return;
	}
      codep++;
    }
}

static int dflag;
static int aflag;		

static char op1out[100], op2out[100], op3out[100];
static int start_pc;

/*
 * disassemble the first instruction in 'inbuf'.  You have to make
 *   sure all of the bytes of the instruction are filled in.
 *   On the 386's of 1988, the maximum length of an instruction is 15 bytes.
 *   (see topic "Redundant prefixes" in the "Differences from 8086"
 *   section of the "Virtual 8086 Mode" chapter.)
 * 'pc' should be the address of this instruction, it will
 *   be used to print the target address if this is a relative jump or call
 * 'outbuf' gets filled in with the disassembled instruction.  it should
 *   be long enough to hold the longest disassembled instruction.
 *   100 bytes is certainly enough, unless symbol printing is added later
 * The function returns the length of this instruction in bytes.
 */
i386dis (pc, inbuf, outbuf)
     int pc;
     unsigned char *inbuf;
     char *outbuf;
{
  struct dis386 *dp;
  char *p;
  int i;
  int enter_instruction;
  char *first, *second, *third;
  int needcomma;
  
  obuf[0] = 0;
  op1out[0] = 0;
  op2out[0] = 0;
  op3out[0] = 0;
  
  start_pc = pc;
  start_codep = inbuf;
  codep = inbuf;
  
  ckprefix ();
  
  if (*codep == 0xc8)
    enter_instruction = 1;
  else
    enter_instruction = 0;
  
  obufp = obuf;
  
  if (prefixes & PREFIX_REPZ)
    oappend ("repz ");
  if (prefixes & PREFIX_REPNZ)
    oappend ("repnz ");
  if (prefixes & PREFIX_LOCK)
    oappend ("lock ");
  
  if ((prefixes & PREFIX_FWAIT)
      && ((*codep < 0xd8) || (*codep > 0xdf)))
    {
      /* fwait not followed by floating point instruction */
      oappend ("fwait");
      strcpy (outbuf, obuf);
      return (1);
    }
  
  /* these would be initialized to 0 if disassembling for 8086 or 286 */
  dflag = 1;
  aflag = 1;
  
  if (prefixes & PREFIX_DATA)
    dflag ^= 1;
  
  if (prefixes & PREFIX_ADR)
    {
      aflag ^= 1;
      oappend ("addr16 ");
    }
  
  if (*codep == 0x0f)
    dp = &dis386_twobyte[*++codep];
  else
    dp = &dis386[*codep];
  codep++;
  mod = (*codep >> 6) & 3;
  reg = (*codep >> 3) & 7;
  rm = *codep & 7;
  
  if (dp->name == NULL && dp->bytemode1 == FLOATCODE)
    {
      dofloat ();
    }
  else
    {
      if (dp->name == NULL)
	dp = &grps[dp->bytemode1][reg];
      
      putop (dp->name);
      
      obufp = op1out;
      if (dp->op1)
	(*dp->op1)(dp->bytemode1);
      
      obufp = op2out;
      if (dp->op2)
	(*dp->op2)(dp->bytemode2);
      
      obufp = op3out;
      if (dp->op3)
	(*dp->op3)(dp->bytemode3);
    }
  
  obufp = obuf + strlen (obuf);
  for (i = strlen (obuf); i < 6; i++)
    oappend (" ");
  oappend (" ");
  
  /* enter instruction is printed with operands in the
   * same order as the intel book; everything else
   * is printed in reverse order 
   */
  if (enter_instruction)
    {
      first = op1out;
      second = op2out;
      third = op3out;
    }
  else
    {
      first = op3out;
      second = op2out;
      third = op1out;
    }
  needcomma = 0;
  if (*first)
    {
      oappend (first);
      needcomma = 1;
    }
  if (*second)
    {
      if (needcomma)
	oappend (",");
      oappend (second);
      needcomma = 1;
    }
  if (*third)
    {
      if (needcomma)
	oappend (",");
      oappend (third);
    }
  strcpy (outbuf, obuf);
  return (codep - inbuf);
}

char *float_mem[] = {
  /* d8 */
  "fadds",
  "fmuls",
  "fcoms",
  "fcomps",
  "fsubs",
  "fsubrs",
  "fdivs",
  "fdivrs",
  /*  d9 */
  "flds",
  "(bad)",
  "fsts",
  "fstps",
  "fldenv",
  "fldcw",
  "fNstenv",
  "fNstcw",
  /* da */
  "fiaddl",
  "fimull",
  "ficoml",
  "ficompl",
  "fisubl",
  "fisubrl",
  "fidivl",
  "fidivrl",
  /* db */
  "fildl",
  "(bad)",
  "fistl",
  "fistpl",
  "(bad)",
  "fldt",
  "(bad)",
  "fstpt",
  /* dc */
  "faddl",
  "fmull",
  "fcoml",
  "fcompl",
  "fsubl",
  "fsubrl",
  "fdivl",
  "fdivrl",
  /* dd */
  "fldl",
  "(bad)",
  "fstl",
  "fstpl",
  "frstor",
  "(bad)",
  "fNsave",
  "fNstsw",
  /* de */
  "fiadd",
  "fimul",
  "ficom",
  "ficomp",
  "fisub",
  "fisubr",
  "fidiv",
  "fidivr",
  /* df */
  "fild",
  "(bad)",
  "fist",
  "fistp",
  "fbld",
  "fildll",
  "fbstp",
  "fistpll",
};

#define ST OP_ST, 0
#define STi OP_STi, 0
int OP_ST(), OP_STi();

#define FGRPd9_2 NULL, NULL, 0
#define FGRPd9_4 NULL, NULL, 1
#define FGRPd9_5 NULL, NULL, 2
#define FGRPd9_6 NULL, NULL, 3
#define FGRPd9_7 NULL, NULL, 4
#define FGRPda_5 NULL, NULL, 5
#define FGRPdb_4 NULL, NULL, 6
#define FGRPde_3 NULL, NULL, 7
#define FGRPdf_4 NULL, NULL, 8

struct dis386 float_reg[][8] = {
  /* d8 */
  {
    { "fadd",	ST, STi },
    { "fmul",	ST, STi },
    { "fcom",	STi },
    { "fcomp",	STi },
    { "fsub",	ST, STi },
    { "fsubr",	ST, STi },
    { "fdiv",	ST, STi },
    { "fdivr",	ST, STi },
  },
  /* d9 */
  {
    { "fld",	STi },
    { "fxch",	STi },
    { FGRPd9_2 },
    { "(bad)" },
    { FGRPd9_4 },
    { FGRPd9_5 },
    { FGRPd9_6 },
    { FGRPd9_7 },
  },
  /* da */
  {
    { "(bad)" },
    { "(bad)" },
    { "(bad)" },
    { "(bad)" },
    { "(bad)" },
    { FGRPda_5 },
    { "(bad)" },
    { "(bad)" },
  },
  /* db */
  {
    { "(bad)" },
    { "(bad)" },
    { "(bad)" },
    { "(bad)" },
    { FGRPdb_4 },
    { "(bad)" },
    { "(bad)" },
    { "(bad)" },
  },
  /* dc */
  {
    { "fadd",	STi, ST },
    { "fmul",	STi, ST },
    { "(bad)" },
    { "(bad)" },
    { "fsub",	STi, ST },
    { "fsubr",	STi, ST },
    { "fdiv",	STi, ST },
    { "fdivr",	STi, ST },
  },
  /* dd */
  {
    { "ffree",	STi },
    { "(bad)" },
    { "fst",	STi },
    { "fstp",	STi },
    { "fucom",	STi },
    { "fucomp",	STi },
    { "(bad)" },
    { "(bad)" },
  },
  /* de */
  {
    { "faddp",	STi, ST },
    { "fmulp",	STi, ST },
    { "(bad)" },
    { FGRPde_3 },
    { "fsubp",	STi, ST },
    { "fsubrp",	STi, ST },
    { "fdivp",	STi, ST },
    { "fdivrp",	STi, ST },
  },
  /* df */
  {
    { "(bad)" },
    { "(bad)" },
    { "(bad)" },
    { "(bad)" },
    { FGRPdf_4 },
    { "(bad)" },
    { "(bad)" },
    { "(bad)" },
  },
};


char *fgrps[][8] = {
  /* d9_2  0 */
  {
    "fnop","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
  },

  /* d9_4  1 */
  {
    "fchs","fabs","(bad)","(bad)","ftst","fxam","(bad)","(bad)",
  },

  /* d9_5  2 */
  {
    "fld1","fldl2t","fldl2e","fldpi","fldlg2","fldln2","fldz","(bad)",
  },

  /* d9_6  3 */
  {
    "f2xm1","fyl2x","fptan","fpatan","fxtract","fprem1","fdecstp","fincstp",
  },

  /* d9_7  4 */
  {
    "fprem","fyl2xp1","fsqrt","fsincos","frndint","fscale","fsin","fcos",
  },

  /* da_5  5 */
  {
    "(bad)","fucompp","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
  },

  /* db_4  6 */
  {
    "feni(287 only)","fdisi(287 only)","fNclex","fNinit",
    "fNsetpm(287 only)","(bad)","(bad)","(bad)",
  },

  /* de_3  7 */
  {
    "(bad)","fcompp","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
  },

  /* df_4  8 */
  {
    "fNstsw","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
  },
};


dofloat ()
{
  struct dis386 *dp;
  unsigned char floatop;
  
  floatop = codep[-1];
  
  if (mod != 3)
    {
      putop (float_mem[(floatop - 0xd8) * 8 + reg]);
      obufp = op1out;
      OP_E (v_mode);
      return;
    }
  codep++;
  
  dp = &float_reg[floatop - 0xd8][reg];
  if (dp->name == NULL)
    {
      putop (fgrps[dp->bytemode1][rm]);
      /* instruction fnstsw is only one with strange arg */
      if (floatop == 0xdf && *codep == 0xe0)
	strcpy (op1out, "%eax");
    }
  else
    {
      putop (dp->name);
      obufp = op1out;
      if (dp->op1)
	(*dp->op1)(dp->bytemode1);
      obufp = op2out;
      if (dp->op2)
	(*dp->op2)(dp->bytemode2);
    }
}

/* ARGSUSED */
OP_ST (ignore)
{
  oappend ("%st");
}

/* ARGSUSED */
OP_STi (ignore)
{
  sprintf (scratchbuf, "%%st(%d)", rm);
  oappend (scratchbuf);
}


/* capital letters in template are macros */
putop (template)
     char *template;
{
  char *p;
  
  for (p = template; *p; p++)
    {
      switch (*p)
	{
	default:
	  *obufp++ = *p;
	  break;
	case 'C':		/* For jcxz/jecxz */
	  if (aflag == 0)
	    *obufp++ = 'e';
	  break;
	case 'N':
	  if ((prefixes & PREFIX_FWAIT) == 0)
	    *obufp++ = 'n';
	  break;
	case 'S':
	  /* operand size flag */
	  if (dflag)
	    *obufp++ = 'l';
	  else
	    *obufp++ = 'w';
	  break;
	}
    }
  *obufp = 0;
}

oappend (s)
char *s;
{
  strcpy (obufp, s);
  obufp += strlen (s);
  *obufp = 0;
}

append_prefix ()
{
  if (prefixes & PREFIX_CS)
    oappend ("%cs:");
  if (prefixes & PREFIX_DS)
    oappend ("%ds:");
  if (prefixes & PREFIX_SS)
    oappend ("%ss:");
  if (prefixes & PREFIX_ES)
    oappend ("%es:");
  if (prefixes & PREFIX_FS)
    oappend ("%fs:");
  if (prefixes & PREFIX_GS)
    oappend ("%gs:");
}

OP_indirE (bytemode)
{
  oappend ("*");
  OP_E (bytemode);
}

OP_E (bytemode)
{
  int disp;
  int havesib;
  int didoutput = 0;
  int base;
  int index;
  int scale;
  int havebase;
  
  /* skip mod/rm byte */
  codep++;
  
  havesib = 0;
  havebase = 0;
  disp = 0;
  
  if (mod == 3)
    {
      switch (bytemode)
	{
	case b_mode:
	  oappend (names8[rm]);
	  break;
	case w_mode:
	  oappend (names16[rm]);
	  break;
	case v_mode:
	  if (dflag)
	    oappend (names32[rm]);
	  else
	    oappend (names16[rm]);
	  break;
	default:
	  oappend ("<bad dis table>");
	  break;
	}
      return;
    }
  
  append_prefix ();
  if (rm == 4)
    {
      havesib = 1;
      havebase = 1;
      scale = (*codep >> 6) & 3;
      index = (*codep >> 3) & 7;
      base = *codep & 7;
      codep++;
    }
  
  switch (mod)
    {
    case 0:
      switch (rm)
	{
	case 4:
	  /* implies havesib and havebase */
	  if (base == 5) {
	    havebase = 0;
	    disp = get32 ();
	  }
	  break;
	case 5:
	  disp = get32 ();
	  break;
	default:
	  havebase = 1;
	  base = rm;
	  break;
	}
      break;
    case 1:
      disp = *(char *)codep++;
      if (rm != 4)
	{
	  havebase = 1;
	  base = rm;
	}
      break;
    case 2:
      disp = get32 ();
      if (rm != 4)
	{
	  havebase = 1;
	  base = rm;
	}
      break;
    }
  
  if (mod != 0 || rm == 5 || (havesib && base == 5))
    {
      sprintf (scratchbuf, "%d", disp);
      oappend (scratchbuf);
    }
  
  if (havebase || havesib) 
    {
      oappend ("(");
      if (havebase)
	oappend (names32[base]);
      if (havesib) 
	{
	  if (index != 4) 
	    {
	      sprintf (scratchbuf, ",%s", names32[index]);
	      oappend (scratchbuf);
	    }
	  sprintf (scratchbuf, ",%d", 1 << scale);
	  oappend (scratchbuf);
	}
      oappend (")");
    }
}

OP_G (bytemode)
{
  switch (bytemode) 
    {
    case b_mode:
      oappend (names8[reg]);
      break;
    case w_mode:
      oappend (names16[reg]);
      break;
    case d_mode:
      oappend (names32[reg]);
      break;
    case v_mode:
      if (dflag)
	oappend (names32[reg]);
      else
	oappend (names16[reg]);
      break;
    default:
      oappend ("<internal disassembler error>");
      break;
    }
}

get32 ()
{
  int x = 0;
  
  x = *codep++ & 0xff;
  x |= (*codep++ & 0xff) << 8;
  x |= (*codep++ & 0xff) << 16;
  x |= (*codep++ & 0xff) << 24;
  return (x);
}

get16 ()
{
  int x = 0;
  
  x = *codep++ & 0xff;
  x |= (*codep++ & 0xff) << 8;
  return (x);
}

OP_REG (code)
{
  char *s;
  
  switch (code) 
    {
    case indir_dx_reg: s = "(%dx)"; break;
	case ax_reg: case cx_reg: case dx_reg: case bx_reg:
	case sp_reg: case bp_reg: case si_reg: case di_reg:
		s = names16[code - ax_reg];
		break;
	case es_reg: case ss_reg: case cs_reg:
	case ds_reg: case fs_reg: case gs_reg:
		s = names_seg[code - es_reg];
		break;
	case al_reg: case ah_reg: case cl_reg: case ch_reg:
	case dl_reg: case dh_reg: case bl_reg: case bh_reg:
		s = names8[code - al_reg];
		break;
	case eAX_reg: case eCX_reg: case eDX_reg: case eBX_reg:
	case eSP_reg: case eBP_reg: case eSI_reg: case eDI_reg:
      if (dflag)
	s = names32[code - eAX_reg];
      else
	s = names16[code - eAX_reg];
      break;
    default:
      s = "<internal disassembler error>";
      break;
    }
  oappend (s);
}

OP_I (bytemode)
{
  int op;
  
  switch (bytemode) 
    {
    case b_mode:
      op = *codep++ & 0xff;
      break;
    case v_mode:
      if (dflag)
	op = get32 ();
      else
	op = get16 ();
      break;
    case w_mode:
      op = get16 ();
      break;
    default:
      oappend ("<internal disassembler error>");
      return;
    }
  sprintf (scratchbuf, "$0x%x", op);
  oappend (scratchbuf);
}

OP_sI (bytemode)
{
  int op;
  
  switch (bytemode) 
    {
    case b_mode:
      op = *(char *)codep++;
      break;
    case v_mode:
      if (dflag)
	op = get32 ();
      else
	op = (short)get16();
      break;
    case w_mode:
      op = (short)get16 ();
      break;
    default:
      oappend ("<internal disassembler error>");
      return;
    }
  sprintf (scratchbuf, "$0x%x", op);
  oappend (scratchbuf);
}

OP_J (bytemode)
{
  int disp;
  int mask = -1;
  
  switch (bytemode) 
    {
    case b_mode:
      disp = *(char *)codep++;
      break;
    case v_mode:
      if (dflag)
	disp = get32 ();
      else
	{
	  disp = (short)get16 ();
	  /* for some reason, a data16 prefix on a jump instruction
	     means that the pc is masked to 16 bits after the
	     displacement is added!  */
	  mask = 0xffff;
	}
      break;
    default:
      oappend ("<internal disassembelr error>");
      return;
    }
  
  sprintf (scratchbuf, "0x%x",
	   (start_pc + codep - start_codep + disp) & mask);
  oappend (scratchbuf);
}

/* ARGSUSED */
OP_SEG (dummy)
{
  static char *sreg[] = {
    "%es","%cs","%ss","%ds","%fs","%gs","%?","%?",
  };

  oappend (sreg[reg]);
}

OP_DIR (size)
{
  int seg, offset;
  
  switch (size) 
    {
    case lptr:
      if (aflag) 
	{
	  offset = get32 ();
	  seg = get16 ();
	} 
      else 
	{
	  offset = get16 ();
	  seg = get16 ();
	}
      sprintf (scratchbuf, "0x%x,0x%x", seg, offset);
      oappend (scratchbuf);
      break;
    case v_mode:
      if (aflag)
	offset = get32 ();
      else
	offset = (short)get16 ();
      
      sprintf (scratchbuf, "0x%x",
	       start_pc + codep - start_codep + offset);
      oappend (scratchbuf);
      break;
    default:
      oappend ("<internal disassembler error>");
      break;
    }
}

/* ARGSUSED */
OP_OFF (bytemode)
{
  int off;
  
  if (aflag)
    off = get32 ();
  else
    off = get16 ();
  
  sprintf (scratchbuf, "0x%x", off);
  oappend (scratchbuf);
}

/* ARGSUSED */
OP_ESDI (dummy)
{
  oappend ("%es:(");
  oappend (aflag ? "%edi" : "%di");
  oappend (")");
}

/* ARGSUSED */
OP_DSSI (dummy)
{
  oappend ("%ds:(");
  oappend (aflag ? "%esi" : "%si");
  oappend (")");
}

/* ARGSUSED */
OP_ONE (dummy)
{
  oappend ("1");
}

/* ARGSUSED */
OP_C (dummy)
{
  codep++; /* skip mod/rm */
  sprintf (scratchbuf, "%%cr%d", reg);
  oappend (scratchbuf);
}

/* ARGSUSED */
OP_D (dummy)
{
  codep++; /* skip mod/rm */
  sprintf (scratchbuf, "%%db%d", reg);
  oappend (scratchbuf);
}

/* ARGSUSED */
OP_T (dummy)
{
  codep++; /* skip mod/rm */
  sprintf (scratchbuf, "%%tr%d", reg);
  oappend (scratchbuf);
}

OP_rm (bytemode)
{
  switch (bytemode) 
    {
    case d_mode:
      oappend (names32[rm]);
      break;
    case w_mode:
      oappend (names16[rm]);
      break;
    }
}
	
/* GDB interface */
#include "defs.h"
#include "param.h"
#include "symtab.h"
#include "frame.h"
#include "inferior.h"

#define MAXLEN 20
print_insn (memaddr, stream)
     CORE_ADDR memaddr;
     FILE *stream;
{
  unsigned char buffer[MAXLEN];
  /* should be expanded if disassembler prints symbol names */
  char outbuf[100];
  int n;
  
  read_memory (memaddr, buffer, MAXLEN);
  
  n = i386dis ((int)memaddr, buffer, outbuf);
  
  fputs (outbuf, stream);
  
  return (n);
}

