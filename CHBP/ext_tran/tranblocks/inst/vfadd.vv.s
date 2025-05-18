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
  flw FREG0, 0(REG0)            
  flw FREG1, 0(REG1)           
  fadd.s FREG2, FREG0, FREG1  

  fsw FREG2, 0(REG2)         

  addi REG0, REG0, 4        
  addi REG1, REG1, 4       
  addi REG2, REG2, 4      

  addi REG3, REG3, -1    
  j loop                

end:
