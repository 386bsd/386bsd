	.file	"m.c"
gcc2_compiled.:
___gnu_compiled_c:
.text
	.align 2
.globl _m
_m:
	pushl %ebp
	movl %esp,%ebp
	movl 12(%ebp),%ecx
	movl 8(%ebp),%eax
	sarl %cl,%eax
	testb $1,%al
	je L2
	call _c
L2:
	leave
	ret
