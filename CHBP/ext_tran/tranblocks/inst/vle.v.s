/**
 * register name:
 *  REGX: Noraml register X(number)
 *  FREGX: Float register X
 *  VPREG: Register. VPREG is a register(initted in prologue) saving the pointer pointing to the data stored in Virtual Vector Register X
 **/
; operand: VPREG0, REG0
; live REGS: VPREG0, REG0
; Temporary REGS: None
sd REG0, VPREG0 ;vle, expample: vle v0, a0 translator to sw a0, VPREG0, the address VPREG0 will store the address of the data loading to v1
