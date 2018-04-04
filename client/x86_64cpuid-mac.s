.private_extern	_xOPENSSL_ia32cap_P
.comm	_xOPENSSL_ia32cap_P,16,2

.text	


.globl	_xOPENSSL_ia32_cpuid

.p2align	4
_xOPENSSL_ia32_cpuid:
	movq	%rbx,%r8

	xorl	%eax,%eax
	movl	%eax,8(%rdi)
	cpuid
	movl	%eax,%r11d

	xorl	%eax,%eax
	cmpl	$0x756e6547,%ebx
	setne	%al
	movl	%eax,%r9d
	cmpl	$0x49656e69,%edx
	setne	%al
	orl	%eax,%r9d
	cmpl	$0x6c65746e,%ecx
	setne	%al
	orl	%eax,%r9d
	jz	L$intel

	cmpl	$0x68747541,%ebx
	setne	%al
	movl	%eax,%r10d
	cmpl	$0x69746E65,%edx
	setne	%al
	orl	%eax,%r10d
	cmpl	$0x444D4163,%ecx
	setne	%al
	orl	%eax,%r10d
	jnz	L$intel


	movl	$0x80000000,%eax
	cpuid
	cmpl	$0x80000001,%eax
	jb	L$intel
	movl	%eax,%r10d
	movl	$0x80000001,%eax
	cpuid
	orl	%ecx,%r9d
	andl	$0x00000801,%r9d

	cmpl	$0x80000008,%r10d
	jb	L$intel

	movl	$0x80000008,%eax
	cpuid
	movzbq	%cl,%r10
	incq	%r10

	movl	$1,%eax
	cpuid
	btl	$28,%edx
	jnc	L$generic
	shrl	$16,%ebx
	cmpb	%r10b,%bl
	ja	L$generic
	andl	$0xefffffff,%edx
	jmp	L$generic

L$intel:
	cmpl	$4,%r11d
	movl	$-1,%r10d
	jb	L$nocacheinfo

	movl	$4,%eax
	movl	$0,%ecx
	cpuid
	movl	%eax,%r10d
	shrl	$14,%r10d
	andl	$0xfff,%r10d

L$nocacheinfo:
	movl	$1,%eax
	cpuid
	andl	$0xbfefffff,%edx
	cmpl	$0,%r9d
	jne	L$notintel
	orl	$0x40000000,%edx
	andb	$15,%ah
	cmpb	$15,%ah
	jne	L$notP4
	orl	$0x00100000,%edx
L$notP4:
	cmpb	$6,%ah
	jne	L$notintel
	andl	$0x0fff0ff0,%eax
	cmpl	$0x00050670,%eax
	je	L$knights
	cmpl	$0x00080650,%eax
	jne	L$notintel
L$knights:
	andl	$0xfbffffff,%ecx

L$notintel:
	btl	$28,%edx
	jnc	L$generic
	andl	$0xefffffff,%edx
	cmpl	$0,%r10d
	je	L$generic

	orl	$0x10000000,%edx
	shrl	$16,%ebx
	cmpb	$1,%bl
	ja	L$generic
	andl	$0xefffffff,%edx
L$generic:
	andl	$0x00000800,%r9d
	andl	$0xfffff7ff,%ecx
	orl	%ecx,%r9d

	movl	%edx,%r10d

	cmpl	$7,%r11d
	jb	L$no_extended_info
	movl	$7,%eax
	xorl	%ecx,%ecx
	cpuid
	btl	$26,%r9d
	jc	L$notknights
	andl	$0xfff7ffff,%ebx
L$notknights:
	movl	%ebx,8(%rdi)
L$no_extended_info:

	btl	$27,%r9d
	jnc	L$clear_avx
	xorl	%ecx,%ecx
.byte	0x0f,0x01,0xd0
	andl	$6,%eax
	cmpl	$6,%eax
	je	L$done
L$clear_avx:
	movl	$0xefffe7ff,%eax
	andl	%eax,%r9d
	andl	$0xffffffdf,8(%rdi)
L$done:
	shlq	$32,%r9
	movl	%r10d,%eax
	movq	%r8,%rbx
	orq	%r9,%rax
	.byte	0xf3,0xc3
