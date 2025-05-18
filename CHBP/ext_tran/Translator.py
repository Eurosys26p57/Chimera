import sys
sys.path.append("objdumpeg")
from objdumpeg.CodeBlock import CodeBlock, build_cb_from_strings
from objdumpeg.Binary import Binary
from objdumpeg.Instr import Instruction, three_opr_branch_instr, two_opr_branch_instr
from objdumpeg.tools import hex_to_twos_complement, hex_to_signed_hex
from InstrTemplate import InstrTemplate
import os
import logging
logging.basicConfig(level=logging.DEBUG,
                    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)
IMPINST = ["", ""]  
IMPINST_CONFIG = []  
A_REGS = [f'a{x}' for x in range(8)]
T_REGS = [f't{x}' for x in range(7)]
F_REGS = [f'f{x}' for x in range(12)]
test_binary_name = "instrtest/riscv_zba_ext/sh1add_test"
class Translator:
    LONGJMP = 1
    SHORTJMP = 0
    def __init__(self, address_info, insnpath="tranblocks/inst", jmptype=LONGJMP, jmpout_reg="ra", is_use_gp=False):
        self.address_info = address_info
        self.insnpath = insnpath
        self.loguepath = "tranblocks"
        self.templatedict = {}
        self.base_ISA_regs = T_REGS + A_REGS
        self.base_ISA_fregs = F_REGS
        self.logue_regs = []
        self.spoffset = 0
        self.jmptype = jmptype  
        self.jmpout_reg = jmpout_reg
        self.is_use_gp = is_use_gp
    def init_addr(self, address_info, jmptype):
        self.trampoline_addr = address_info["trampoline_addr"]
        self.trampoline_addr_ret = address_info["trampoline_addr_ret"]
        self.translated_cb_addr = address_info["translated_cb_addr"]
        self.jmptype = jmptype
    def init_template(self, inst):
        module_file = inst.opcode + ".s"
        module_file = os.path.join(self.insnpath, module_file)
        i = InstrTemplate(module_file)
        i.read_info_from_str()
        i.get_tmpFREG_num()
        return i
    def add_prologue(self):
        prologuepath = os.path.join(self.loguepath, "prologue.s")
        plgfilecontent = []
        regoffset = self.spoffset
        with open(prologuepath, "r") as f:
            for l in f.readlines():
                if l[0] != ';':
                    plgfilecontent.append(l.strip())
        content = [plgfilecontent[0]]
        for reg in self.logue_regs:
            l = plgfilecontent[1]
            l = l.replace("REG", reg)
            l = l.replace("CONTEXTOFFSET", str(regoffset))
            regoffset -= 4
            if regoffset < 0:
                raise Exception("context reg num > sp offset!")
            content.append(l)
        content_str = "\n".join(content)
        content_str = content_str.replace("SPOFFSET", str(-self.spoffset))
        return content_str
    def add_eilogue(self):
        eiloguepath = os.path.join(self.loguepath, "eilogue.s")
        elgfilecontent = []
        regoffset = self.spoffset
        with open(eiloguepath, "r") as f:
            for l in f.readlines():
                if l[0] != ';':
                    elgfilecontent.append(l.strip())
        content = []
        for reg in self.logue_regs:
            l = elgfilecontent[0]
            l = l.replace("REG", reg)
            l = l.replace("CONTEXTOFFSET", str(regoffset))
            regoffset -= 4
            if regoffset < 0:
                raise Exception("context reg num > sp offset!")
            content.append(l)
        content.append(elgfilecontent[1])
        content_str = "\n".join(content)
        content_str = content_str.replace("SPOFFSET", str(self.spoffset))
        return content_str
    def correct_memory_accessing(self, inst):
        instcb = CodeBlock([inst])
        return instcb
    def get_tempregs(self, fnum, rnum):
        return self.base_ISA_fregs[0:fnum] + self.base_ISA_regs[0:rnum]
    def tran_to_vreg(self, inst):
        if inst.opcode in IMPINST_CONFIG:
            return "CONFIG_X"
        insttemplate = self.templatedict[inst.opcode]  
        insttemplate.update_oprand(inst.operand_extract)
        insttemplate.update_tempregs(self.get_tempregs(insttemplate.tempFREG_num, insttemplate.tempREG_num))
        return insttemplate.return_content()
    def remove_trampoline_reg(self):
        pass
    def choose_template_regs(self, cb):
        used_regs_list = []
        max_freg = 0
        max_reg = 0
        for inst in cb.instructions:
            if inst.opcode in IMPINST:
                insttemplate = self.init_template(inst)
                self.templatedict[inst.opcode] = insttemplate
                max_freg = max(max_freg, insttemplate.tempFREG_num)
                max_reg = max(max_freg, insttemplate.tempREG_num)
            for r in inst.operand_extract:
                if r in self.base_ISA_regs:
                    self.base_ISA_regs.remove(r)
                if r in self.base_ISA_fregs:
                    self.base_ISA_fregs.remove(r)
        if max_reg <= len(self.base_ISA_regs) and max_freg <= len(self.base_ISA_fregs):
            self.logue_regs = self.base_ISA_regs[0:max_reg] + self.base_ISA_fregs[0:max_freg]
            self.spoffset = len(self.logue_regs) * 4
            return
        else:
            raise Exception("not enough registers!")
    def translate_ext_insts_stage1(self, cb):
        ext_inst_index = []
        self.choose_template_regs(cb)
        translated_cb_inst_list = []
        if self.is_use_gp:
            print(self.is_use_gp)
            if self.is_use_gp.startswith('-'):
                high, low = self.is_use_gp[:-3], '-0x' + self.is_use_gp[-3:]
            else:
                high, low = self.is_use_gp[:-3], '0x' + self.is_use_gp[-3:]
            start_list = [f'lui gp, {high}', f'addi gp, gp, {low}']
            start_list.extend(self.add_prologue().split('\n'))
        else:
            start_list = self.add_prologue().split('\n')
        translated_cb_inst_list.extend(start_list)
        for x in cb.instructions:
            if x.opcode in IMPINST + IMPINST_CONFIG:
                ext_inst_index.append(x.cb_index)
                translated_cb_inst_list.extend(self.tran_to_vreg(x).split("\n"))
            else:
                translated_cb_inst_list.append(x)  
        end_list = self.add_eilogue().split("\n") + self.add_ret_trampoline().split("\n")
        translated_cb_inst_list.extend(end_list)
        instructions = []
        fst_transed_inst, fst_transed_index = True, 0
        for i in translated_cb_inst_list[len(start_list):-len(end_list)]:
            if type(i) == str:  
                if ' ' in i:
                    opcode, operand = i.split(" ", 1)
                    operand = operand.lstrip()
                else:
                    opcode = i
                    operand = ""
                instr = Instruction(opcode, operand)
                if fst_transed_inst:  
                    instr.cb_index = ext_inst_index[fst_transed_index]
                    fst_transed_inst = False
                    fst_transed_index += 1
                instructions.append(instr)
            else:  
                instructions.append(i)
                fst_transed_inst = True  
        for i in range(len(start_list)):
            opcode, operand = start_list[i].split(" ", 1)
            ins = Instruction(opcode, operand)
            ins.is_logue = True
            instructions.insert(i, ins)
        for i in range(len(end_list)):
            opcode, operand = end_list[i].split(" ", 1)
            ins = Instruction(opcode, operand)
            ins.is_logue = True
            instructions.append(ins)
        print('-' * 20)
        print(cb.tostr())
        return CodeBlock(instructions)
    def translate_pcrelated_insts(self, cb0, cb1, offset: str):
        def hex_add(*add_num):
            total = 0x0
            for num in add_num:
                if type(num) == str:
                    num = int(num, 16)
                total += num
            return hex(total)
        ext_inst_offset = [0x0]  
        auipc_inst_offset = [0x0]  
        instructions_copy = cb1.instructions.copy()
        j = 0
        for i, inst in enumerate(instructions_copy):
            if inst.opcode == "auipc" and not inst.is_logue:
                cb1.instructions.insert(i + j + 1, "addi")
                j += 1
        for inst in cb1.instructions:
            if type(inst) == str:
                auipc_inst_offset.append(auipc_inst_offset[-1] - 0x4)
            else:
                auipc_inst_offset.append(auipc_inst_offset[-1])
        for inst in cb1.instructions:
            if type(inst) != str and inst.cb_index == -1:
                ext_inst_offset.append(ext_inst_offset[-1] - 0x4)
            else:
                ext_inst_offset.append(ext_inst_offset[-1])
        for i, inst in enumerate(cb1.instructions):
            if type(inst) == str:
                imm = hex_add(-int(offset, 16), ext_inst_offset[i],
                              auipc_inst_offset[i])
                imm_int = int(imm, 16)
                imm_flag = 1
                if imm_int < 0:
                    imm_int_abs = -imm_int
                    imm_flag = -1
                else:
                    imm_int_abs = imm_int
                imm_high20 = imm_int_abs >> 12
                imm_low12 = imm_int_abs & 0xfff
                reg, imm_op = cb1.instructions[i - 1].operand_extract
                print(f"imm_high20:{imm_high20}, imm_op:{imm_op}, flag:{imm_flag}")
                imm_op = hex_to_signed_hex(imm_op)
                print(cb1.instructions[i-1].tostr(), imm_op)
                cb1.instructions[i - 1].operand_extract[-1] = hex_to_twos_complement(hex_add(imm_high20 * imm_flag, imm_op))
                cb1.instructions[i - 1].operand = ', '.join(cb1.instructions[i - 1].operand_extract)
                print(cb1.instructions[i-1].tostr())
                cb1.instructions[i] = Instruction("addi", f"{reg}, {reg}, {hex(imm_low12 * imm_flag)}")
                continue
        cb1.instructions[0].address = hex_add(cb0.instructions[0].address, offset)
        cb1.start_addr = hex_add(cb0.startaddr, offset)
        for i in range(1, len(cb1.instructions)):
            cb1.instructions[i].address = hex_add(cb1.instructions[i - 1].address, '0x4')
        for inst in cb1.instructions:
            if inst.jump_to_instr != -1:
                for inst_ in cb1.instructions:
                    if inst_.cb_index == inst.jump_to_instr:
                        to_addr = inst_.address
                        break
                else:
                    raise Exception("jump_to_instr is wrong")
                indirect_addr = hex_add(to_addr, -int(inst.address, 16))
                if inst.opcode[0] == "j":
                    inst.operand_extract[-1] = indirect_addr
                    inst.operand = indirect_addr
                elif inst.opcode[0] == "b":
                    inst.operand_extract[-1] = indirect_addr
                    inst.operand = ', '.join(inst.operand_extract)
        return cb1
    def replace_jmpout_reg(self, content):
        content_replaced = []
        for l in content:
            l = l.replace("ra", self.jmpout_reg)
            content_replaced.append(l)
        return content_replaced
    def add_ret_trampoline(self):
        if self.jmptype == Translator.LONGJMP:
            tramppath = os.path.join(self.loguepath, "longret.s")
        else:
            tramppath = os.path.join(self.loguepath, "shortret.s")
        trampfilecontent = []
        with open(tramppath, "r") as f:
            for l in f.readlines():
                if l[0] != ';':
                    trampfilecontent.append(l.strip())
        if self.jmptype == Translator.SHORTJMP:
            return "\n".join(trampfilecontent)
        trampfilecontent = self.replace_jmpout_reg(trampfilecontent)
        return "\n".join(trampfilecontent)
    def translate_ext_insts(self, cb, offset):
        cb.init_cb_jump_to_instr()
        cb1 = self.translate_ext_insts_stage1(cb)
        cb = self.translate_pcrelated_insts(cb, cb1, offset)
        return cb.tostr()
if __name__ == "__main__":
    t = Translator({}, jmpout_reg="a0", is_use_gp="0x17ff00")
    s = ("zext.w a5, a3\n"
         "slli a4, a5, 0x2\n"
         "lui a5, 0x2ff\n"
         "addi a5, a5, -0x48c\n"
         "add a5, a5, a4\n"
         "lw a5, 0x0(a5)\n"
         "jr a5\n"
         "sw zero, -0x60(s0)\n"
         "ld a5, -0x248(s0)\n"
         "ld a5, 0x8(a5)\n"
         "mv a0, a5\n"
         "jal 0x12")
    print(s)
    cb0 = build_cb_from_strings(s)
    cb0.init_cb_jump_to_instr()
    cb1 = t.translate_ext_insts_stage1(cb0)
    cb = t.translate_pcrelated_insts(cb0, cb1, "0x1000")
    print(cb.tostr())
    print(t.logue_regs)
