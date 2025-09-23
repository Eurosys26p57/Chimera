        addi   sp, sp, -16
        sd     t0, 8(sp)
	lui    t0, 0x1
	add    t0, t0, t0
	auipc  t0, 0
	addi   t0, t0, 8
	jr     t0
        ld     t0, 8(sp)
        addi   sp, sp, 16
