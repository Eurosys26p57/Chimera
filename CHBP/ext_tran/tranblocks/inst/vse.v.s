; operand: VPREG0, REG0
; live REGS: VPREG0, REG0
; Temporary REGS: REG3, REG2, REG1
li REG3, NUM
ld REG1, VPREG0
mv REG2, REG0

loop:
  beqz REG3, end
  sw REG2, REG1
  addi REG2, REG2, 4
  addi REG1, REG1, 4
  addi REG3, REG3, -1         # 计数器减一
  j loop                    # 继续循环

end:
