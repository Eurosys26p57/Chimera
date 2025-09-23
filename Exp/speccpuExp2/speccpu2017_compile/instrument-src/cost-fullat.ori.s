	.file	"cost-fullat.c"
	.option pic
	.attribute arch, "rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_zicsr2p0_zifencei2p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
	.align	1
	.globl	_start
	.type	_start, @function
_start:
.LFB15:
	.cfi_startproc
	li	a5,-1640529920
	addi	a5,a5,-1607
	li	a4,2135588864
	addi	a4,a4,-939
	slli	a5,a5,32
	add	a5,a5,a4
	srli	a4,a0,12
	mul	a4,a4,a5
	ld	a5,.LANCHOR0
	li	a7,64
	srli	a4,a4,60
	slli	a4,a4,4
	add	a5,a5,a4
	lbu	a4,9(a5)
	lbu	a3,8(a5)
	lbu	a6,10(a5)
	lbu	a1,11(a5)
	lbu	a2,12(a5)
	slli	a4,a4,8
	or	a4,a4,a3
	slli	a6,a6,16
	lbu	a3,13(a5)
	or	a6,a6,a4
	slli	a1,a1,24
	lbu	a4,14(a5)
	or	a1,a1,a6
	lbu	a5,15(a5)
	slli	a2,a2,32
	or	a2,a2,a1
	slli	a3,a3,40
	or	a3,a3,a2
	slli	a4,a4,48
	or	a4,a4,a3
	slli	a5,a5,56
	or	a5,a5,a4
	ld	a2,8(a5)
	ld	a4,72(a5)
	ld	a3,56(a5)
	sub	a0,a0,a2
	mul	a0,a0,a4
	ld	a4,64(a5)
	subw	a7,a7,a3
	ld	a2,48(a5)
	addi	a4,a4,-1
	ld	a3,32(a5)
	srl	a0,a0,a7
	and	a5,a4,a0
	slli	a4,a5,32
	srli	a5,a4,29
	add	a5,a5,a2
	ld	a4,0(a5)
	slli	a5,a4,2
	add	a5,a5,a4
	slli	a5,a5,3
	add	a5,a5,a3
	lbu	a3,9(a5)
	lbu	a2,8(a5)
	lbu	a4,10(a5)
	lbu	a0,11(a5)
	slli	a3,a3,8
	or	a3,a3,a2
	slli	a5,a4,16
	or	a5,a5,a3
	slli	a0,a0,24
	or	a0,a0,a5
	sext.w	a0,a0
	ret
	.cfi_endproc
.LFE15:
	.size	_start, .-_start
	.globl	gtt_hash
	.globl	gtt
	.bss
	.align	3
	.set	.LANCHOR0,. + 0
	.type	gtt_hash, @object
	.size	gtt_hash, 8
gtt_hash:
	.zero	8
	.type	gtt, @object
	.size	gtt, 8
gtt:
	.zero	8
	.ident	"GCC: (Debian 14.2.0-11revyos1) 14.2.0"
	.section	.note.GNU-stack,"",@progbits
