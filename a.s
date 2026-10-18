	.file	"master_module"
	.text
	.p2align	4                               # -- Begin function qc_opendir
	.type	qc_opendir,@function
qc_opendir:                             # @qc_opendir
	.cfi_startproc
# %bb.0:                                # %entry
	jmp	opendir@PLT                     # TAILCALL
.Lfunc_end0:
	.size	qc_opendir, .Lfunc_end0-qc_opendir
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_readdir
	.type	qc_readdir,@function
qc_readdir:                             # @qc_readdir
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r15
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%rbx
	.cfi_def_cfa_offset 32
	.cfi_offset %rbx, -32
	.cfi_offset %r14, -24
	.cfi_offset %r15, -16
	movq	%rdx, %r14
	movq	%rsi, %rbx
	callq	readdir@PLT
	testq	%rax, %rax
	je	.LBB1_3
# %bb.1:                                # %if.end
	movq	%rax, %r15
	addq	$19, %r15
	movq	%r15, %rdi
	callq	strlen@PLT
	incq	%rax
	cmpq	%r14, %rax
	jbe	.LBB1_4
.LBB1_3:
	xorl	%eax, %eax
	jmp	.LBB1_5
.LBB1_4:                                # %if.end5
	movq	%rbx, %rdi
	movq	%r15, %rsi
	movq	%rax, %rdx
	callq	memcpy@PLT
	movb	$1, %al
.LBB1_5:                                # %cleanup9
                                        # kill: def $al killed $al killed $eax
	popq	%rbx
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end1:
	.size	qc_readdir, .Lfunc_end1-qc_readdir
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_closedir
	.type	qc_closedir,@function
qc_closedir:                            # @qc_closedir
	.cfi_startproc
# %bb.0:                                # %entry
	jmp	closedir@PLT                    # TAILCALL
.Lfunc_end2:
	.size	qc_closedir, .Lfunc_end2-qc_closedir
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_malloc
	.type	qc_malloc,@function
qc_malloc:                              # @qc_malloc
	.cfi_startproc
# %bb.0:                                # %entry
	jmp	malloc@PLT                      # TAILCALL
.Lfunc_end3:
	.size	qc_malloc, .Lfunc_end3-qc_malloc
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_free
	.type	qc_free,@function
qc_free:                                # @qc_free
	.cfi_startproc
# %bb.0:                                # %entry
	jmp	free@PLT                        # TAILCALL
.Lfunc_end4:
	.size	qc_free, .Lfunc_end4-qc_free
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_realloc
	.type	qc_realloc,@function
qc_realloc:                             # @qc_realloc
	.cfi_startproc
# %bb.0:                                # %entry
	jmp	realloc@PLT                     # TAILCALL
.Lfunc_end5:
	.size	qc_realloc, .Lfunc_end5-qc_realloc
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_calloc
	.type	qc_calloc,@function
qc_calloc:                              # @qc_calloc
	.cfi_startproc
# %bb.0:                                # %entry
	jmp	calloc@PLT                      # TAILCALL
.Lfunc_end6:
	.size	qc_calloc, .Lfunc_end6-qc_calloc
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_flush
	.type	qc_flush,@function
qc_flush:                               # @qc_flush
	.cfi_startproc
# %bb.0:                                # %entry
	xorl	%edi, %edi
	jmp	fflush@PLT                      # TAILCALL
.Lfunc_end7:
	.size	qc_flush, .Lfunc_end7-qc_flush
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_fmt_int
	.type	qc_fmt_int,@function
qc_fmt_int:                             # @qc_fmt_int
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	subq	$40, %rsp
	.cfi_def_cfa_offset 64
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	movl	%esi, %r8d
	movq	%rdi, %rbx
	testl	%edx, %edx
	js	.LBB8_4
# %bb.1:                                # %if.then
	movl	%edx, %r9d
	testl	%r8d, %r8d
	jle	.LBB8_3
# %bb.2:                                # %if.then2
	movl	$.L.str.1, %edx
	movl	$.L.str.2, %eax
	testb	%cl, %cl
	cmovneq	%rdx, %rax
	movq	%rsp, %rdi
	movl	$32, %esi
	movl	$.L.str, %edx
	movq	%rax, %rcx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB8_7
.LBB8_4:                                # %if.else5
	testl	%r8d, %r8d
	jle	.LBB8_6
# %bb.5:                                # %if.then7
	movl	$.L.str.1, %edx
	movl	$.L.str.2, %eax
	testb	%cl, %cl
	cmovneq	%rdx, %rax
	movq	%rsp, %rdi
	movl	$32, %esi
	movl	$.L.str.4, %edx
	movq	%rax, %rcx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB8_7
.LBB8_3:                                # %if.else
	movq	%rsp, %rdi
	movl	$32, %esi
	movl	$.L.str.3, %edx
	movl	%r9d, %ecx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB8_7
.LBB8_6:                                # %if.else12
	movq	%rsp, %rdi
	movl	$32, %esi
	movl	$.L.str.5, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
.LBB8_7:                                # %if.end16
	movq	%rsp, %rdx
	xorl	%edi, %edi
	xorl	%esi, %esi
	movq	%rbx, %rcx
	xorl	%eax, %eax
	callq	snprintf@PLT
	incl	%eax
	movslq	%eax, %r14
	movq	%r14, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB8_9
# %bb.8:                                # %if.end21
	movq	%rsp, %rdx
	movq	%rax, %rdi
	movq	%r14, %rsi
	movq	%rbx, %rcx
	movq	%rax, %rbx
	xorl	%eax, %eax
	callq	snprintf@PLT
	movq	%rbx, %rax
.LBB8_9:                                # %cleanup
	addq	$40, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end8:
	.size	qc_fmt_int, .Lfunc_end8-qc_fmt_int
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_fmt_unsigned_int
	.type	qc_fmt_unsigned_int,@function
qc_fmt_unsigned_int:                    # @qc_fmt_unsigned_int
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r15
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%rbx
	.cfi_def_cfa_offset 32
	subq	$32, %rsp
	.cfi_def_cfa_offset 64
	.cfi_offset %rbx, -32
	.cfi_offset %r14, -24
	.cfi_offset %r15, -16
	movq	%rdi, %rbx
	movl	$.L.str.6, %eax
	movl	$.L.str.7, %edx
	testl	%esi, %esi
	cmovneq	%rax, %rdx
	xorl	%r14d, %r14d
	movq	%rsp, %r15
	movl	$32, %esi
	movq	%r15, %rdi
	xorl	%eax, %eax
	callq	snprintf@PLT
	xorl	%edi, %edi
	xorl	%esi, %esi
	movq	%r15, %rdx
	movq	%rbx, %rcx
	xorl	%eax, %eax
	callq	snprintf@PLT
	testl	%eax, %eax
	js	.LBB9_4
# %bb.1:                                # %if.end6
	movl	%eax, %r15d
	incl	%r15d
	movq	%r15, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB9_2
# %bb.3:                                # %if.end9
	movq	%rax, %r14
	movq	%rsp, %rdx
	movq	%rax, %rdi
	movq	%r15, %rsi
	movq	%rbx, %rcx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB9_4
.LBB9_2:
	xorl	%r14d, %r14d
.LBB9_4:                                # %cleanup14
	movq	%r14, %rax
	addq	$32, %rsp
	.cfi_def_cfa_offset 32
	popq	%rbx
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end9:
	.size	qc_fmt_unsigned_int, .Lfunc_end9-qc_fmt_unsigned_int
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_fmt_float
	.type	qc_fmt_float,@function
qc_fmt_float:                           # @qc_fmt_float
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	subq	$56, %rsp
	.cfi_def_cfa_offset 80
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	movl	%edi, %r8d
	movsd	%xmm0, 8(%rsp)                  # 8-byte Spill
	testl	%esi, %esi
	js	.LBB10_4
# %bb.1:                                # %if.then
	movl	%esi, %r9d
	testl	%r8d, %r8d
	jle	.LBB10_3
# %bb.2:                                # %if.then2
	movl	$.L.str.1, %eax
	movl	$.L.str.2, %ecx
	testb	%dl, %dl
	cmovneq	%rax, %rcx
	leaq	16(%rsp), %rdi
	movl	$32, %esi
	movl	$.L.str.8, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB10_7
.LBB10_4:                               # %if.else5
	testl	%r8d, %r8d
	jle	.LBB10_6
# %bb.5:                                # %if.then7
	movl	$.L.str.1, %eax
	movl	$.L.str.2, %ecx
	testb	%dl, %dl
	cmovneq	%rax, %rcx
	leaq	16(%rsp), %rdi
	movl	$32, %esi
	movl	$.L.str.10, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB10_7
.LBB10_3:                               # %if.else
	leaq	16(%rsp), %rdi
	movl	$32, %esi
	movl	$.L.str.9, %edx
	movl	%r9d, %ecx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB10_7
.LBB10_6:                               # %if.else12
	leaq	16(%rsp), %rdi
	movl	$32, %esi
	movl	$.L.str.11, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
.LBB10_7:                               # %if.end16
	xorl	%ebx, %ebx
	leaq	16(%rsp), %rdx
	xorl	%edi, %edi
	xorl	%esi, %esi
	movsd	8(%rsp), %xmm0                  # 8-byte Reload
                                        # xmm0 = mem[0],zero
	movb	$1, %al
	callq	snprintf@PLT
	testl	%eax, %eax
	js	.LBB10_11
# %bb.8:                                # %if.end21
	movl	%eax, %r14d
	incl	%r14d
	movq	%r14, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB10_9
# %bb.10:                               # %if.end24
	movq	%rax, %rbx
	leaq	16(%rsp), %rdx
	movq	%rax, %rdi
	movq	%r14, %rsi
	movsd	8(%rsp), %xmm0                  # 8-byte Reload
                                        # xmm0 = mem[0],zero
	movb	$1, %al
	callq	snprintf@PLT
	jmp	.LBB10_11
.LBB10_9:
	xorl	%ebx, %ebx
.LBB10_11:                              # %cleanup29
	movq	%rbx, %rax
	addq	$56, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end10:
	.size	qc_fmt_float, .Lfunc_end10-qc_fmt_float
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_fmt_double
	.type	qc_fmt_double,@function
qc_fmt_double:                          # @qc_fmt_double
	.cfi_startproc
# %bb.0:                                # %entry
	jmp	qc_fmt_float                    # TAILCALL
.Lfunc_end11:
	.size	qc_fmt_double, .Lfunc_end11-qc_fmt_double
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_fmt_scientific
	.type	qc_fmt_scientific,@function
qc_fmt_scientific:                      # @qc_fmt_scientific
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	subq	$56, %rsp
	.cfi_def_cfa_offset 80
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	movl	%edi, %r8d
	movsd	%xmm0, 8(%rsp)                  # 8-byte Spill
	testl	%esi, %esi
	js	.LBB12_4
# %bb.1:                                # %if.then
	movl	%esi, %r9d
	testl	%r8d, %r8d
	jle	.LBB12_3
# %bb.2:                                # %if.then2
	movl	$.L.str.1, %eax
	movl	$.L.str.2, %ecx
	testb	%dl, %dl
	cmovneq	%rax, %rcx
	leaq	16(%rsp), %rdi
	movl	$32, %esi
	movl	$.L.str.12, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB12_7
.LBB12_4:                               # %if.else5
	testl	%r8d, %r8d
	jle	.LBB12_6
# %bb.5:                                # %if.then7
	movl	$.L.str.1, %eax
	movl	$.L.str.2, %ecx
	testb	%dl, %dl
	cmovneq	%rax, %rcx
	leaq	16(%rsp), %rdi
	movl	$32, %esi
	movl	$.L.str.14, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB12_7
.LBB12_3:                               # %if.else
	leaq	16(%rsp), %rdi
	movl	$32, %esi
	movl	$.L.str.13, %edx
	movl	%r9d, %ecx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB12_7
.LBB12_6:                               # %if.else12
	leaq	16(%rsp), %rdi
	movl	$32, %esi
	movl	$.L.str.15, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
.LBB12_7:                               # %if.end16
	xorl	%ebx, %ebx
	leaq	16(%rsp), %rdx
	xorl	%edi, %edi
	xorl	%esi, %esi
	movsd	8(%rsp), %xmm0                  # 8-byte Reload
                                        # xmm0 = mem[0],zero
	movb	$1, %al
	callq	snprintf@PLT
	testl	%eax, %eax
	js	.LBB12_11
# %bb.8:                                # %if.end21
	movl	%eax, %r14d
	incl	%r14d
	movq	%r14, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB12_9
# %bb.10:                               # %if.end24
	movq	%rax, %rbx
	leaq	16(%rsp), %rdx
	movq	%rax, %rdi
	movq	%r14, %rsi
	movsd	8(%rsp), %xmm0                  # 8-byte Reload
                                        # xmm0 = mem[0],zero
	movb	$1, %al
	callq	snprintf@PLT
	jmp	.LBB12_11
.LBB12_9:
	xorl	%ebx, %ebx
.LBB12_11:                              # %cleanup29
	movq	%rbx, %rax
	addq	$56, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end12:
	.size	qc_fmt_scientific, .Lfunc_end12-qc_fmt_scientific
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_fmt_char
	.type	qc_fmt_char,@function
qc_fmt_char:                            # @qc_fmt_char
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%rbx
	.cfi_def_cfa_offset 32
	subq	$16, %rsp
	.cfi_def_cfa_offset 48
	.cfi_offset %rbx, -32
	.cfi_offset %r14, -24
	.cfi_offset %rbp, -16
	movl	%edi, %ebx
	testl	%esi, %esi
	jle	.LBB13_2
# %bb.1:                                # %if.then
	movl	%esi, %r8d
	movl	$.L.str.1, %eax
	movl	$.L.str.2, %ecx
	testb	%dl, %dl
	cmovneq	%rax, %rcx
	movq	%rsp, %rdi
	movl	$16, %esi
	movl	$.L.str.16, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB13_3
.LBB13_2:                               # %if.else
	movq	%rsp, %rdi
	movl	$16, %esi
	movl	$.L.str.17, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
.LBB13_3:                               # %if.end
	movsbl	%bl, %ebp
	xorl	%ebx, %ebx
	movq	%rsp, %rdx
	xorl	%edi, %edi
	xorl	%esi, %esi
	movl	%ebp, %ecx
	xorl	%eax, %eax
	callq	snprintf@PLT
	testl	%eax, %eax
	js	.LBB13_7
# %bb.4:                                # %if.end7
	movl	%eax, %r14d
	incl	%r14d
	movq	%r14, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB13_5
# %bb.6:                                # %if.end11
	movq	%rax, %rbx
	movq	%rsp, %rdx
	movq	%rax, %rdi
	movq	%r14, %rsi
	movl	%ebp, %ecx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB13_7
.LBB13_5:
	xorl	%ebx, %ebx
.LBB13_7:                               # %cleanup17
	movq	%rbx, %rax
	addq	$16, %rsp
	.cfi_def_cfa_offset 32
	popq	%rbx
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end13:
	.size	qc_fmt_char, .Lfunc_end13-qc_fmt_char
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_fmt_string
	.type	qc_fmt_string,@function
qc_fmt_string:                          # @qc_fmt_string
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r15
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%rbx
	.cfi_def_cfa_offset 32
	subq	$16, %rsp
	.cfi_def_cfa_offset 48
	.cfi_offset %rbx, -32
	.cfi_offset %r14, -24
	.cfi_offset %r15, -16
	movq	%rdi, %rbx
	testq	%rdi, %rdi
	movl	$.L.str.2, %ecx
	cmoveq	%rcx, %rbx
	testl	%esi, %esi
	jle	.LBB14_2
# %bb.1:                                # %if.then1
	movl	%esi, %r8d
	movl	$.L.str.1, %eax
	testb	%dl, %dl
	cmovneq	%rax, %rcx
	movq	%rsp, %rdi
	movl	$16, %esi
	movl	$.L.str.18, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB14_3
.LBB14_2:                               # %if.else
	movq	%rsp, %rdi
	movl	$16, %esi
	movl	$.L.str.19, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
.LBB14_3:                               # %if.end4
	xorl	%r14d, %r14d
	movq	%rsp, %rdx
	xorl	%edi, %edi
	xorl	%esi, %esi
	movq	%rbx, %rcx
	xorl	%eax, %eax
	callq	snprintf@PLT
	testl	%eax, %eax
	js	.LBB14_7
# %bb.4:                                # %if.end9
	movl	%eax, %r15d
	incl	%r15d
	movq	%r15, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB14_5
# %bb.6:                                # %if.end13
	movq	%rax, %r14
	movq	%rsp, %rdx
	movq	%rax, %rdi
	movq	%r15, %rsi
	movq	%rbx, %rcx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB14_7
.LBB14_5:
	xorl	%r14d, %r14d
.LBB14_7:                               # %cleanup18
	movq	%r14, %rax
	addq	$16, %rsp
	.cfi_def_cfa_offset 32
	popq	%rbx
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end14:
	.size	qc_fmt_string, .Lfunc_end14-qc_fmt_string
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_fmt_hex
	.type	qc_fmt_hex,@function
qc_fmt_hex:                             # @qc_fmt_hex
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	subq	$40, %rsp
	.cfi_def_cfa_offset 64
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	movq	%rdi, %rbx
	testl	%esi, %esi
	jle	.LBB15_2
# %bb.1:                                # %if.then
	movl	%esi, %r8d
	movl	$.L.str.1, %eax
	movl	$.L.str.2, %ecx
	testb	%dl, %dl
	cmovneq	%rax, %rcx
	movq	%rsp, %rdi
	movl	$32, %esi
	movl	$.L.str.20, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB15_3
.LBB15_2:                               # %if.else
	movq	%rsp, %rdi
	movl	$32, %esi
	movl	$.L.str.21, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
.LBB15_3:                               # %if.end
	movq	%rsp, %rdx
	xorl	%edi, %edi
	xorl	%esi, %esi
	movq	%rbx, %rcx
	xorl	%eax, %eax
	callq	snprintf@PLT
	incl	%eax
	movslq	%eax, %r14
	movq	%r14, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB15_5
# %bb.4:                                # %if.end7
	movq	%rsp, %rdx
	movq	%rax, %rdi
	movq	%r14, %rsi
	movq	%rbx, %rcx
	movq	%rax, %rbx
	xorl	%eax, %eax
	callq	snprintf@PLT
	movq	%rbx, %rax
.LBB15_5:                               # %cleanup
	addq	$40, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end15:
	.size	qc_fmt_hex, .Lfunc_end15-qc_fmt_hex
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_fmt_octal
	.type	qc_fmt_octal,@function
qc_fmt_octal:                           # @qc_fmt_octal
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	subq	$40, %rsp
	.cfi_def_cfa_offset 64
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	movq	%rdi, %rbx
	testl	%esi, %esi
	jle	.LBB16_2
# %bb.1:                                # %if.then
	movl	%esi, %r8d
	movl	$.L.str.1, %eax
	movl	$.L.str.2, %ecx
	testb	%dl, %dl
	cmovneq	%rax, %rcx
	movq	%rsp, %rdi
	movl	$32, %esi
	movl	$.L.str.22, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB16_3
.LBB16_2:                               # %if.else
	movq	%rsp, %rdi
	movl	$32, %esi
	movl	$.L.str.23, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
.LBB16_3:                               # %if.end
	movq	%rsp, %rdx
	xorl	%edi, %edi
	xorl	%esi, %esi
	movq	%rbx, %rcx
	xorl	%eax, %eax
	callq	snprintf@PLT
	incl	%eax
	movslq	%eax, %r14
	movq	%r14, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB16_5
# %bb.4:                                # %if.end7
	movq	%rsp, %rdx
	movq	%rax, %rdi
	movq	%r14, %rsi
	movq	%rbx, %rcx
	movq	%rax, %rbx
	xorl	%eax, %eax
	callq	snprintf@PLT
	movq	%rbx, %rax
.LBB16_5:                               # %cleanup
	addq	$40, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end16:
	.size	qc_fmt_octal, .Lfunc_end16-qc_fmt_octal
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_fmt_bool
	.type	qc_fmt_bool,@function
qc_fmt_bool:                            # @qc_fmt_bool
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r15
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%rbx
	.cfi_def_cfa_offset 32
	subq	$16, %rsp
	.cfi_def_cfa_offset 48
	.cfi_offset %rbx, -32
	.cfi_offset %r14, -24
	.cfi_offset %r15, -16
	movl	$.L.str.24, %eax
	movl	$.L.str.25, %ebx
	testl	%edi, %edi
	cmovneq	%rax, %rbx
	testl	%esi, %esi
	jle	.LBB17_2
# %bb.1:                                # %if.then1.i
	movl	%esi, %r8d
	movl	$.L.str.1, %eax
	movl	$.L.str.2, %ecx
	testb	%dl, %dl
	cmovneq	%rax, %rcx
	movq	%rsp, %rdi
	movl	$16, %esi
	movl	$.L.str.18, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB17_3
.LBB17_2:                               # %if.else.i
	movq	%rsp, %rdi
	movl	$16, %esi
	movl	$.L.str.19, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
.LBB17_3:                               # %if.end4.i
	xorl	%r14d, %r14d
	movq	%rsp, %rdx
	xorl	%edi, %edi
	xorl	%esi, %esi
	movq	%rbx, %rcx
	xorl	%eax, %eax
	callq	snprintf@PLT
	testl	%eax, %eax
	js	.LBB17_7
# %bb.4:                                # %if.end9.i
	movl	%eax, %r15d
	incl	%r15d
	movq	%r15, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB17_5
# %bb.6:                                # %if.end13.i
	movq	%rax, %r14
	movq	%rsp, %rdx
	movq	%rax, %rdi
	movq	%r15, %rsi
	movq	%rbx, %rcx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB17_7
.LBB17_5:
	xorl	%r14d, %r14d
.LBB17_7:                               # %qc_fmt_string.exit
	movq	%r14, %rax
	addq	$16, %rsp
	.cfi_def_cfa_offset 32
	popq	%rbx
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end17:
	.size	qc_fmt_bool, .Lfunc_end17-qc_fmt_bool
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_fmt_qbool
	.type	qc_fmt_qbool,@function
qc_fmt_qbool:                           # @qc_fmt_qbool
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r15
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%rbx
	.cfi_def_cfa_offset 32
	subq	$16, %rsp
	.cfi_def_cfa_offset 48
	.cfi_offset %rbx, -32
	.cfi_offset %r14, -24
	.cfi_offset %r15, -16
                                        # kill: def $edi killed $edi def $rdi
	andl	$3, %edi
	movq	.Lswitch.table.qc_to_string_qbool(,%rdi,8), %rbx
	testl	%esi, %esi
	jle	.LBB18_2
# %bb.1:                                # %if.then1.i
	movl	%esi, %r8d
	movl	$.L.str.1, %eax
	movl	$.L.str.2, %ecx
	testb	%dl, %dl
	cmovneq	%rax, %rcx
	movq	%rsp, %rdi
	movl	$16, %esi
	movl	$.L.str.18, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB18_3
.LBB18_2:                               # %if.else.i
	movq	%rsp, %rdi
	movl	$16, %esi
	movl	$.L.str.19, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
.LBB18_3:                               # %if.end4.i
	xorl	%r14d, %r14d
	movq	%rsp, %rdx
	xorl	%edi, %edi
	xorl	%esi, %esi
	movq	%rbx, %rcx
	xorl	%eax, %eax
	callq	snprintf@PLT
	testl	%eax, %eax
	js	.LBB18_7
# %bb.4:                                # %if.end9.i
	movl	%eax, %r15d
	incl	%r15d
	movq	%r15, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB18_5
# %bb.6:                                # %if.end13.i
	movq	%rax, %r14
	movq	%rsp, %rdx
	movq	%rax, %rdi
	movq	%r15, %rsi
	movq	%rbx, %rcx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB18_7
.LBB18_5:
	xorl	%r14d, %r14d
.LBB18_7:                               # %qc_fmt_string.exit
	movq	%r14, %rax
	addq	$16, %rsp
	.cfi_def_cfa_offset 32
	popq	%rbx
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end18:
	.size	qc_fmt_qbool, .Lfunc_end18-qc_fmt_qbool
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_fmt_ptr
	.type	qc_fmt_ptr,@function
qc_fmt_ptr:                             # @qc_fmt_ptr
	.cfi_startproc
# %bb.0:                                # %entry
	subq	$72, %rsp
	.cfi_def_cfa_offset 80
	movl	%esi, %ecx
	movq	%rdi, %r8
	xorb	$1, %dl
	testl	%esi, %esi
	setle	%al
	movq	%rsp, %rdi
	movl	$64, %esi
	orb	%dl, %al
	jne	.LBB19_3
# %bb.1:                                # %if.then
	movl	$.L.str.30, %edx
	jmp	.LBB19_2
.LBB19_3:                               # %if.else
	testl	%ecx, %ecx
	jle	.LBB19_5
# %bb.4:                                # %if.then2
	movl	$.L.str.31, %edx
.LBB19_2:                               # %if.end8
	xorl	%eax, %eax
	callq	snprintf@PLT
.LBB19_6:                               # %if.end8
	movq	%rsp, %rdi
	callq	strdup@PLT
	addq	$72, %rsp
	.cfi_def_cfa_offset 8
	retq
.LBB19_5:                               # %if.else5
	.cfi_def_cfa_offset 80
	movl	$.L.str.32, %edx
	movq	%r8, %rcx
	xorl	%eax, %eax
	callq	snprintf@PLT
	jmp	.LBB19_6
.Lfunc_end19:
	.size	qc_fmt_ptr, .Lfunc_end19-qc_fmt_ptr
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_powi_i32
	.type	qc_powi_i32,@function
qc_powi_i32:                            # @qc_powi_i32
	.cfi_startproc
# %bb.0:                                # %entry
	testl	%esi, %esi
	js	.LBB20_1
# %bb.2:                                # %while.cond.preheader
	je	.LBB20_3
# %bb.4:                                # %while.body.preheader
	movl	$1, %ecx
	movl	$1, %eax
	.p2align	4
.LBB20_5:                               # %while.body
                                        # =>This Inner Loop Header: Depth=1
	testb	$1, %sil
	movl	%edi, %edx
	cmovel	%ecx, %edx
	imull	%edx, %eax
	imull	%edi, %edi
	shrl	%esi
	jne	.LBB20_5
# %bb.6:                                # %return
	retq
.LBB20_1:
	xorl	%eax, %eax
	retq
.LBB20_3:
	movl	$1, %eax
	retq
.Lfunc_end20:
	.size	qc_powi_i32, .Lfunc_end20-qc_powi_i32
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_string_concat
	.type	qc_string_concat,@function
qc_string_concat:                       # @qc_string_concat
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r15
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%r13
	.cfi_def_cfa_offset 32
	pushq	%r12
	.cfi_def_cfa_offset 40
	pushq	%rbx
	.cfi_def_cfa_offset 48
	.cfi_offset %rbx, -48
	.cfi_offset %r12, -40
	.cfi_offset %r13, -32
	.cfi_offset %r14, -24
	.cfi_offset %r15, -16
	movq	%rdi, %r14
	testq	%rdi, %rdi
	movl	$.L.str.2, %eax
	cmoveq	%rax, %r14
	movq	%rsi, %rbx
	testq	%rsi, %rsi
	cmoveq	%rax, %rbx
	movq	%r14, %rdi
	callq	strlen@PLT
	movq	%rax, %r15
	movq	%rbx, %rdi
	callq	strlen@PLT
	movq	%rax, %r12
	leaq	(%rax,%r15), %rdi
	incq	%rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB21_2
# %bb.1:                                # %if.end9
	movq	%r12, %r13
	addq	%r15, %r13
	movq	%rax, %rdi
	movq	%r14, %rsi
	movq	%r15, %rdx
	movq	%rax, %r14
	callq	memcpy@PLT
	movq	%r14, %rdi
	addq	%r15, %rdi
	movq	%rbx, %rsi
	movq	%r12, %rdx
	callq	memcpy@PLT
	movq	%r14, %rax
	movb	$0, (%r14,%r13)
.LBB21_2:                               # %cleanup
	popq	%rbx
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end21:
	.size	qc_string_concat, .Lfunc_end21-qc_string_concat
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_string_eq
	.type	qc_string_eq,@function
qc_string_eq:                           # @qc_string_eq
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	sete	%al
	testq	%rsi, %rsi
	sete	%cl
	orb	%al, %cl
	jne	.LBB22_1
# %bb.2:                                # %if.end
	pushq	%rax
	.cfi_def_cfa_offset 16
	callq	strcmp@PLT
	testl	%eax, %eax
	sete	%al
	addq	$8, %rsp
	.cfi_def_cfa_offset 8
                                        # kill: def $al killed $al killed $eax
	retq
.LBB22_1:
	xorl	%eax, %eax
                                        # kill: def $al killed $al killed $eax
	retq
.Lfunc_end22:
	.size	qc_string_eq, .Lfunc_end22-qc_string_eq
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_qand
	.type	qc_qand,@function
qc_qand:                                # @qc_qand
	.cfi_startproc
# %bb.0:                                # %entry
	movl	%esi, %eax
	orb	%dil, %al
	je	.LBB23_1
# %bb.2:                                # %if.end
	cmpb	$2, %dil
	setb	%al
	cmpb	$2, %sil
	setb	%cl
	orb	%al, %cl
	movb	$1, %al
	jne	.LBB23_4
# %bb.3:                                # %if.end14
	xorb	$2, %dil
	xorb	$2, %sil
	orb	%dil, %sil
	sete	%al
	xorb	$3, %al
.LBB23_4:                               # %return
                                        # kill: def $al killed $al killed $eax
	retq
.LBB23_1:
	xorl	%eax, %eax
                                        # kill: def $al killed $al killed $eax
	retq
.Lfunc_end23:
	.size	qc_qand, .Lfunc_end23-qc_qand
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_qor
	.type	qc_qor,@function
qc_qor:                                 # @qc_qor
	.cfi_startproc
# %bb.0:                                # %entry
	movl	%esi, %eax
	orb	%dil, %al
	je	.LBB24_1
# %bb.2:                                # %if.end
	movl	%esi, %ecx
	xorb	$1, %cl
	movb	$1, %al
	orb	%dil, %cl
	jne	.LBB24_3
.LBB24_8:                               # %return
                                        # kill: def $al killed $al killed $eax
	retq
.LBB24_1:
	xorl	%eax, %eax
                                        # kill: def $al killed $al killed $eax
	retq
.LBB24_3:                               # %if.end9
	movl	%esi, %ecx
	xorb	$2, %cl
	movl	%edi, %edx
	orb	%cl, %dl
	jne	.LBB24_5
# %bb.4:
	movb	$2, %al
                                        # kill: def $al killed $al killed $eax
	retq
.LBB24_5:                               # %if.end16
	cmpb	$1, %dil
	sete	%dl
	cmpb	$2, %sil
	setb	%r8b
	testb	%r8b, %dl
	jne	.LBB24_8
# %bb.6:                                # %if.end30
	xorb	$2, %dil
	orb	%dil, %sil
	movb	$2, %al
	je	.LBB24_8
# %bb.7:                                # %if.end37
	orb	%cl, %dil
	sete	%al
	xorb	$3, %al
                                        # kill: def $al killed $al killed $eax
	retq
.Lfunc_end24:
	.size	qc_qor, .Lfunc_end24-qc_qor
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_qxor
	.type	qc_qxor,@function
qc_qxor:                                # @qc_qxor
	.cfi_startproc
# %bb.0:                                # %entry
	movl	%esi, %eax
	orb	%dil, %al
	cmpb	$2, %al
	jae	.LBB25_2
# %bb.1:
	xorl	%eax, %eax
                                        # kill: def $al killed $al killed $eax
	retq
.LBB25_2:                               # %if.end
	cmpb	$2, %sil
	setb	%al
	cmpb	$2, %dil
	setb	%cl
	xorb	%al, %cl
	movb	$3, %al
	jne	.LBB25_4
# %bb.3:                                # %if.end16
	xorb	$2, %dil
	xorb	$2, %sil
	orb	%dil, %sil
	setne	%al
	addb	%al, %al
	incb	%al
.LBB25_4:                               # %return
                                        # kill: def $al killed $al killed $eax
	retq
.Lfunc_end25:
	.size	qc_qxor, .Lfunc_end25-qc_qxor
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_qnot
	.type	qc_qnot,@function
qc_qnot:                                # @qc_qnot
	.cfi_startproc
# %bb.0:                                # %entry
	movb	$3, %al
	subb	%dil, %al
	retq
.Lfunc_end26:
	.size	qc_qnot, .Lfunc_end26-qc_qnot
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_qand_collapse
	.type	qc_qand_collapse,@function
qc_qand_collapse:                       # @qc_qand_collapse
	.cfi_startproc
# %bb.0:                                # %entry
	movl	%edi, %eax
	andl	%esi, %eax
	andb	$2, %al
	shrb	%al
                                        # kill: def $al killed $al killed $eax
	retq
.Lfunc_end27:
	.size	qc_qand_collapse, .Lfunc_end27-qc_qand_collapse
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_qor_collapse
	.type	qc_qor_collapse,@function
qc_qor_collapse:                        # @qc_qor_collapse
	.cfi_startproc
# %bb.0:                                # %entry
	movl	%edi, %eax
	orl	%esi, %eax
	andb	$2, %al
	shrb	%al
                                        # kill: def $al killed $al killed $eax
	retq
.Lfunc_end28:
	.size	qc_qor_collapse, .Lfunc_end28-qc_qor_collapse
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_string_int
	.type	qc_to_string_int,@function
qc_to_string_int:                       # @qc_to_string_int
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	subq	$40, %rsp
	.cfi_def_cfa_offset 64
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	movl	%edi, %ecx
	xorl	%ebx, %ebx
	movq	%rsp, %rdi
	movl	$32, %esi
	movl	$.L.str.33, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
	testl	%eax, %eax
	js	.LBB29_4
# %bb.1:                                # %if.end
	movl	%eax, %r14d
	incl	%r14d
	movq	%r14, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB29_2
# %bb.3:                                # %if.end3
	movq	%rax, %rbx
	movq	%rsp, %rsi
	movq	%rax, %rdi
	movq	%r14, %rdx
	callq	memcpy@PLT
	jmp	.LBB29_4
.LBB29_2:
	xorl	%ebx, %ebx
.LBB29_4:                               # %cleanup7
	movq	%rbx, %rax
	addq	$40, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end29:
	.size	qc_to_string_int, .Lfunc_end29-qc_to_string_int
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_string_float
	.type	qc_to_string_float,@function
qc_to_string_float:                     # @qc_to_string_float
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbx
	.cfi_def_cfa_offset 16
	subq	$64, %rsp
	.cfi_def_cfa_offset 80
	.cfi_offset %rbx, -16
	cvtss2sd	%xmm0, %xmm0
	movq	%rsp, %rdi
	movl	$64, %esi
	movl	$.L.str.34, %edx
	movb	$1, %al
	callq	snprintf@PLT
	testl	%eax, %eax
	js	.LBB30_4
# %bb.1:                                # %if.end
	movl	%eax, %ebx
	incl	%ebx
	movq	%rbx, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB30_4
# %bb.2:                                # %if.end4
	movq	%rsp, %rsi
	movq	%rax, %rdi
	movq	%rbx, %rdx
	movq	%rax, %rbx
	callq	memcpy@PLT
	movq	%rbx, %rax
	jmp	.LBB30_5
.LBB30_4:
	xorl	%eax, %eax
.LBB30_5:                               # %cleanup8
	addq	$64, %rsp
	.cfi_def_cfa_offset 16
	popq	%rbx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end30:
	.size	qc_to_string_float, .Lfunc_end30-qc_to_string_float
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_string_long_int
	.type	qc_to_string_long_int,@function
qc_to_string_long_int:                  # @qc_to_string_long_int
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	subq	$40, %rsp
	.cfi_def_cfa_offset 64
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	movq	%rdi, %rcx
	xorl	%ebx, %ebx
	movq	%rsp, %rdi
	movl	$32, %esi
	movl	$.L.str.35, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
	testl	%eax, %eax
	js	.LBB31_4
# %bb.1:                                # %if.end
	movl	%eax, %r14d
	incl	%r14d
	movq	%r14, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB31_2
# %bb.3:                                # %if.end3
	movq	%rax, %rbx
	movq	%rsp, %rsi
	movq	%rax, %rdi
	movq	%r14, %rdx
	callq	memcpy@PLT
	jmp	.LBB31_4
.LBB31_2:
	xorl	%ebx, %ebx
.LBB31_4:                               # %cleanup7
	movq	%rbx, %rax
	addq	$40, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end31:
	.size	qc_to_string_long_int, .Lfunc_end31-qc_to_string_long_int
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_string_short_int
	.type	qc_to_string_short_int,@function
qc_to_string_short_int:                 # @qc_to_string_short_int
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	subq	$72, %rsp
	.cfi_def_cfa_offset 96
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	movl	%edi, %ecx
	xorl	%ebx, %ebx
	movq	%rsp, %rdi
	movl	$64, %esi
	movl	$.L.str.36, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
	testl	%eax, %eax
	js	.LBB32_4
# %bb.1:                                # %if.end
	movl	%eax, %r14d
	incl	%r14d
	movq	%r14, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB32_2
# %bb.3:                                # %if.end4
	movq	%rax, %rbx
	movq	%rsp, %rsi
	movq	%rax, %rdi
	movq	%r14, %rdx
	callq	memcpy@PLT
	jmp	.LBB32_4
.LBB32_2:
	xorl	%ebx, %ebx
.LBB32_4:                               # %cleanup8
	movq	%rbx, %rax
	addq	$72, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end32:
	.size	qc_to_string_short_int, .Lfunc_end32-qc_to_string_short_int
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_string_double
	.type	qc_to_string_double,@function
qc_to_string_double:                    # @qc_to_string_double
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbx
	.cfi_def_cfa_offset 16
	subq	$64, %rsp
	.cfi_def_cfa_offset 80
	.cfi_offset %rbx, -16
	movq	%rsp, %rdi
	movl	$64, %esi
	movl	$.L.str.34, %edx
	movb	$1, %al
	callq	snprintf@PLT
	testl	%eax, %eax
	js	.LBB33_4
# %bb.1:                                # %if.end
	movl	%eax, %ebx
	incl	%ebx
	movq	%rbx, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB33_4
# %bb.2:                                # %if.end3
	movq	%rsp, %rsi
	movq	%rax, %rdi
	movq	%rbx, %rdx
	movq	%rax, %rbx
	callq	memcpy@PLT
	movq	%rbx, %rax
	jmp	.LBB33_5
.LBB33_4:
	xorl	%eax, %eax
.LBB33_5:                               # %cleanup7
	addq	$64, %rsp
	.cfi_def_cfa_offset 16
	popq	%rbx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end33:
	.size	qc_to_string_double, .Lfunc_end33-qc_to_string_double
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_string_addr_t
	.type	qc_to_string_addr_t,@function
qc_to_string_addr_t:                    # @qc_to_string_addr_t
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	subq	$40, %rsp
	.cfi_def_cfa_offset 64
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	movq	%rdi, %rcx
	xorl	%ebx, %ebx
	movq	%rsp, %rdi
	movl	$32, %esi
	movl	$.L.str.37, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
	testl	%eax, %eax
	js	.LBB34_4
# %bb.1:                                # %if.end
	movl	%eax, %r14d
	incl	%r14d
	movq	%r14, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB34_2
# %bb.3:                                # %if.end3
	movq	%rax, %rbx
	movq	%rsp, %rsi
	movq	%rax, %rdi
	movq	%r14, %rdx
	callq	memcpy@PLT
	jmp	.LBB34_4
.LBB34_2:
	xorl	%ebx, %ebx
.LBB34_4:                               # %cleanup7
	movq	%rbx, %rax
	addq	$40, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end34:
	.size	qc_to_string_addr_t, .Lfunc_end34-qc_to_string_addr_t
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_string_byte
	.type	qc_to_string_byte,@function
qc_to_string_byte:                      # @qc_to_string_byte
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	subq	$40, %rsp
	.cfi_def_cfa_offset 64
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	movl	%edi, %ecx
	xorl	%ebx, %ebx
	movq	%rsp, %rdi
	movl	$32, %esi
	movl	$.L.str.38, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
	testl	%eax, %eax
	js	.LBB35_4
# %bb.1:                                # %if.end
	movl	%eax, %r14d
	incl	%r14d
	movq	%r14, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB35_2
# %bb.3:                                # %if.end4
	movq	%rax, %rbx
	movq	%rsp, %rsi
	movq	%rax, %rdi
	movq	%r14, %rdx
	callq	memcpy@PLT
	jmp	.LBB35_4
.LBB35_2:
	xorl	%ebx, %ebx
.LBB35_4:                               # %cleanup8
	movq	%rbx, %rax
	addq	$40, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end35:
	.size	qc_to_string_byte, .Lfunc_end35-qc_to_string_byte
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_string_nibble
	.type	qc_to_string_nibble,@function
qc_to_string_nibble:                    # @qc_to_string_nibble
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	subq	$40, %rsp
	.cfi_def_cfa_offset 64
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	movl	%edi, %ecx
	andl	$15, %ecx
	xorl	%ebx, %ebx
	movq	%rsp, %rdi
	movl	$32, %esi
	movl	$.L.str.38, %edx
	xorl	%eax, %eax
	callq	snprintf@PLT
	testl	%eax, %eax
	js	.LBB36_4
# %bb.1:                                # %if.end
	movl	%eax, %r14d
	incl	%r14d
	movq	%r14, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB36_2
# %bb.3:                                # %if.end4
	movq	%rax, %rbx
	movq	%rsp, %rsi
	movq	%rax, %rdi
	movq	%r14, %rdx
	callq	memcpy@PLT
	jmp	.LBB36_4
.LBB36_2:
	xorl	%ebx, %ebx
.LBB36_4:                               # %cleanup8
	movq	%rbx, %rax
	addq	$40, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end36:
	.size	qc_to_string_nibble, .Lfunc_end36-qc_to_string_nibble
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_string_bool
	.type	qc_to_string_bool,@function
qc_to_string_bool:                      # @qc_to_string_bool
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	pushq	%rax
	.cfi_def_cfa_offset 32
	.cfi_offset %rbx, -24
	.cfi_offset %rbp, -16
	movl	%edi, %ebp
	movl	%edi, %eax
	movl	$6, %ebx
	subq	%rax, %rbx
	movq	%rbx, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB37_1
# %bb.2:                                # %if.end
	movl	$.L.str.24, %ecx
	movl	$.L.str.25, %esi
	testb	%bpl, %bpl
	cmovneq	%rcx, %rsi
	movq	%rax, %rdi
	movq	%rbx, %rdx
	addq	$8, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	jmp	memcpy@PLT                      # TAILCALL
.LBB37_1:                               # %cleanup
	.cfi_def_cfa_offset 32
	addq	$8, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end37:
	.size	qc_to_string_bool, .Lfunc_end37-qc_to_string_bool
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_string_qbool
	.type	qc_to_string_qbool,@function
qc_to_string_qbool:                     # @qc_to_string_qbool
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	pushq	%rax
	.cfi_def_cfa_offset 32
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
                                        # kill: def $edi killed $edi def $rdi
	andl	$3, %edi
	movq	.Lswitch.table.qc_to_string_qbool(,%rdi,8), %rbx
	movq	%rbx, %rdi
	callq	strlen@PLT
	movq	%rax, %r14
	incq	%r14
	movq	%r14, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB38_1
# %bb.2:                                # %if.end
	movq	%rax, %rdi
	movq	%rbx, %rsi
	movq	%r14, %rdx
	addq	$8, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	jmp	memcpy@PLT                      # TAILCALL
.LBB38_1:                               # %cleanup
	.cfi_def_cfa_offset 32
	addq	$8, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end38:
	.size	qc_to_string_qbool, .Lfunc_end38-qc_to_string_qbool
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_string_char
	.type	qc_to_string_char,@function
qc_to_string_char:                      # @qc_to_string_char
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbx
	.cfi_def_cfa_offset 16
	.cfi_offset %rbx, -16
	movl	%edi, %ebx
	movl	$2, %edi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB39_2
# %bb.1:                                # %if.end
	movb	%bl, (%rax)
	movb	$0, 1(%rax)
.LBB39_2:                               # %cleanup
	popq	%rbx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end39:
	.size	qc_to_string_char, .Lfunc_end39-qc_to_string_char
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_print_string
	.type	qc_print_string,@function
qc_print_string:                        # @qc_print_string
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	movl	$.L.str.2, %eax
	cmoveq	%rax, %rdi
	movq	stdout@GOTPCREL(%rip), %rax
	movq	(%rax), %rsi
	jmp	fputs@PLT                       # TAILCALL
.Lfunc_end40:
	.size	qc_print_string, .Lfunc_end40-qc_print_string
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_len
	.type	qc_len,@function
qc_len:                                 # @qc_len
	.cfi_startproc
# %bb.0:                                # %entry
	jmp	strlen@PLT                      # TAILCALL
.Lfunc_end41:
	.size	qc_len, .Lfunc_end41-qc_len
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_lower
	.type	qc_to_lower,@function
qc_to_lower:                            # @qc_to_lower
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r15
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%r12
	.cfi_def_cfa_offset 32
	pushq	%rbx
	.cfi_def_cfa_offset 40
	pushq	%rax
	.cfi_def_cfa_offset 48
	.cfi_offset %rbx, -40
	.cfi_offset %r12, -32
	.cfi_offset %r14, -24
	.cfi_offset %r15, -16
	testq	%rdi, %rdi
	je	.LBB42_7
# %bb.1:                                # %if.end
	movq	%rdi, %rbx
	callq	strlen@PLT
	movq	%rax, %r15
	leaq	1(%rax), %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB42_7
# %bb.2:                                # %for.cond.preheader
	movq	%rax, %r14
	testq	%r15, %r15
	je	.LBB42_5
# %bb.3:                                # %for.body.preheader
	xorl	%r12d, %r12d
	.p2align	4
.LBB42_4:                               # %for.body
                                        # =>This Inner Loop Header: Depth=1
	movzbl	(%rbx,%r12), %edi
	callq	tolower@PLT
	movb	%al, (%r14,%r12)
	incq	%r12
	cmpq	%r12, %r15
	jne	.LBB42_4
.LBB42_5:                               # %for.cond.cleanup
	movb	$0, (%r14,%r15)
	jmp	.LBB42_8
.LBB42_7:
	xorl	%r14d, %r14d
.LBB42_8:                               # %return
	movq	%r14, %rax
	addq	$8, %rsp
	.cfi_def_cfa_offset 40
	popq	%rbx
	.cfi_def_cfa_offset 32
	popq	%r12
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end42:
	.size	qc_to_lower, .Lfunc_end42-qc_to_lower
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_upper
	.type	qc_to_upper,@function
qc_to_upper:                            # @qc_to_upper
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r15
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%r12
	.cfi_def_cfa_offset 32
	pushq	%rbx
	.cfi_def_cfa_offset 40
	pushq	%rax
	.cfi_def_cfa_offset 48
	.cfi_offset %rbx, -40
	.cfi_offset %r12, -32
	.cfi_offset %r14, -24
	.cfi_offset %r15, -16
	testq	%rdi, %rdi
	je	.LBB43_7
# %bb.1:                                # %if.end
	movq	%rdi, %rbx
	callq	strlen@PLT
	movq	%rax, %r15
	leaq	1(%rax), %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB43_7
# %bb.2:                                # %for.cond.preheader
	movq	%rax, %r14
	testq	%r15, %r15
	je	.LBB43_5
# %bb.3:                                # %for.body.preheader
	xorl	%r12d, %r12d
	.p2align	4
.LBB43_4:                               # %for.body
                                        # =>This Inner Loop Header: Depth=1
	movzbl	(%rbx,%r12), %edi
	callq	toupper@PLT
	movb	%al, (%r14,%r12)
	incq	%r12
	cmpq	%r12, %r15
	jne	.LBB43_4
.LBB43_5:                               # %for.cond.cleanup
	movb	$0, (%r14,%r15)
	jmp	.LBB43_8
.LBB43_7:
	xorl	%r14d, %r14d
.LBB43_8:                               # %return
	movq	%r14, %rax
	addq	$8, %rsp
	.cfi_def_cfa_offset 40
	popq	%rbx
	.cfi_def_cfa_offset 32
	popq	%r12
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end43:
	.size	qc_to_upper, .Lfunc_end43-qc_to_upper
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_substring
	.type	qc_substring,@function
qc_substring:                           # @qc_substring
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	je	.LBB44_1
# %bb.3:                                # %if.end
	pushq	%r15
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%r12
	.cfi_def_cfa_offset 32
	pushq	%rbx
	.cfi_def_cfa_offset 40
	pushq	%rax
	.cfi_def_cfa_offset 48
	.cfi_offset %rbx, -40
	.cfi_offset %r12, -32
	.cfi_offset %r14, -24
	.cfi_offset %r15, -16
	movl	%edx, %r15d
	movl	%esi, %r14d
	movq	%rdi, %rbx
	callq	strlen@PLT
	testl	%r14d, %r14d
	js	.LBB44_5
# %bb.4:                                # %lor.lhs.false
	movl	%eax, %r12d
	subl	%r14d, %r12d
	setg	%cl
	testl	%r15d, %r15d
	setg	%dl
	testb	%cl, %dl
	jne	.LBB44_6
.LBB44_5:                               # %if.then4
	movl	$1, %edi
	callq	malloc@PLT
	movb	$0, (%rax)
	jmp	.LBB44_9
.LBB44_1:
	.cfi_def_cfa_offset 8
	.cfi_restore %rbx
	.cfi_restore %r12
	.cfi_restore %r14
	.cfi_restore %r15
	xorl	%eax, %eax
	retq
.LBB44_6:                               # %if.end6
	.cfi_def_cfa_offset 48
	.cfi_offset %rbx, -40
	.cfi_offset %r12, -32
	.cfi_offset %r14, -24
	.cfi_offset %r15, -16
	leal	(%r15,%r14), %ecx
	cmpl	%eax, %ecx
	cmovbel	%r15d, %r12d
	leal	1(%r12), %edi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB44_7
# %bb.8:                                # %if.end15
	movl	%r14d, %ecx
	addq	%rcx, %rbx
	movslq	%r12d, %r14
	movq	%rax, %rdi
	movq	%rbx, %rsi
	movq	%r14, %rdx
	movq	%rax, %rbx
	callq	memcpy@PLT
	movq	%rbx, %rax
	movb	$0, (%rbx,%r14)
	jmp	.LBB44_9
.LBB44_7:
	xorl	%eax, %eax
.LBB44_9:
	addq	$8, %rsp
	.cfi_def_cfa_offset 40
	popq	%rbx
	.cfi_def_cfa_offset 32
	popq	%r12
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	.cfi_restore %rbx
	.cfi_restore %r12
	.cfi_restore %r14
	.cfi_restore %r15
	retq
.Lfunc_end44:
	.size	qc_substring, .Lfunc_end44-qc_substring
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_contains
	.type	qc_contains,@function
qc_contains:                            # @qc_contains
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	sete	%cl
	testq	%rsi, %rsi
	sete	%dl
	xorl	%eax, %eax
	orb	%cl, %dl
	jne	.LBB45_2
# %bb.1:                                # %if.end
	pushq	%rax
	.cfi_def_cfa_offset 16
	callq	strstr@PLT
	movq	%rax, %rcx
	xorl	%eax, %eax
	testq	%rcx, %rcx
	setne	%al
	addq	$8, %rsp
	.cfi_def_cfa_offset 8
.LBB45_2:                               # %return
	retq
.Lfunc_end45:
	.size	qc_contains, .Lfunc_end45-qc_contains
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_startswith
	.type	qc_startswith,@function
qc_startswith:                          # @qc_startswith
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	sete	%cl
	testq	%rsi, %rsi
	sete	%dl
	xorl	%eax, %eax
	orb	%cl, %dl
	jne	.LBB46_2
# %bb.1:                                # %if.end
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	pushq	%rax
	.cfi_def_cfa_offset 32
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	movq	%rdi, %rbx
	movq	%rsi, %rdi
	movq	%rsi, %r14
	callq	strlen@PLT
	movq	%rbx, %rdi
	movq	%r14, %rsi
	movq	%rax, %rdx
	callq	strncmp@PLT
	movl	%eax, %ecx
	xorl	%eax, %eax
	testl	%ecx, %ecx
	sete	%al
	addq	$8, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	.cfi_restore %rbx
	.cfi_restore %r14
.LBB46_2:                               # %return
	retq
.Lfunc_end46:
	.size	qc_startswith, .Lfunc_end46-qc_startswith
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_endswith
	.type	qc_endswith,@function
qc_endswith:                            # @qc_endswith
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	pushq	%r15
	.cfi_def_cfa_offset 24
	pushq	%r14
	.cfi_def_cfa_offset 32
	pushq	%rbx
	.cfi_def_cfa_offset 40
	pushq	%rax
	.cfi_def_cfa_offset 48
	.cfi_offset %rbx, -40
	.cfi_offset %r14, -32
	.cfi_offset %r15, -24
	.cfi_offset %rbp, -16
	testq	%rdi, %rdi
	sete	%al
	testq	%rsi, %rsi
	sete	%cl
	xorl	%ebp, %ebp
	orb	%al, %cl
	jne	.LBB47_3
# %bb.1:                                # %if.end
	movq	%rsi, %rbx
	movq	%rdi, %r15
	callq	strlen@PLT
	movq	%rax, %r14
	movq	%rbx, %rdi
	callq	strlen@PLT
	cmpq	%r14, %rax
	ja	.LBB47_3
# %bb.2:                                # %if.end4
	movq	%r15, %rdi
	addq	%r14, %rdi
	subq	%rax, %rdi
	movq	%rbx, %rsi
	callq	strcmp@PLT
	xorl	%ebp, %ebp
	testl	%eax, %eax
	sete	%bpl
.LBB47_3:                               # %return
	movl	%ebp, %eax
	addq	$8, %rsp
	.cfi_def_cfa_offset 40
	popq	%rbx
	.cfi_def_cfa_offset 32
	popq	%r14
	.cfi_def_cfa_offset 24
	popq	%r15
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end47:
	.size	qc_endswith, .Lfunc_end47-qc_endswith
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_bool_from_string
	.type	qc_to_bool_from_string,@function
qc_to_bool_from_string:                 # @qc_to_bool_from_string
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	je	.LBB48_4
# %bb.1:                                # %if.end
	pushq	%rbx
	.cfi_def_cfa_offset 16
	.cfi_offset %rbx, -16
	movq	%rdi, %rbx
	movl	$.L.str.24, %esi
	callq	strcmp@PLT
	testl	%eax, %eax
	je	.LBB48_5
# %bb.2:                                # %sub_0
	cmpb	$49, (%rbx)
	jne	.LBB48_7
# %bb.3:                                # %sub_1
	cmpb	$0, 1(%rbx)
	sete	%al
	popq	%rbx
	.cfi_def_cfa_offset 8
	.cfi_restore %rbx
                                        # kill: def $al killed $al killed $eax
	retq
.LBB48_4:
	xorl	%eax, %eax
                                        # kill: def $al killed $al killed $eax
	retq
.LBB48_5:
	.cfi_def_cfa_offset 16
	.cfi_offset %rbx, -16
	movb	$1, %al
	popq	%rbx
	.cfi_def_cfa_offset 8
	.cfi_restore %rbx
                                        # kill: def $al killed $al killed $eax
	retq
.LBB48_7:
	.cfi_def_cfa_offset 16
	.cfi_offset %rbx, -16
	xorl	%eax, %eax
	popq	%rbx
	.cfi_def_cfa_offset 8
	.cfi_restore %rbx
                                        # kill: def $al killed $al killed $eax
	retq
.Lfunc_end48:
	.size	qc_to_bool_from_string, .Lfunc_end48-qc_to_bool_from_string
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_qbool_from_string
	.type	qc_to_qbool_from_string,@function
qc_to_qbool_from_string:                # @qc_to_qbool_from_string
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	je	.LBB49_4
# %bb.1:                                # %if.end
	pushq	%rbx
	.cfi_def_cfa_offset 16
	.cfi_offset %rbx, -16
	movq	%rdi, %rbx
	movl	$.L.str.28, %esi
	callq	strcmp@PLT
	testl	%eax, %eax
	je	.LBB49_5
# %bb.2:                                # %if.end2
	movl	$.L.str.27, %esi
	movq	%rbx, %rdi
	callq	strcmp@PLT
	testl	%eax, %eax
	je	.LBB49_7
# %bb.3:                                # %if.end6
	movl	$.L.str.29, %esi
	movq	%rbx, %rdi
	callq	strcmp@PLT
	xorl	%ecx, %ecx
	testl	%eax, %eax
	sete	%cl
	leal	(%rcx,%rcx,2), %eax
	popq	%rbx
	.cfi_def_cfa_offset 8
	.cfi_restore %rbx
                                        # kill: def $al killed $al killed $eax
	retq
.LBB49_4:
	xorl	%eax, %eax
                                        # kill: def $al killed $al killed $eax
	retq
.LBB49_5:
	.cfi_def_cfa_offset 16
	.cfi_offset %rbx, -16
	movb	$2, %al
	popq	%rbx
	.cfi_def_cfa_offset 8
	.cfi_restore %rbx
                                        # kill: def $al killed $al killed $eax
	retq
.LBB49_7:
	.cfi_def_cfa_offset 16
	.cfi_offset %rbx, -16
	movb	$1, %al
	popq	%rbx
	.cfi_def_cfa_offset 8
	.cfi_restore %rbx
                                        # kill: def $al killed $al killed $eax
	retq
.Lfunc_end49:
	.size	qc_to_qbool_from_string, .Lfunc_end49-qc_to_qbool_from_string
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_qin
	.type	qc_qin,@function
qc_qin:                                 # @qc_qin
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%rbx
	.cfi_def_cfa_offset 32
	subq	$1024, %rsp                     # imm = 0x400
	.cfi_def_cfa_offset 1056
	.cfi_offset %rbx, -32
	.cfi_offset %r14, -24
	.cfi_offset %rbp, -16
	movq	stdout@GOTPCREL(%rip), %rax
	movq	(%rax), %rdi
	callq	fflush@PLT
	movq	stderr@GOTPCREL(%rip), %r14
	movq	(%r14), %rdi
	callq	fflush@PLT
	movq	%rsp, %rbx
	movl	$.L.str.40, %edi
	movq	%rbx, %rsi
	xorl	%eax, %eax
	callq	__isoc23_scanf@PLT
	movl	%eax, %ebp
	movq	(%r14), %rdi
	callq	fflush@PLT
	cmpl	$1, %ebp
	movl	$.L.str.2, %edi
	cmoveq	%rbx, %rdi
	callq	strdup@PLT
	addq	$1024, %rsp                     # imm = 0x400
	.cfi_def_cfa_offset 32
	popq	%rbx
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end50:
	.size	qc_qin, .Lfunc_end50-qc_qin
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_trim
	.type	qc_trim,@function
qc_trim:                                # @qc_trim
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	je	.LBB51_1
# %bb.3:                                # %while.cond.preheader
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	pushq	%rax
	.cfi_def_cfa_offset 32
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	movq	%rdi, %rbx
	movzbl	(%rdi), %eax
	testb	%al, %al
	je	.LBB51_7
	.p2align	4
.LBB51_5:                               # %land.rhs
                                        # =>This Inner Loop Header: Depth=1
	movzbl	%al, %edi
	callq	isspace@PLT
	testl	%eax, %eax
	je	.LBB51_8
# %bb.6:                                # %while.body
                                        #   in Loop: Header=BB51_5 Depth=1
	movzbl	1(%rbx), %eax
	incq	%rbx
	testb	%al, %al
	jne	.LBB51_5
.LBB51_7:                               # %if.then4
	movl	$1, %edi
	callq	malloc@PLT
	movb	$0, (%rax)
	jmp	.LBB51_14
.LBB51_1:
	.cfi_def_cfa_offset 8
	.cfi_restore %rbx
	.cfi_restore %r14
	xorl	%eax, %eax
	retq
.LBB51_8:                               # %if.end6
	.cfi_def_cfa_offset 32
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	movq	%rbx, %rdi
	callq	strlen@PLT
	.p2align	4
.LBB51_9:                               # %while.cond9
                                        # =>This Inner Loop Header: Depth=1
	movq	%rax, %r14
	addq	%rbx, %rax
	decq	%rax
	cmpq	%rbx, %rax
	jbe	.LBB51_11
# %bb.10:                               # %land.rhs11
                                        #   in Loop: Header=BB51_9 Depth=1
	movzbl	(%rax), %edi
	callq	isspace@PLT
	movl	%eax, %ecx
	leaq	-1(%r14), %rax
	testl	%ecx, %ecx
	jne	.LBB51_9
.LBB51_11:                              # %while.end18
	leaq	1(%r14), %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB51_12
# %bb.13:                               # %if.end23
	movq	%rax, %rdi
	movq	%rbx, %rsi
	movq	%r14, %rdx
	movq	%rax, %rbx
	callq	memcpy@PLT
	movq	%rbx, %rax
	movb	$0, (%rbx,%r14)
	jmp	.LBB51_14
.LBB51_12:
	xorl	%eax, %eax
.LBB51_14:
	addq	$8, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	.cfi_restore %rbx
	.cfi_restore %r14
	retq
.Lfunc_end51:
	.size	qc_trim, .Lfunc_end51-qc_trim
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_replace
	.type	qc_replace,@function
qc_replace:                             # @qc_replace
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	pushq	%r15
	.cfi_def_cfa_offset 24
	pushq	%r14
	.cfi_def_cfa_offset 32
	pushq	%r13
	.cfi_def_cfa_offset 40
	pushq	%r12
	.cfi_def_cfa_offset 48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	subq	$24, %rsp
	.cfi_def_cfa_offset 80
	.cfi_offset %rbx, -56
	.cfi_offset %r12, -48
	.cfi_offset %r13, -40
	.cfi_offset %r14, -32
	.cfi_offset %r15, -24
	.cfi_offset %rbp, -16
	testq	%rdi, %rdi
	sete	%al
	testq	%rsi, %rsi
	sete	%cl
	orb	%al, %cl
	testq	%rdx, %rdx
	sete	%al
	orb	%cl, %al
	jne	.LBB52_12
# %bb.1:                                # %if.end
	movq	%rdx, %r14
	movq	%rsi, %r15
	movq	%rdi, %rbx
	movq	%rsi, %rdi
	callq	strlen@PLT
	movq	%rax, %r12
	movq	%r14, %rdi
	callq	strlen@PLT
	movq	%rax, 16(%rsp)                  # 8-byte Spill
	testq	%r12, %r12
	je	.LBB52_14
# %bb.2:                                # %while.cond.preheader
	movq	%rbx, %rdi
	movq	%r15, %rsi
	callq	strstr@PLT
	testq	%rax, %rax
	je	.LBB52_14
# %bb.3:                                # %while.body.preheader
	movq	16(%rsp), %r13                  # 8-byte Reload
	subq	%r12, %r13
	movl	$1, %ebp
	.p2align	4
.LBB52_4:                               # %while.body
                                        # =>This Inner Loop Header: Depth=1
	addq	%r13, %rbp
	addq	%r12, %rax
	movq	%rax, %rdi
	movq	%r15, %rsi
	callq	strstr@PLT
	testq	%rax, %rax
	jne	.LBB52_4
# %bb.5:                                # %if.end19
	movq	%rbx, %rdi
	callq	strlen@PLT
	addq	%rax, %rbp
	movq	%rbp, %rdi
	callq	malloc@PLT
	testq	%rax, %rax
	je	.LBB52_12
# %bb.6:                                # %while.cond27.preheader
	movzbl	(%rbx), %r13d
	movq	%rax, 8(%rsp)                   # 8-byte Spill
	movq	%rax, %rbp
	testb	%r13b, %r13b
	je	.LBB52_15
# %bb.7:                                # %while.body29.preheader
	movq	8(%rsp), %rbp                   # 8-byte Reload
	jmp	.LBB52_10
	.p2align	4
.LBB52_8:                               # %if.else
                                        #   in Loop: Header=BB52_10 Depth=1
	incq	%rbx
	movb	%r13b, (%rbp)
	incq	%rbp
.LBB52_9:                               # %if.end36
                                        #   in Loop: Header=BB52_10 Depth=1
	movzbl	(%rbx), %r13d
	testb	%r13b, %r13b
	je	.LBB52_15
.LBB52_10:                              # %while.body29
                                        # =>This Inner Loop Header: Depth=1
	movq	%r15, %rdi
	callq	strlen@PLT
	movq	%rbx, %rdi
	movq	%r15, %rsi
	movq	%rax, %rdx
	callq	strncmp@PLT
	testl	%eax, %eax
	jne	.LBB52_8
# %bb.11:                               # %if.then32
                                        #   in Loop: Header=BB52_10 Depth=1
	movq	%rbp, %rdi
	movq	%r14, %rsi
	movq	16(%rsp), %r13                  # 8-byte Reload
	movq	%r13, %rdx
	callq	memcpy@PLT
	addq	%r13, %rbp
	addq	%r12, %rbx
	jmp	.LBB52_9
.LBB52_12:
	xorl	%eax, %eax
.LBB52_13:                              # %return
	addq	$24, %rsp
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%r12
	.cfi_def_cfa_offset 40
	popq	%r13
	.cfi_def_cfa_offset 32
	popq	%r14
	.cfi_def_cfa_offset 24
	popq	%r15
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	retq
.LBB52_14:                              # %if.then5
	.cfi_def_cfa_offset 80
	movq	%rbx, %rdi
	callq	strlen@PLT
	leaq	1(%rax), %rdi
	callq	malloc@PLT
	movq	%rax, %rdi
	movq	%rbx, %rsi
	addq	$24, %rsp
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%r12
	.cfi_def_cfa_offset 40
	popq	%r13
	.cfi_def_cfa_offset 32
	popq	%r14
	.cfi_def_cfa_offset 24
	popq	%r15
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	jmp	strcpy@PLT                      # TAILCALL
.LBB52_15:                              # %while.end37
	.cfi_def_cfa_offset 80
	movb	$0, (%rbp)
	movq	8(%rsp), %rax                   # 8-byte Reload
	jmp	.LBB52_13
.Lfunc_end52:
	.size	qc_replace, .Lfunc_end52-qc_replace
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_int_from_string
	.type	qc_to_int_from_string,@function
qc_to_int_from_string:                  # @qc_to_int_from_string
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	je	.LBB53_1
# %bb.2:                                # %cond.true
	pushq	%rax
	.cfi_def_cfa_offset 16
	xorl	%esi, %esi
	movl	$10, %edx
	callq	__isoc23_strtol@PLT
	addq	$8, %rsp
	.cfi_def_cfa_offset 8
                                        # kill: def $eax killed $eax killed $rax
	retq
.LBB53_1:
	xorl	%eax, %eax
                                        # kill: def $eax killed $eax killed $rax
	retq
.Lfunc_end53:
	.size	qc_to_int_from_string, .Lfunc_end53-qc_to_int_from_string
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_short_int_from_string
	.type	qc_to_short_int_from_string,@function
qc_to_short_int_from_string:            # @qc_to_short_int_from_string
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	je	.LBB54_1
# %bb.2:                                # %cond.true
	pushq	%rax
	.cfi_def_cfa_offset 16
	xorl	%esi, %esi
	movl	$10, %edx
	callq	__isoc23_strtol@PLT
	addq	$8, %rsp
	.cfi_def_cfa_offset 8
                                        # kill: def $ax killed $ax killed $rax
	retq
.LBB54_1:
	xorl	%eax, %eax
                                        # kill: def $ax killed $ax killed $rax
	retq
.Lfunc_end54:
	.size	qc_to_short_int_from_string, .Lfunc_end54-qc_to_short_int_from_string
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_long_int_from_string
	.type	qc_to_long_int_from_string,@function
qc_to_long_int_from_string:             # @qc_to_long_int_from_string
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	je	.LBB55_1
# %bb.2:                                # %cond.true
	xorl	%esi, %esi
	movl	$10, %edx
	jmp	__isoc23_strtoll@PLT            # TAILCALL
.LBB55_1:                               # %cond.end
	xorl	%eax, %eax
	retq
.Lfunc_end55:
	.size	qc_to_long_int_from_string, .Lfunc_end55-qc_to_long_int_from_string
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_addr_t_from_string
	.type	qc_to_addr_t_from_string,@function
qc_to_addr_t_from_string:               # @qc_to_addr_t_from_string
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	je	.LBB56_1
# %bb.2:                                # %cond.true
	xorl	%esi, %esi
	movl	$10, %edx
	jmp	__isoc23_strtoull@PLT           # TAILCALL
.LBB56_1:                               # %cond.end
	xorl	%eax, %eax
	retq
.Lfunc_end56:
	.size	qc_to_addr_t_from_string, .Lfunc_end56-qc_to_addr_t_from_string
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_nibble_from_string
	.type	qc_to_nibble_from_string,@function
qc_to_nibble_from_string:               # @qc_to_nibble_from_string
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	je	.LBB57_1
# %bb.2:                                # %cond.true
	pushq	%rax
	.cfi_def_cfa_offset 16
	xorl	%esi, %esi
	movl	$10, %edx
	callq	__isoc23_strtoull@PLT
	andb	$15, %al
	addq	$8, %rsp
	.cfi_def_cfa_offset 8
                                        # kill: def $al killed $al killed $rax
	retq
.LBB57_1:
	xorl	%eax, %eax
                                        # kill: def $al killed $al killed $rax
	retq
.Lfunc_end57:
	.size	qc_to_nibble_from_string, .Lfunc_end57-qc_to_nibble_from_string
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_byte_from_string
	.type	qc_to_byte_from_string,@function
qc_to_byte_from_string:                 # @qc_to_byte_from_string
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	je	.LBB58_1
# %bb.2:                                # %cond.true
	pushq	%rax
	.cfi_def_cfa_offset 16
	xorl	%esi, %esi
	movl	$10, %edx
	callq	__isoc23_strtoull@PLT
	addq	$8, %rsp
	.cfi_def_cfa_offset 8
                                        # kill: def $al killed $al killed $rax
	retq
.LBB58_1:
	xorl	%eax, %eax
                                        # kill: def $al killed $al killed $rax
	retq
.Lfunc_end58:
	.size	qc_to_byte_from_string, .Lfunc_end58-qc_to_byte_from_string
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_float_from_string
	.type	qc_to_float_from_string,@function
qc_to_float_from_string:                # @qc_to_float_from_string
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	je	.LBB59_1
# %bb.2:                                # %cond.true
	pushq	%rax
	.cfi_def_cfa_offset 16
	xorl	%esi, %esi
	callq	strtod@PLT
	cvtsd2ss	%xmm0, %xmm0
	addq	$8, %rsp
	.cfi_def_cfa_offset 8
	retq
.LBB59_1:
	xorps	%xmm0, %xmm0
	retq
.Lfunc_end59:
	.size	qc_to_float_from_string, .Lfunc_end59-qc_to_float_from_string
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_double_from_string
	.type	qc_to_double_from_string,@function
qc_to_double_from_string:               # @qc_to_double_from_string
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	je	.LBB60_1
# %bb.2:                                # %cond.true
	xorl	%esi, %esi
	jmp	strtod@PLT                      # TAILCALL
.LBB60_1:                               # %cond.end
	xorps	%xmm0, %xmm0
	retq
.Lfunc_end60:
	.size	qc_to_double_from_string, .Lfunc_end60-qc_to_double_from_string
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_to_char_from_string
	.type	qc_to_char_from_string,@function
qc_to_char_from_string:                 # @qc_to_char_from_string
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rdi, %rdi
	je	.LBB61_1
# %bb.2:                                # %land.lhs.true
	movzbl	(%rdi), %eax
                                        # kill: def $al killed $al killed $eax
	retq
.LBB61_1:
	xorl	%eax, %eax
                                        # kill: def $al killed $al killed $eax
	retq
.Lfunc_end61:
	.size	qc_to_char_from_string, .Lfunc_end61-qc_to_char_from_string
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_open
	.type	qc_open,@function
qc_open:                                # @qc_open
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rax
	.cfi_def_cfa_offset 16
	callq	fopen@PLT
	testq	%rax, %rax
	je	.LBB62_1
# %bb.2:                                # %if.end
	movq	%rax, %rdi
	popq	%rax
	.cfi_def_cfa_offset 8
	jmp	fileno@PLT                      # TAILCALL
.LBB62_1:                               # %cleanup
	.cfi_def_cfa_offset 16
	movl	$-1, %eax
	popq	%rcx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end62:
	.size	qc_open, .Lfunc_end62-qc_open
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_close
	.type	qc_close,@function
qc_close:                               # @qc_close
	.cfi_startproc
# %bb.0:                                # %entry
	jmp	close@PLT                       # TAILCALL
.Lfunc_end63:
	.size	qc_close, .Lfunc_end63-qc_close
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_read
	.type	qc_read,@function
qc_read:                                # @qc_read
	.cfi_startproc
# %bb.0:                                # %entry
	jmp	read@PLT                        # TAILCALL
.Lfunc_end64:
	.size	qc_read, .Lfunc_end64-qc_read
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_lseek
	.type	qc_lseek,@function
qc_lseek:                               # @qc_lseek
	.cfi_startproc
# %bb.0:                                # %entry
	jmp	lseek@PLT                       # TAILCALL
.Lfunc_end65:
	.size	qc_lseek, .Lfunc_end65-qc_lseek
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_write
	.type	qc_write,@function
qc_write:                               # @qc_write
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rsi, %rsi
	je	.LBB66_5
# %bb.1:                                # %if.end
	pushq	%rbp
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%rbx
	.cfi_def_cfa_offset 32
	.cfi_offset %rbx, -32
	.cfi_offset %r14, -24
	.cfi_offset %rbp, -16
	movq	%rsi, %rbx
	movl	%edi, %ebp
	movq	%rsi, %rdi
	callq	strlen@PLT
	movq	%rax, %r14
	.p2align	4
.LBB66_2:                               # %while.cond
                                        # =>This Inner Loop Header: Depth=1
	testq	%r14, %r14
	je	.LBB66_4
# %bb.3:                                # %while.body
                                        #   in Loop: Header=BB66_2 Depth=1
	movl	%ebp, %edi
	movq	%rbx, %rsi
	movq	%r14, %rdx
	callq	write@PLT
	addq	%rax, %rbx
	subq	%rax, %r14
	testq	%rax, %rax
	jg	.LBB66_2
.LBB66_4:
	popq	%rbx
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	.cfi_restore %rbx
	.cfi_restore %r14
	.cfi_restore %rbp
.LBB66_5:                               # %cleanup.cont7
	retq
.Lfunc_end66:
	.size	qc_write, .Lfunc_end66-qc_write
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_write_sized
	.type	qc_write_sized,@function
qc_write_sized:                         # @qc_write_sized
	.cfi_startproc
# %bb.0:                                # %entry
	testq	%rsi, %rsi
	jne	write@PLT                       # TAILCALL
# %bb.1:                                # %return
	movq	$-1, %rax
	retq
.Lfunc_end67:
	.size	qc_write_sized, .Lfunc_end67-qc_write_sized
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_variadic_is_empty
	.type	qc_variadic_is_empty,@function
qc_variadic_is_empty:                   # @qc_variadic_is_empty
	.cfi_startproc
# %bb.0:                                # %entry
	movl	12(%rdi), %eax
	cmpl	8(%rdi), %eax
	setge	%al
	retq
.Lfunc_end68:
	.size	qc_variadic_is_empty, .Lfunc_end68-qc_variadic_is_empty
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function qc_variadic_next
	.type	qc_variadic_next,@function
qc_variadic_next:                       # @qc_variadic_next
	.cfi_startproc
# %bb.0:                                # %entry
	movslq	12(%rdi), %rcx
	cmpl	8(%rdi), %ecx
	jge	.LBB69_1
# %bb.3:                                # %if.end
	movq	(%rdi), %rax
	movq	(%rax,%rcx,8), %rax
	incl	%ecx
	movl	%ecx, 12(%rdi)
	retq
.LBB69_1:
	xorl	%eax, %eax
	retq
.Lfunc_end69:
	.size	qc_variadic_next, .Lfunc_end69-qc_variadic_next
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function __qc_exception_get_value
	.type	__qc_exception_get_value,@function
__qc_exception_get_value:               # @__qc_exception_get_value
	.cfi_startproc
# %bb.0:                                # %entry
	movq	40(%rdi), %rax
	retq
.Lfunc_end70:
	.size	__qc_exception_get_value, .Lfunc_end70-__qc_exception_get_value
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function __qc_create_exception
	.type	__qc_create_exception,@function
__qc_create_exception:                  # @__qc_create_exception
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r15
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%rbx
	.cfi_def_cfa_offset 32
	.cfi_offset %rbx, -32
	.cfi_offset %r14, -24
	.cfi_offset %r15, -16
	movq	%rsi, %rbx
	movq	%rdi, %r14
	movl	$48, %edi
	callq	malloc@PLT
	movq	%rax, %r15
	movabsq	$5855600185899831380, %rax      # imm = 0x5143455843455054
	movq	%rax, (%r15)
	movq	$_ZL12__qc_cleanup19_Unwind_Reason_CodeP17_Unwind_Exception, 8(%r15)
	movq	%r14, %rdi
	callq	strdup@PLT
	movq	%rax, 32(%r15)
	movq	%rbx, 40(%r15)
	movq	%r15, %rax
	popq	%rbx
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end71:
	.size	__qc_create_exception, .Lfunc_end71-__qc_create_exception
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function _ZL12__qc_cleanup19_Unwind_Reason_CodeP17_Unwind_Exception
	.type	_ZL12__qc_cleanup19_Unwind_Reason_CodeP17_Unwind_Exception,@function
_ZL12__qc_cleanup19_Unwind_Reason_CodeP17_Unwind_Exception: # @_ZL12__qc_cleanup19_Unwind_Reason_CodeP17_Unwind_Exception
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbx
	.cfi_def_cfa_offset 16
	.cfi_offset %rbx, -16
	movq	%rsi, %rbx
	movq	32(%rsi), %rdi
	callq	free@PLT
	movq	%rbx, %rdi
	popq	%rbx
	.cfi_def_cfa_offset 8
	jmp	free@PLT                        # TAILCALL
.Lfunc_end72:
	.size	_ZL12__qc_cleanup19_Unwind_Reason_CodeP17_Unwind_Exception, .Lfunc_end72-_ZL12__qc_cleanup19_Unwind_Reason_CodeP17_Unwind_Exception
	.cfi_endproc
                                        # -- End function
	.section	.text.unlikely.,"ax",@progbits
	.p2align	4                               # -- Begin function __qc_throw
	.type	__qc_throw,@function
__qc_throw:                             # @__qc_throw
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbx
	.cfi_def_cfa_offset 16
	.cfi_offset %rbx, -16
	movq	%rdi, %rbx
	callq	_Unwind_RaiseException@PLT
	movq	stderr@GOTPCREL(%rip), %rax
	movq	(%rax), %rdi
	movq	32(%rbx), %rdx
	movl	$.L.str.41, %esi
	xorl	%eax, %eax
	callq	fprintf@PLT
	callq	abort@PLT
.Lfunc_end73:
	.size	__qc_throw, .Lfunc_end73-__qc_throw
	.cfi_endproc
                                        # -- End function
	.text
	.p2align	4                               # -- Begin function __qc_personality
	.type	__qc_personality,@function
__qc_personality:                       # @__qc_personality
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	pushq	%r15
	.cfi_def_cfa_offset 24
	pushq	%r14
	.cfi_def_cfa_offset 32
	pushq	%r13
	.cfi_def_cfa_offset 40
	pushq	%r12
	.cfi_def_cfa_offset 48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	subq	$104, %rsp
	.cfi_def_cfa_offset 160
	.cfi_offset %rbx, -56
	.cfi_offset %r12, -48
	.cfi_offset %r13, -40
	.cfi_offset %r14, -32
	.cfi_offset %r15, -24
	.cfi_offset %rbp, -16
	movl	$3, %r14d
	cmpl	$1, %edi
	jne	.LBB74_52
# %bb.1:                                # %if.end
	movl	$8, %r14d
	movabsq	$5855600185899831380, %rax      # imm = 0x5143455843455054
	cmpq	%rax, %rdx
	jne	.LBB74_52
# %bb.2:                                # %if.end3
	movq	%r8, %r15
	movl	%esi, %ebp
	movq	%r8, %rdi
	movq	%rcx, %rbx
	callq	_Unwind_GetLanguageSpecificData@PLT
	testq	%rax, %rax
	je	.LBB74_52
# %bb.3:                                # %if.end5
	movq	%rbx, %rcx
	movq	%rax, %rbx
	movq	%rcx, 56(%rsp)                  # 8-byte Spill
	movq	%rax, %r14
	movq	%r15, %rdi
	callq	_Unwind_GetRegionStart@PLT
	movq	%rax, %r12
	movq	%r15, %rdi
	callq	_Unwind_GetIP@PLT
	movq	%rax, %r13
	cmpq	$1, %rax
	adcq	$-1, %r13
	incq	%rbx
	movq	%rbx, 8(%rsp)
	movzbl	(%r14), %ecx
	movq	%r12, %rax
	cmpb	$-1, %cl
	je	.LBB74_5
# %bb.4:                                # %if.then12
	movzbl	%cl, %esi
	leaq	8(%rsp), %rdi
	movq	%r12, %rdx
	callq	_ZL17__qc_read_encodedPPhhmmm
	movq	8(%rsp), %rbx
.LBB74_5:                               # %if.end14
	movq	%rax, 24(%rsp)                  # 8-byte Spill
	leaq	1(%rbx), %rax
	movq	%rax, 8(%rsp)
	movzbl	(%rbx), %ecx
	movzwl	%cx, %ecx
	movq	%rcx, 80(%rsp)                  # 8-byte Spill
	cmpl	$255, %ecx
	movq	%r15, 72(%rsp)                  # 8-byte Spill
	movl	%ebp, 20(%rsp)                  # 4-byte Spill
	je	.LBB74_6
# %bb.7:                                # %while.cond.i.preheader
	xorl	%ecx, %ecx
	xorl	%edi, %edi
	.p2align	4
.LBB74_8:                               # %while.cond.i
                                        # =>This Inner Loop Header: Depth=1
	movzbl	(%rax), %edx
	movl	%edx, %esi
	andl	$127, %edx
	shlq	%cl, %rdx
	incq	%rax
	orq	%rdx, %rdi
	addq	$7, %rcx
	testb	%sil, %sil
	js	.LBB74_8
# %bb.9:                                # %_ZL17__qc_read_uleb128PPh.exit
	addq	%rax, %rdi
	jmp	.LBB74_10
.LBB74_6:
	xorl	%edi, %edi
.LBB74_10:                              # %if.end20
	movq	%rdi, 88(%rsp)                  # 8-byte Spill
	leaq	1(%rax), %r15
	movq	%r15, 8(%rsp)
	movzbl	(%rax), %eax
	xorl	%ecx, %ecx
	xorl	%ebp, %ebp
	.p2align	4
.LBB74_11:                              # %while.cond.i190
                                        # =>This Inner Loop Header: Depth=1
	movzbl	(%r15), %edx
	movl	%edx, %esi
	andl	$127, %edx
	shlq	%cl, %rdx
	incq	%r15
	orq	%rdx, %rbp
	addq	$7, %rcx
	testb	%sil, %sil
	js	.LBB74_11
# %bb.12:                               # %_ZL17__qc_read_uleb128PPh.exit199
	movq	%r15, 8(%rsp)
	movq	%r12, 64(%rsp)                  # 8-byte Spill
	subq	%r12, %r13
	movq	%r15, (%rsp)
	movzbl	%al, %eax
	movl	%eax, 32(%rsp)                  # 4-byte Spill
	movq	%r15, %rsi
	movq	%r13, 40(%rsp)                  # 8-byte Spill
	movl	32(%rsp), %r14d                 # 4-byte Reload
	.p2align	4
.LBB74_13:                              # %for.cond
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB74_16 Depth 2
	leaq	(%r15,%rbp), %rax
	cmpq	%rax, %rsi
	jae	.LBB74_14
# %bb.15:                               # %for.body
                                        #   in Loop: Header=BB74_13 Depth=1
	movq	%rsp, %r12
	movq	%r12, %rdi
	movl	%r14d, %esi
	movq	64(%rsp), %r13                  # 8-byte Reload
	movq	%r13, %rdx
	callq	_ZL17__qc_read_encodedPPhhmmm
	movq	%rax, 48(%rsp)                  # 8-byte Spill
	movq	%r12, %rdi
	movl	%r14d, %esi
	movq	%r13, %rdx
	callq	_ZL17__qc_read_encodedPPhhmmm
	movq	%rax, %rbx
	movq	%r12, %rdi
	movl	%r14d, %esi
	movq	%r13, %rdx
	callq	_ZL17__qc_read_encodedPPhhmmm
	movq	(%rsp), %rsi
	xorl	%ecx, %ecx
	xorl	%edx, %edx
	.p2align	4
.LBB74_16:                              # %while.cond.i201
                                        #   Parent Loop BB74_13 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	movzbl	(%rsi), %edi
	movl	%edi, %r8d
	andl	$127, %edi
	shlq	%cl, %rdi
	incq	%rsi
	orq	%rdi, %rdx
	addq	$7, %rcx
	testb	%r8b, %r8b
	js	.LBB74_16
# %bb.17:                               # %_ZL17__qc_read_uleb128PPh.exit210
                                        #   in Loop: Header=BB74_13 Depth=1
	movq	%rsi, (%rsp)
	movq	40(%rsp), %r13                  # 8-byte Reload
	movq	48(%rsp), %rcx                  # 8-byte Reload
	cmpq	%rcx, %r13
	jb	.LBB74_13
# %bb.18:                               # %_ZL17__qc_read_uleb128PPh.exit210
                                        #   in Loop: Header=BB74_13 Depth=1
	addq	%rcx, %rbx
	cmpq	%rbx, %r13
	jae	.LBB74_13
# %bb.19:                               # %cleanup37.split.loop.exit282
	addq	24(%rsp), %rax                  # 8-byte Folded Reload
	jmp	.LBB74_20
.LBB74_14:
	xorl	%edx, %edx
	xorl	%eax, %eax
.LBB74_20:                              # %cleanup37
	movl	$8, %r14d
	testq	%rax, %rax
	je	.LBB74_52
# %bb.21:                               # %cleanup37
	testq	%rdx, %rdx
	je	.LBB74_52
# %bb.22:                               # %if.end41
	movq	%rax, 24(%rsp)                  # 8-byte Spill
	addq	%rbp, %rdx
	movq	80(%rsp), %rbx                  # 8-byte Reload
                                        # kill: def $ebx killed $ebx killed $rbx def $rbx
	andl	$15, %ebx
	movl	$7197, %eax                     # imm = 0x1C1D
	movl	%ebx, %ecx
	shrl	%cl, %eax
	leaq	(%r15,%rdx), %r12
	decq	%r12
	xorl	%r14d, %r14d
	xorb	$1, %al
	movl	%eax, 64(%rsp)                  # 4-byte Spill
	movq	%rsp, %r13
	movq	$0, 48(%rsp)                    # 8-byte Folded Spill
	movl	$0, 40(%rsp)                    # 4-byte Folded Spill
	movq	$0, 32(%rsp)                    # 8-byte Folded Spill
	.p2align	4
.LBB74_23:                              # %while.cond
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB74_24 Depth 2
                                        #     Child Loop BB74_26 Depth 2
	movq	$-7, %rax
	xorl	%edi, %edi
	.p2align	4
.LBB74_24:                              # %do.body.i
                                        #   Parent Loop BB74_23 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	addq	$7, %rax
	movzbl	(%r12), %edx
	movl	%edx, %r8d
	andl	$127, %edx
	movl	%eax, %ecx
	shlq	%cl, %rdx
	incq	%r12
	orq	%rdx, %rdi
	testb	%r8b, %r8b
	js	.LBB74_24
# %bb.25:                               # %do.body.i221.preheader
                                        #   in Loop: Header=BB74_23 Depth=1
	leaq	7(%rax), %rsi
	movq	$-7, %rdx
	movq	%r12, %r10
	xorl	%r15d, %r15d
	.p2align	4
.LBB74_26:                              # %do.body.i221
                                        #   Parent Loop BB74_23 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	addq	$7, %rdx
	movzbl	(%r10), %r11d
	movl	%r11d, %r9d
	andl	$127, %r11d
	movl	%edx, %ecx
	shlq	%cl, %r11
	incq	%r10
	orq	%r11, %r15
	testb	%r9b, %r9b
	js	.LBB74_26
# %bb.27:                               # %_ZL17__qc_read_sleb128PPh.exit237
                                        #   in Loop: Header=BB74_23 Depth=1
	movq	$-1, %r10
	movl	%esi, %ecx
	shlq	%cl, %r10
	cmpb	$64, %r8b
	cmovbq	%r14, %r10
	cmpq	$57, %rax
	cmovaeq	%r14, %r10
	leal	7(%rdx), %ecx
	movq	$-1, %rbp
                                        # kill: def $cl killed $cl killed $ecx
	shlq	%cl, %rbp
	cmpb	$64, %r9b
	cmovbq	%r14, %rbp
	cmpq	$57, %rdx
	cmovaeq	%r14, %rbp
	orq	%r10, %rdi
	je	.LBB74_28
# %bb.29:                               # %if.else
                                        #   in Loop: Header=BB74_23 Depth=1
	jle	.LBB74_38
# %bb.30:                               # %if.else
                                        #   in Loop: Header=BB74_23 Depth=1
	cmpq	$0, 88(%rsp)                    # 8-byte Folded Reload
	je	.LBB74_38
# %bb.31:                               # %if.then52
                                        #   in Loop: Header=BB74_23 Depth=1
	cmpb	$12, %bl
	ja	.LBB74_53
# %bb.32:                               # %if.then52
                                        #   in Loop: Header=BB74_23 Depth=1
	testb	$1, 64(%rsp)                    # 1-byte Folded Reload
	jne	.LBB74_53
# %bb.33:                               # %switch.lookup
                                        #   in Loop: Header=BB74_23 Depth=1
	movq	%rbx, %r14
	movq	.Lswitch.table.__qc_personality(,%rbx,8), %rdx
	imulq	%rdx, %rdi
	movq	88(%rsp), %rbx                  # 8-byte Reload
	subq	%rdi, %rbx
	movq	%rbx, (%rsp)
	movq	$0, 96(%rsp)
	leaq	96(%rsp), %rdi
	movq	%rbx, %rsi
	callq	memcpy@PLT
	cmpq	$0, 96(%rsp)
	je	.LBB74_34
# %bb.35:                               # %if.else61
                                        #   in Loop: Header=BB74_23 Depth=1
	incl	40(%rsp)                        # 4-byte Folded Spill
	movq	%r13, %rdi
	movq	80(%rsp), %rsi                  # 8-byte Reload
                                        # kill: def $esi killed $esi killed $rsi
	movq	%rbx, %rdx
	callq	_ZL17__qc_read_encodedPPhhmmm
	testq	%rax, %rax
	je	.LBB74_37
# %bb.36:                               # %land.lhs.true64
                                        #   in Loop: Header=BB74_23 Depth=1
	movq	56(%rsp), %rcx                  # 8-byte Reload
	movq	32(%rcx), %rdi
	movq	%rax, %rsi
	callq	strcmp@PLT
	testl	%eax, %eax
	jne	.LBB74_37
	jmp	.LBB74_41
	.p2align	4
.LBB74_28:                              #   in Loop: Header=BB74_23 Depth=1
	movb	$1, %al
	movq	%rax, 48(%rsp)                  # 8-byte Spill
.LBB74_38:                              # %if.end80
                                        #   in Loop: Header=BB74_23 Depth=1
	orq	%rbp, %r15
	jne	.LBB74_40
	jmp	.LBB74_39
.LBB74_34:                              #   in Loop: Header=BB74_23 Depth=1
	movb	$1, %al
	movq	%rax, 32(%rsp)                  # 8-byte Spill
.LBB74_37:                              #   in Loop: Header=BB74_23 Depth=1
	movq	%r14, %rbx
	xorl	%r14d, %r14d
	orq	%rbp, %r15
	je	.LBB74_39
.LBB74_40:                              # %cleanup85
                                        #   in Loop: Header=BB74_23 Depth=1
	addq	%r15, %r12
	jmp	.LBB74_23
.LBB74_39:
	xorl	%r14d, %r14d
	xorl	%ebx, %ebx
.LBB74_42:                              # %while.end
	movl	20(%rsp), %edx                  # 4-byte Reload
	movzbl	%r14b, %ecx
	xorb	$1, %r14b
	andb	32(%rsp), %r14b                 # 1-byte Folded Reload
	movl	$1, %eax
	cmovel	%ecx, %eax
	testb	$1, %dl
	jne	.LBB74_43
# %bb.44:                               # %if.end98
	testb	$2, %dl
	jne	.LBB74_46
# %bb.45:
	movl	$8, %r14d
	jmp	.LBB74_52
.LBB74_43:                              # %if.then96
	xorb	$1, %al
	movzbl	%al, %eax
	leal	6(,%rax,2), %r14d
	jmp	.LBB74_52
.LBB74_46:                              # %if.then101
	testb	$4, %dl
	je	.LBB74_49
# %bb.47:                               # %if.then101
	testb	%al, %al
	je	.LBB74_49
# %bb.48:                               # %if.then106
	xorl	%r15d, %r15d
	movq	72(%rsp), %r12                  # 8-byte Reload
	movq	%r12, %rdi
	xorl	%esi, %esi
	movq	56(%rsp), %rdx                  # 8-byte Reload
	callq	_Unwind_SetGR@PLT
	testb	%r14b, %r14b
	cmovneq	%r15, %rbx
	movq	%r12, %rdi
	movl	$1, %esi
	movq	%rbx, %rdx
	callq	_Unwind_SetGR@PLT
	movq	%r12, %rdi
	jmp	.LBB74_51
.LBB74_49:                              # %if.end108
	testb	$1, 48(%rsp)                    # 1-byte Folded Reload
	movl	$8, %r14d
	je	.LBB74_52
# %bb.50:                               # %if.then110
	movq	72(%rsp), %rbx                  # 8-byte Reload
	movq	%rbx, %rdi
	xorl	%esi, %esi
	movq	56(%rsp), %rdx                  # 8-byte Reload
	callq	_Unwind_SetGR@PLT
	movq	%rbx, %rdi
	movl	$1, %esi
	xorl	%edx, %edx
	callq	_Unwind_SetGR@PLT
	movq	%rbx, %rdi
.LBB74_51:                              # %cleanup119
	movq	24(%rsp), %rsi                  # 8-byte Reload
	callq	_Unwind_SetIP@PLT
	movl	$7, %r14d
.LBB74_52:                              # %return
	movl	%r14d, %eax
	addq	$104, %rsp
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%r12
	.cfi_def_cfa_offset 40
	popq	%r13
	.cfi_def_cfa_offset 32
	popq	%r14
	.cfi_def_cfa_offset 24
	popq	%r15
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	retq
.LBB74_41:                              # %cleanup74
	.cfi_def_cfa_offset 160
	movslq	40(%rsp), %rbx                  # 4-byte Folded Reload
	movb	$1, %r14b
	jmp	.LBB74_42
.LBB74_53:                              # %sw.default
	callq	abort@PLT
.Lfunc_end74:
	.size	__qc_personality, .Lfunc_end74-__qc_personality
	.cfi_endproc
                                        # -- End function
	.p2align	4                               # -- Begin function _ZL17__qc_read_encodedPPhhmmm
	.type	_ZL17__qc_read_encodedPPhhmmm,@function
_ZL17__qc_read_encodedPPhhmmm:          # @_ZL17__qc_read_encodedPPhhmmm
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	pushq	%rax
	.cfi_def_cfa_offset 32
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	cmpl	$255, %esi
	je	.LBB75_6
# %bb.1:                                # %if.end
	movl	%esi, %r10d
	andb	$112, %r10b
	movq	(%rdi), %r9
	cmpb	$80, %r10b
	jne	.LBB75_3
# %bb.2:                                # %if.then5
	addq	$7, %r9
	andq	$-8, %r9
	movq	%r9, (%rdi)
	andb	$-113, %sil
	xorl	%r10d, %r10d
.LBB75_3:                               # %if.end11
	movl	%esi, %eax
	andb	$15, %al
	cmpb	$12, %al
	ja	.LBB75_25
# %bb.4:                                # %if.end11
	movzbl	%al, %eax
	jmpq	*.LJTI75_0(,%rax,8)
.LBB75_5:                               # %sw.bb
	movq	(%r9), %rax
	leaq	8(%r9), %rcx
	jmp	.LBB75_12
.LBB75_6:
	xorl	%eax, %eax
	jmp	.LBB75_18
.LBB75_7:                               # %sw.bb31
	movslq	(%r9), %rax
	leaq	4(%r9), %rcx
	jmp	.LBB75_12
.LBB75_8:                               # %sw.bb15
	movzwl	(%r9), %eax
	jmp	.LBB75_11
.LBB75_9:                               # %sw.bb18
	movl	(%r9), %eax
	leaq	4(%r9), %rcx
	jmp	.LBB75_12
.LBB75_10:                              # %sw.bb27
	movswq	(%r9), %rax
.LBB75_11:                              # %sw.epilog
	leaq	2(%r9), %rcx
.LBB75_12:                              # %sw.epilog
	movq	%rcx, (%rdi)
.LBB75_13:                              # %sw.epilog
	shrb	$4, %r10b
	cmpb	$4, %r10b
	ja	.LBB75_25
# %bb.14:                               # %sw.epilog
	movzbl	%r10b, %ecx
	jmpq	*.LJTI75_1(,%rcx,8)
.LBB75_16:                              # %sw.epilog48
	testb	%sil, %sil
	jns	.LBB75_18
.LBB75_17:                              # %if.then51
	movq	(%rax), %rax
.LBB75_18:                              # %return
	addq	$8, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	retq
.LBB75_19:                              # %sw.bb45
	.cfi_def_cfa_offset 32
	addq	%rdx, %rax
	testb	%sil, %sil
	jns	.LBB75_18
	jmp	.LBB75_17
.LBB75_15:                              # %sw.bb39
	addq	%r9, %rax
	testb	%sil, %sil
	jns	.LBB75_18
	jmp	.LBB75_17
.LBB75_20:                              # %while.cond.i.preheader
	leaq	1(%r9), %r8
	xorl	%ecx, %ecx
	xorl	%eax, %eax
	.p2align	4
.LBB75_21:                              # %while.cond.i
                                        # =>This Inner Loop Header: Depth=1
	movzbl	-1(%r8), %r11d
	movl	%r11d, %ebx
	movq	%r8, (%rdi)
	andl	$127, %r11d
	shlq	%cl, %r11
	orq	%r11, %rax
	addq	$7, %rcx
	incq	%r8
	testb	%bl, %bl
	js	.LBB75_21
	jmp	.LBB75_13
.LBB75_22:                              # %do.body.i.preheader
	leaq	1(%r9), %r11
	movq	$-7, %r8
	xorl	%eax, %eax
	.p2align	4
.LBB75_23:                              # %do.body.i
                                        # =>This Inner Loop Header: Depth=1
	addq	$7, %r8
	movzbl	-1(%r11), %r14d
	movl	%r14d, %ebx
	andl	$127, %r14d
	movl	%r8d, %ecx
	shlq	%cl, %r14
	movq	%r11, (%rdi)
	orq	%r14, %rax
	incq	%r11
	testb	%bl, %bl
	js	.LBB75_23
# %bb.24:                               # %_ZL17__qc_read_sleb128PPh.exit
	leal	7(%r8), %ecx
	movq	$-1, %rdi
                                        # kill: def $cl killed $cl killed $ecx
	shlq	%cl, %rdi
	xorl	%ecx, %ecx
	cmpb	$64, %bl
	cmovbq	%rcx, %rdi
	cmpq	$57, %r8
	cmovaeq	%rcx, %rdi
	orq	%rdi, %rax
	jmp	.LBB75_13
.LBB75_25:                              # %sw.default
	callq	abort@PLT
.Lfunc_end75:
	.size	_ZL17__qc_read_encodedPPhhmmm, .Lfunc_end75-_ZL17__qc_read_encodedPPhhmmm
	.cfi_endproc
	.section	.rodata,"a",@progbits
	.p2align	3, 0x0
.LJTI75_0:
	.quad	.LBB75_5
	.quad	.LBB75_20
	.quad	.LBB75_8
	.quad	.LBB75_9
	.quad	.LBB75_5
	.quad	.LBB75_25
	.quad	.LBB75_25
	.quad	.LBB75_25
	.quad	.LBB75_25
	.quad	.LBB75_22
	.quad	.LBB75_10
	.quad	.LBB75_7
	.quad	.LBB75_5
.LJTI75_1:
	.quad	.LBB75_16
	.quad	.LBB75_15
	.quad	.LBB75_16
	.quad	.LBB75_16
	.quad	.LBB75_19
                                        # -- End function
	.text
	.globl	"C::static_works"               # -- Begin function C::static_works
	.p2align	4
	.type	"C::static_works",@function
"C::static_works":                      # @"C::static_works"
	.cfi_startproc
# %bb.0:                                # %entry
                                        # kill: def $esi killed $esi def $rsi
                                        # kill: def $edi killed $edi def $rdi
	movl	%edi, -16(%rsp)
	movl	%esi, -12(%rsp)
	leal	(%rdi,%rsi), %eax
	retq
.Lfunc_end76:
	.size	"C::static_works", .Lfunc_end76-"C::static_works"
	.cfi_endproc
                                        # -- End function
	.globl	C_add_int_int                   # -- Begin function C_add_int_int
	.p2align	4
	.type	C_add_int_int,@function
C_add_int_int:                          # @C_add_int_int
	.cfi_startproc
# %bb.0:                                # %entry
                                        # kill: def $edx killed $edx def $rdx
                                        # kill: def $esi killed $esi def $rsi
	movl	%esi, -8(%rsp)
	movl	%edx, -4(%rsp)
	leal	(%rsi,%rdx), %eax
	retq
.Lfunc_end77:
	.size	C_add_int_int, .Lfunc_end77-C_add_int_int
	.cfi_endproc
                                        # -- End function
	.globl	C_C                             # -- Begin function C_C
	.p2align	4
	.type	C_C,@function
C_C:                                    # @C_C
	.cfi_startproc
# %bb.0:                                # %entry
	retq
.Lfunc_end78:
	.size	C_C, .Lfunc_end78-C_C
	.cfi_endproc
                                        # -- End function
	.globl	C_did_i_seriously_never_test_this # -- Begin function C_did_i_seriously_never_test_this
	.p2align	4
	.type	C_did_i_seriously_never_test_this,@function
C_did_i_seriously_never_test_this:      # @C_did_i_seriously_never_test_this
	.cfi_startproc
# %bb.0:                                # %entry
	movl	$123, %eax
	retq
.Lfunc_end79:
	.size	C_did_i_seriously_never_test_this, .Lfunc_end79-C_did_i_seriously_never_test_this
	.cfi_endproc
                                        # -- End function
	.globl	add                             # -- Begin function add
	.p2align	4
	.type	add,@function
add:                                    # @add
	.cfi_startproc
# %bb.0:                                # %entry
                                        # kill: def $esi killed $esi def $rsi
                                        # kill: def $edi killed $edi def $rdi
	movl	%edi, -8(%rsp)
	movl	%esi, -4(%rsp)
	leal	(%rdi,%rsi), %eax
	retq
.Lfunc_end80:
	.size	add, .Lfunc_end80-add
	.cfi_endproc
                                        # -- End function
	.globl	__user_entry                    # -- Begin function __user_entry
	.p2align	4
	.type	__user_entry,@function
__user_entry:                           # @__user_entry
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	pushq	%rax
	.cfi_def_cfa_offset 32
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	movq	%rsp, %rbx
	movq	%rbx, %rdi
	callq	C_C@PLT
	movq	%rbx, %rdi
	movl	$123, %esi
	movl	$321, %edx                      # imm = 0x141
	callq	C_add_int_int@PLT
	movl	$12, %edi
	movl	$23, %esi
	callq	"C::static_works"@PLT
	movslq	%eax, %r14
	movl	$.L__unnamed_1, %edi
	callq	qc_print_string
	movq	%r14, %rdi
	movl	$-1, %esi
	movl	$-1, %edx
	xorl	%ecx, %ecx
	callq	qc_fmt_int
	movq	%rax, %rdi
	callq	qc_print_string
	movl	$.L__unnamed_2, %edi
	callq	qc_print_string
	movq	%rbx, %rdi
	callq	C_did_i_seriously_never_test_this@PLT
	movslq	%eax, %rbx
	movl	$.L__unnamed_3, %edi
	callq	qc_print_string
	movq	%rbx, %rdi
	movl	$-1, %esi
	movl	$-1, %edx
	xorl	%ecx, %ecx
	callq	qc_fmt_int
	movq	%rax, %rdi
	callq	qc_print_string
	movl	$.L__unnamed_4, %edi
	callq	qc_print_string
	xorl	%edi, %edi
	xorl	%esi, %esi
	callq	add@PLT
	addq	$8, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end81:
	.size	__user_entry, .Lfunc_end81-__user_entry
	.cfi_endproc
                                        # -- End function
	.globl	main                            # -- Begin function main
	.p2align	4
	.type	main,@function
main:                                   # @main
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rax
	.cfi_def_cfa_offset 16
	callq	__user_entry@PLT
	popq	%rcx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end82:
	.size	main, .Lfunc_end82-main
	.cfi_endproc
                                        # -- End function
	.type	.L.str,@object                  # @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"%%%s%d.%dd"
	.size	.L.str, 11

	.type	.L.str.1,@object                # @.str.1
.L.str.1:
	.asciz	"0"
	.size	.L.str.1, 2

	.type	.L.str.2,@object                # @.str.2
.L.str.2:
	.zero	1
	.size	.L.str.2, 1

	.type	.L.str.3,@object                # @.str.3
.L.str.3:
	.asciz	"%%.%dd"
	.size	.L.str.3, 7

	.type	.L.str.4,@object                # @.str.4
.L.str.4:
	.asciz	"%%%s%dd"
	.size	.L.str.4, 8

	.type	.L.str.5,@object                # @.str.5
.L.str.5:
	.asciz	"%%d"
	.size	.L.str.5, 4

	.type	.L.str.6,@object                # @.str.6
.L.str.6:
	.asciz	"%%010lu"
	.size	.L.str.6, 8

	.type	.L.str.7,@object                # @.str.7
.L.str.7:
	.asciz	"%%lu"
	.size	.L.str.7, 5

	.type	.L.str.8,@object                # @.str.8
.L.str.8:
	.asciz	"%%%s%d.%df"
	.size	.L.str.8, 11

	.type	.L.str.9,@object                # @.str.9
.L.str.9:
	.asciz	"%%.%df"
	.size	.L.str.9, 7

	.type	.L.str.10,@object               # @.str.10
.L.str.10:
	.asciz	"%%%s%df"
	.size	.L.str.10, 8

	.type	.L.str.11,@object               # @.str.11
.L.str.11:
	.asciz	"%%f"
	.size	.L.str.11, 4

	.type	.L.str.12,@object               # @.str.12
.L.str.12:
	.asciz	"%%%s%d.%de"
	.size	.L.str.12, 11

	.type	.L.str.13,@object               # @.str.13
.L.str.13:
	.asciz	"%%.%de"
	.size	.L.str.13, 7

	.type	.L.str.14,@object               # @.str.14
.L.str.14:
	.asciz	"%%%s%de"
	.size	.L.str.14, 8

	.type	.L.str.15,@object               # @.str.15
.L.str.15:
	.asciz	"%%e"
	.size	.L.str.15, 4

	.type	.L.str.16,@object               # @.str.16
.L.str.16:
	.asciz	"%%%s%dc"
	.size	.L.str.16, 8

	.type	.L.str.17,@object               # @.str.17
.L.str.17:
	.asciz	"%%c"
	.size	.L.str.17, 4

	.type	.L.str.18,@object               # @.str.18
.L.str.18:
	.asciz	"%%%s%ds"
	.size	.L.str.18, 8

	.type	.L.str.19,@object               # @.str.19
.L.str.19:
	.asciz	"%%s"
	.size	.L.str.19, 4

	.type	.L.str.20,@object               # @.str.20
.L.str.20:
	.asciz	"%%%s%dx"
	.size	.L.str.20, 8

	.type	.L.str.21,@object               # @.str.21
.L.str.21:
	.asciz	"%%x"
	.size	.L.str.21, 4

	.type	.L.str.22,@object               # @.str.22
.L.str.22:
	.asciz	"%%%s%do"
	.size	.L.str.22, 8

	.type	.L.str.23,@object               # @.str.23
.L.str.23:
	.asciz	"%%o"
	.size	.L.str.23, 4

	.type	.L.str.24,@object               # @.str.24
.L.str.24:
	.asciz	"true"
	.size	.L.str.24, 5

	.type	.L.str.25,@object               # @.str.25
.L.str.25:
	.asciz	"false"
	.size	.L.str.25, 6

	.type	.L.str.26,@object               # @.str.26
.L.str.26:
	.asciz	"none"
	.size	.L.str.26, 5

	.type	.L.str.27,@object               # @.str.27
.L.str.27:
	.asciz	"qfalse"
	.size	.L.str.27, 7

	.type	.L.str.28,@object               # @.str.28
.L.str.28:
	.asciz	"qtrue"
	.size	.L.str.28, 6

	.type	.L.str.29,@object               # @.str.29
.L.str.29:
	.asciz	"both"
	.size	.L.str.29, 5

	.type	.L.str.30,@object               # @.str.30
.L.str.30:
	.asciz	"0x%0*jx"
	.size	.L.str.30, 8

	.type	.L.str.31,@object               # @.str.31
.L.str.31:
	.asciz	"%*p"
	.size	.L.str.31, 4

	.type	.L.str.32,@object               # @.str.32
.L.str.32:
	.asciz	"%p"
	.size	.L.str.32, 3

	.type	.L.str.33,@object               # @.str.33
.L.str.33:
	.asciz	"%d"
	.size	.L.str.33, 3

	.type	.L.str.34,@object               # @.str.34
.L.str.34:
	.asciz	"%g"
	.size	.L.str.34, 3

	.type	.L.str.35,@object               # @.str.35
.L.str.35:
	.asciz	"%ld"
	.size	.L.str.35, 4

	.type	.L.str.36,@object               # @.str.36
.L.str.36:
	.asciz	"%hd"
	.size	.L.str.36, 4

	.type	.L.str.37,@object               # @.str.37
.L.str.37:
	.asciz	"%lu"
	.size	.L.str.37, 4

	.type	.L.str.38,@object               # @.str.38
.L.str.38:
	.asciz	"%u"
	.size	.L.str.38, 3

	.type	.L.str.40,@object               # @.str.40
.L.str.40:
	.asciz	"%s"
	.size	.L.str.40, 3

	.type	.L.str.41,@object               # @.str.41
.L.str.41:
	.asciz	"Uncaught exception: %s\n"
	.size	.L.str.41, 24

	.type	.Lswitch.table.qc_to_string_qbool,@object # @switch.table.qc_to_string_qbool
	.section	.rodata,"a",@progbits
	.p2align	3, 0x0
.Lswitch.table.qc_to_string_qbool:
	.quad	.L.str.26
	.quad	.L.str.27
	.quad	.L.str.28
	.quad	.L.str.29
	.size	.Lswitch.table.qc_to_string_qbool, 32

	.type	.Lswitch.table.__qc_personality,@object # @switch.table.__qc_personality
	.p2align	3, 0x0
.Lswitch.table.__qc_personality:
	.quad	8                               # 0x8
	.zero	8
	.quad	2                               # 0x2
	.quad	4                               # 0x4
	.quad	8                               # 0x8
	.zero	8
	.zero	8
	.zero	8
	.zero	8
	.zero	8
	.quad	2                               # 0x2
	.quad	4                               # 0x4
	.quad	8                               # 0x8
	.size	.Lswitch.table.__qc_personality, 104

	.type	.L.str.39,@object               # @.str.39
.L.str.39:
	.asciz	"%i\n"
	.size	.L.str.39, 4

	.type	.L__unnamed_1,@object           # @0
	.section	.rodata.str1.1,"aMS",@progbits,1
.L__unnamed_1:
	.zero	1
	.size	.L__unnamed_1, 1

	.type	.L__unnamed_2,@object           # @1
	.section	.rodata,"a",@progbits
.L__unnamed_2:
	.asciz	"\n\000"
	.size	.L__unnamed_2, 3

	.type	.L.str.42,@object               # @.str.42
.L.str.42:
	.asciz	"%i\n"
	.size	.L.str.42, 4

	.type	.L__unnamed_3,@object           # @2
	.section	.rodata.str1.1,"aMS",@progbits,1
.L__unnamed_3:
	.zero	1
	.size	.L__unnamed_3, 1

	.type	.L__unnamed_4,@object           # @3
	.section	.rodata,"a",@progbits
.L__unnamed_4:
	.asciz	"\n\000"
	.size	.L__unnamed_4, 3

	.ident	"clang version 22.1.8 (https://github.com/llvm/llvm-project.git ca7933e47d3a3451d81e72ac174dcb5aa28b59d1)"
	.section	".note.GNU-stack","",@progbits
