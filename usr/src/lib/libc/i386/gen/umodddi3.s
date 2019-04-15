.text
.globl ___umoddi3
___umoddi3:
	pushl %ebp
	movl %esp,%ebp
	subl $8,%esp
	leal -8(%ebp),%eax
	pushl %eax
	pushl 20(%ebp)
	pushl 16(%ebp)
	pushl 12(%ebp)
	pushl 8(%ebp)
	call ___udivmoddi4
	movl -8(%ebp),%eax
	movl -4(%ebp),%edx
	leave
	ret
