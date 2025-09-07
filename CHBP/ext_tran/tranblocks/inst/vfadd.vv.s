; operand: VPREG2,VPREG1,VPREG0
; live REGS: VPREG2,VPREG1,VPREG0
; Temporary REGS: REG0, REG1, REG2, FREG0, FREG1, FREG2
ld REG0, VPREG0
ld REG1, VPREG1
ld REG2, VPREG2

;li REG3, NUM
CONFIG REG3

loop:
  beqz REG3, end
  flw FREG0, 0(REG0)            # 加载 array1[i] 到 f0
  flw FREG1, 0(REG1)            # 加载 array2[i] 到 f1
  fadd.s FREG2, FREG0, FREG1         # f2 = f0 + f1

  fsw FREG2, 0(REG2)            # 把结果存储到 result[i]

  addi REG0, REG0, 4          # 移动到 array1 的下一个元素
  addi REG1, REG1, 4          # 移动到 array2 的下一个元素
  addi REG2, REG2, 4          # 移动到 result 的下一个位置

  addi REG3, REG3, -1         # 计数器减一
  j loop                    # 继续循环

end:
