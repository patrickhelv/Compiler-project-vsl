.section .rodata
.intout: .string "%ld "
.strout: .string "%s "
.errout: .string "Wrong number of arguments"
.STR0: .string "Hello, World!"
.section .data
.globl main
.section .text
main:
	pushq   %rbp
	movq    %rsp, %rbp
	subq	$1,%rdi
	cmpq	$0,%rdi
	jne	ABORT
	cmpq	$0,%rdi
	jz	SKIP_ARGS
	movq	%rdi,%rcx
	addq $0, %rsi
PARSE_ARGV:
	pushq %rcx
	pushq %rsi
	movq	(%rsi),%rdi
	movq	$0,%rsi
	movq	$10,%rdx
	call	strtol
	popq %rsi
	popq %rcx
	pushq %rax
	subq $8, %rsi
	loop PARSE_ARGV
SKIP_ARGS:
	call	_main
	jmp	END
ABORT:
	movq	$.errout, %rdi
	call puts
END:
	movq    %rax, %rdi
	call    exit
_main:
	pushq   %rbp
	movq    %rsp, %rbp
	movq	$.STR0, %rsi
	movq	$.strout, %rdi
	movq	$0, %rax
	call	printf
	movq	$0x0A, %rdi
	call	putchar
	movq	%rbp, %rsp
	movq	$0, %rax
	popq	%rbp
	ret
