        addi   sp, sp, -32
        sd     a0, 8(sp)
        sd     a7, 16(sp)
        li     a7, 998
        ecall
        ld     a0, 8(sp)
        ld     a7, 16(sp)
        addi   sp, sp, 32
