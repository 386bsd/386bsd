# 1 "/arch/odysseus/wdbootblk-x.c"



















































# 1 "../..//i386/isa/isa.h" 1


























































		












		
					



					




					



					



					




					






					




































# 52 "/arch/odysseus/wdbootblk-x.c" 2

# 1 "../..//i386/isa/wdreg.h" 1


































































































# 143 "../..//i386/isa/wdreg.h"

# 53 "/arch/odysseus/wdbootblk-x.c" 2





	
	
	cli
	.byte 0xb8,0x30,0x00	
	mov %ax, %ss
	.byte 0xbc,0x00,0x01	

	xorl	%eax,%eax
	movl	%ax,%ds
	movl	%ax,%es
# 91 "/arch/odysseus/wdbootblk-x.c"

	

	.byte 0x3E,0x0F,1,0x16



	.word	0x7c00+0x4a	#GDTptr

	# word aword cs lgdt GDTptr

	

	smsw	%ax
	orb	$1,%al
	lmsw	%ax
	jmp	1f
	nop

	

1:
	xorl	%eax,%eax
	movb	$0x10,%al
	movl	%ax,%ds
	movl	%ax,%es
	movl	%ax,%ss
	word



	ljmp	$0x8,$ 0x7c00+0x50	

 





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




GDTptr:	.word	0x17	# limit to three 8 byte selectors(null,code,data)



	.long 	0x7c00+0x32	# GDT -- arrgh, gas again!


	
reloc:
	movl	$ 0x7c00,%esi
	movl	$ 0x90000,%edi
	movl	$512,%ecx
	rep
	movsb
 	movl	$0xa0000, %esp
	pushl	$dodisk
	ret

	
dodisk:
	movl	$ 	0x1f0		+0x2		,%edx
	movb	$ 15,%al
	outb	%al,%dx
	inb $0x84,%al
	movl	$ 	0x1f0		+0x3		,%edx
	movb	$ 2,%al
	outb	%al,%dx
	inb $0x84,%al
	#outb(wdc+0x4		, (cyloffset & 0xff));
	#outb(wdc+0x5		, (cyloffset >> 8));
	#outb(wdc+	0x6		, 0xa0		 | (unit << 4));

	movl	$ 	0x1f0		+0x7		,%edx
	movb	$ 0x20		,%al
	outb	%al,%dx
	inb $0x84,%al
	cld

	
readblk:
	movl	$ 	0x1f0		+0x7				,%edx
	inb $0x84,%al
	inb	%dx,%al
	testb	$ 0x80		,%al
	jnz readblk
	testb	$ 0x08		,%al
	jz readblk

	

	movl	$ 	0x1f0		+	0x0		,%edx
	movl	$ 256,%ecx
	.byte 0x66,0xf2,0x6d	# rep insw
	inb $0x84,%al

	

	cmpl	$ 0x90000+16*512-1,%edi
	jl	readblk

	
	
	movl	$ 	0x1f0		+0x4		,%edx
	inb	%dx,%al
	xorl	%ecx,%ecx
	movb	%al,%cl
	incl	%edx
	inb $0x84,%al
	inb	%dx,%al		
	movb	%al,%ch
	pushl	%ecx		

	incl	%edx
	xorl	%eax,%eax
	inb $0x84,%al
	inb	%dx,%al		
	andb	$0x10,%al	
	shrb	$4,%al
	pushl	%eax		

	
	xorl	%eax,%eax
	pushl	%eax		

	

	movl	$ 	0x90000+0x400, %eax
	call	%eax 

ebootblkcode:

	
	
	.org	0x1fe
	.word	0xaa55		

ebootblk: 			

