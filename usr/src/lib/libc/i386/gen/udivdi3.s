.text
.globl ___udivdi3
___udivdi3:
	pushl %ebp
	movl %esp,%ebp
	pushl $0
	pushl 20(%ebp)
	pushl 16(%ebp)
	pushl 12(%ebp)
	pushl 8(%ebp)
	call ___udivmoddi4
	leave
	ret
