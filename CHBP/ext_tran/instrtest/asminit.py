import sys
sys.path.append("..")
sys.path.append("../objdumpeg")
import Translator
import Instr

def generate_asm(opcode, oprands, tempregs):
    target_instruction = Instr.Instruction(opcode.strip().lstrip(), oprands.strip().lstrip())
    t = Translator.Translator({}, "../tranblocks/inst")
#    t.init_template(target_instruction)
    insttemplate = t.init_template(target_instruction)
    insttemplate.update_oprand(target_instruction.operand_extract)
    insttemplate.update_tempregs(tempregs)
    return insttemplate.return_content()




def generate_output(regs):
    pass

if __name__ == "__main__":
    print(generate_asm(" add.uw ", " %0,%0,%1", ["%2"]))
    print(generate_asm(" sh1add ", " %0,%1,%2", ["%3"]))
    print(generate_asm(" sh1add.uw ", " %0,%1,%2", ["%3"]))
    print(generate_asm(" sh2add ", " %0,%1,%2", ["%3"]))
    print(generate_asm(" sh2add.uw ", " %0,%1,%2", ["%3"]))
    print(generate_asm(" sh3add ", " %0,%1,%2", ["%3"]))
    print(generate_asm(" sh3add.uw ", " %0,%1,%2", ["%3"]))
    print(generate_asm(" slli.uw ", " %0,%1,%2", ["%3"]))

