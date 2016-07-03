/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)fdbootblk.c	7.2 (Berkeley) 5/4/91
 */

/*
 * fdbootblk.s:
 *	Written 10/6/90 by William F. Jolitz
 *	Initial block boot for AT/386 with typical stupid NEC controller
 *
 *	Goal is to read in sucessive 7.5Kbytes of bootstrap to
 *	execute.
 *
 *	No attempt is made to handle disk errors.
 */
/*#include "/sys/i386/isa/isa.h"
#include "/sys/i386/isa/fdreg.h"*/
#define	NOP	pushl %eax ; inb $0x84, %al ; inb $0x84, %al ; popl %eax
#define BIOSRELOC	0x7c00
#define	COLORFB	0xb8000
#define	MONOFB	0xb0000
#define	FB	COLORFB
#define	start	RELOC+0x400
#if	DEBUG > 0
#define PUTC(s)	movw $ s ,(%ebp) ; incl %ebp ; incl %ebp
#else
#define PUTC(s)
#endif

	/* mumbo-jumbo to pacify diskcopy */
	jmp 1f
	.asciz "386BSD "
	.byte 1			# sectors per allocation
	.word 15		# additional sectors for bootstrap
	.word 0			# number of DOS fat sectors
	.word 0			# number of DOS rootdir entries
#ifdef	F3
	.byte 0xf0		# media descriptor
#else
	.byte 0xf9		# media descriptor
#endif
	.word 0			# number of sectors per a DOS fat entry
#ifdef	F3
	.word 18		# number of sectors per track
#else
	.word 15		# number of sectors per track
#endif
	.word 2			# number of heads
	.long 0			# number of hidden sectors
#ifdef	F3
	.long 2880-18		# logical sectors per volume
#else
	.long 2400-15		# logical sectors per volume
#endif
	.byte 0			# physical drive
	.byte 0x29		# ?
	.long 137		# binary id
	.ascii "Release 1.0"	# volume label
	.space 5
1:
	/* step 0 force descriptors to bottom of address space */
	
	cli
	.byte 0xb8,0x30,0x00	/* mov $0x30,%ax */
	mov %ax, %ss
	.byte 0xbc,0x00,0x01	/* mov $0x100,%sp */

	xorl	%eax, %eax
	movl	%ax, %ds
	movl	%ax, %es

	/* obtain BIOS parameters for hard disk XXX */
	movb	$0x9f, %ah	 /* write to 0x9ff00  XXX */
	movb	$0xf0, %al
	mov	%ax, %es
	xor	%edi, %edi

	.byte 0xf, 0xb4, 0x36 ; .word  0x41*4	/* lfs 0x41*4, %si */
	xorb	%ch, %ch
	movb	$0x10, %cl
	fs
	rep
	movsb

	.byte 0xf, 0xb4, 0x36 ; .word  0x46*4	/* lfs 0x46*4, %si */
	xorb	%ch, %ch
	movb	$0x10, %cl
	fs
	rep
	movsb

 	xorl	%eax, %eax
	movl	%ax, %es

	/* step 1 load new descriptor table */

	.byte 0x2E, 0x0F, 1, 0x16 /* word aword cs lgdt GDTptr */
	.word	BIOSRELOC+0xa4	#GDTptr

	/* step 2 turn on protected mode */

	smsw	%ax
	orb	$1, %al
	lmsw	%ax
	jmp	1f
	nop

	/* step 3  reload segment descriptors */

 1:
	xorl	%eax, %eax
	movb	$0x10, %al
	movl	%ax, %ds
	movl	%ax, %es
	movl	%ax, %ss
	word
	ljmp	$0x8, $ BIOSRELOC+0xb3	/* would be nice if .-RELOC+0x7c00 worked */

 /* Global Descriptor Table contains three descriptors:
  * 0x00: Null: not used
  * 0x08: Code: code segment starts at 0 and extents for 4 gigabytes
  * 0x10: Data: data segment starts at 0 and extends for 4 gigabytes
  *		(overlays code)
  */
GDT:
NullDesc:	.word	0,0,0,0	# null descriptor - not used
CodeDesc:	.word	0xFFFF	# limit at maximum: (bits 15:0)
	.byte	0,0,0	# base at 0: (bits 23:0)
	.byte	0x9f	# present/priv level 0/code/conforming/readable
	.byte	0xcf	# page granular/default 32-bit/limit(bits 19:16)
	.byte	0	# base at 0: (bits 31:24)
DataDesc:	.word	0xFFFF	# limit at maximum: (bits 15:0)
	.byte	0,0,0	# base at 0: (bits 23:0)
	.byte	0x93	# present/priv level 0/data/expand-up/writeable
	.byte	0xcf	# page granular/default 32-bit/limit(bits 19:16)
	.byte	0	# base at 0: (bits 31:24)

/* Global Descriptor Table pointer
 *  contains 6-byte pointer information for LGDT
 */
GDTptr:	.word	0x17	# limit to three 8 byte selectors(null,code,data)
	.long 	BIOSRELOC+0x8c	# GDT -- arrgh, gas again!
readcmd: .byte 0xe6,0,0,0,0,2,18,0x1b,0xff

	/* step 4 relocate to final bootstrap address. */
reloc:
	movl	$ BIOSRELOC, %esi
	movl	$ RELOC, %edi
	movl	$512, %ecx
	rep
	movsb
	movl	$0xa0000, %esp
	pushl	$dodisk
	ret

	/* step 5 load remaining 15 sectors off disk */
dodisk:
#if	DEBUG > 0
	movl	$ FB, %ebp
#endif
	PUTC(0x7030)
	movl	$ RELOC+0x200, %edi

	movb	$2, %bx
 8:
	movb	%bl, readcmd+4
	movl	%edi, %ecx

	/* Set read/write bytes */
	xorl	%edx, %edx
	movb	$0x0c, %dl	# outb(0xC,0x46); outb(0xB,0x46);
	movb	$0x46, %al
	outb	%al, %dx
	NOP
	jmp	1f
1:	decb	%dx
	outb	%al, %dx
	NOP

	/* Send start address */
	movb	$0x04, %dl	# outb(0x4, addr);
	movb	%cl, %al
	outb	%al, %dx
	NOP
	jmp	1f
1:	movb	%ch, %al		# outb(0x4, addr>>8);
	outb	%al, %dx
	NOP
	rorl	$8, %ecx		# outb(0x81, addr>>16);
	movb	%ch, %al
	outb	%al, $0x81
	NOP

	/* Send count - (512 - 1) */
	movb	$0x05, %dl	# outb(0x5, 0xff);
	movb	$0xff, %al
	outb	%al, %dx
	NOP
	movb	$1, %al		# outb(0x5, 1);
	outb	%al, %dx
	NOP

	/* set channel 2 */
	movb	$2, %al		# outb(0x0A,2);
	outb	%al, $0x0A
	NOP

	/* issue read command to fdc */
	movw	$0x3f4, %dx
	movl	$readcmd, %esi
	xorl	%ecx, %ecx
	movb	$9, %cl

 2:	NOP
	inb	%dx, %al
	testb	$0x80, %al
	jz 2b

	incb	%dx
	NOP
	movl	(%esi), %al
	outb	%al, %dx
	NOP
	incl	%esi
	decb	%dx
	loop	 2b
	PUTC(0x7031)

	/* extract the status bytes after the read. */
	movl	$0x3f4, %edx
	xorl	%ecx, %ecx
	movb	$7, %cl
 2:
	NOP
	inb	%dx, %al
	andb	$0xc0, %al
	cmpb	$0xc0, %al
	jne	2b
	incb	%dx
	inb	%dx, %al
	decb	%dx
	loop	2b

	PUTC(0x7032)
	addw	$0x200, %edi	# next addr to load to
	incb	%bl
	cmpb	$15, %bl
	jle	8b
	
	PUTC(0x7033)
	/* for clever bootstrap, dig out boot unit and cylinder */
	pushl	$0
	pushl	$0
	
	/* fd controller is major device 2 */
	pushl	$2	/* dev */

	/* sorry, no flags at this point! */

	movl	$ start, %eax
	call	%eax	/* main (dev, unit, off) */

ebootblkcode:

	/* remaining space usable for a partition table */
	
	.org	0x1fe
	.word	0xaa55		/* signature -- used by BIOS ROM */

ebootblk: 			/* MUST BE EXACTLY 0x200 BIG FOR SURE */
