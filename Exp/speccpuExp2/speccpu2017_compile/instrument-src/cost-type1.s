        addi   sp, sp, -16
        sd     t0, 8(sp)
	auipc  t0, 0
	addi   t0, t0, 8
	jr     t0
        ld     t0, 8(sp)
        addi   sp, sp, 16
