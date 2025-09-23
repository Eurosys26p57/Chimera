        addi   sp, sp, -48
        sd     a0, 8(sp)
        sd     a7, 16(sp)
        csrr   a0, vtype
        csrr   a7, vl
        sd     a0, 24(sp)
        sd     a7, 32(sp)
        li     a7, 999
        ecall
        ld     a7, 32(sp)
        ld     a0, 24(sp)
        vsetvl zero, a7, a0
        ld     a0, 8(sp)
        ld     a7, 16(sp)
        addi   sp, sp, 48
