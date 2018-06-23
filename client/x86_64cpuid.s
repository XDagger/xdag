
.hidden	xOPENSSL_ia32cap_P
.comm	xOPENSSL_ia32cap_P,16,4

.text	

.globl	xOPENSSL_ia32_cpuid
.type	xOPENSSL_ia32_cpuid,@function
.align	16
xOPENSSL_ia32_cpuid:
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
	jz	.Lintel

	cmpl	$0x68747541,%ebx
	setne	%al
	movl	%eax,%r10d
	cmpl	$0x69746E65,%edx
	setne	%al
	orl	%eax,%r10d
	cmpl	$0x444D4163,%ecx
	setne	%al
	orl	%eax,%r10d
	jnz	.Lintel


	movl	$0x80000000,%eax
	cpuid
	cmpl	$0x80000001,%eax
	jb	.Lintel
	movl	%eax,%r10d
	movl	$0x80000001,%eax
	cpuid
	orl	%ecx,%r9d
	andl	$0x00000801,%r9d

	cmpl	$0x80000008,%r10d
	jb	.Lintel

	movl	$0x80000008,%eax
	cpuid
	movzbq	%cl,%r10
	incq	%r10

	movl	$1,%eax
	cpuid
	btl	$28,%edx
	jnc	.Lgeneric
	shrl	$16,%ebx
	cmpb	%r10b,%bl
	ja	.Lgeneric
	andl	$0xefffffff,%edx
	jmp	.Lgeneric

.Lintel:
	cmpl	$4,%r11d
	movl	$-1,%r10d
	jb	.Lnocacheinfo

	movl	$4,%eax
	movl	$0,%ecx
	cpuid
	movl	%eax,%r10d
	shrl	$14,%r10d
	andl	$0xfff,%r10d

.Lnocacheinfo:
	movl	$1,%eax
	cpuid
	andl	$0xbfefffff,%edx
	cmpl	$0,%r9d
	jne	.Lnotintel
	orl	$0x40000000,%edx
	andb	$15,%ah
	cmpb	$15,%ah
	jne	.Lnotintel
	orl	$0x00100000,%edx
.Lnotintel:
	btl	$28,%edx
	jnc	.Lgeneric
	andl	$0xefffffff,%edx
	cmpl	$0,%r10d
	je	.Lgeneric

	orl	$0x10000000,%edx
	shrl	$16,%ebx
	cmpb	$1,%bl
	ja	.Lgeneric
	andl	$0xefffffff,%edx
.Lgeneric:
	andl	$0x00000800,%r9d
	andl	$0xfffff7ff,%ecx
	orl	%ecx,%r9d

	movl	%edx,%r10d

	cmpl	$7,%r11d
	jb	.Lno_extended_info
	movl	$7,%eax
	xorl	%ecx,%ecx
	cpuid
	movl	%ebx,8(%rdi)
.Lno_extended_info:

	btl	$27,%r9d
	jnc	.Lclear_avx
	xorl	%ecx,%ecx
.byte	0x0f,0x01,0xd0
	andl	$6,%eax
	cmpl	$6,%eax
	je	.Ldone
.Lclear_avx:
	movl	$0xefffe7ff,%eax
	andl	%eax,%r9d
	andl	$0xffffffdf,8(%rdi)
.Ldone:
	shlq	$32,%r9
	movl	%r10d,%eax
	movq	%r8,%rbx
	orq	%r9,%rax
	.byte	0xf3,0xc3
.size	xOPENSSL_ia32_cpuid,.-xOPENSSL_ia32_cpuid

