#NO_APP
gcc_compiled.:
.text
	.align 2
.globl _ldexp
_ldexp:
	pushl %ebp
	movl %esp,%ebp
	subl $24,%esp
	fildl 16(%ebp)
	fstpl -16(%ebp)
	fldl -16(%ebp)
	fldl 8(%ebp)
#APP
	fscale ; fxch %st(1) ; fstpl -24(%ebp) 
#NO_APP
	fstpl -8(%ebp)
	fldl -8(%ebp)
	jmp L1
L1:
	leave
	ret
