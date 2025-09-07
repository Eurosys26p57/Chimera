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

# IMPINST = ["sh1add", "zext.w"]  # ["vle.v", "vse.v", "vfadd.vv"]
IMPINST = ["", ""]  # ["vle.v", "vse.v", "vfadd.vv"]
IMPINST_CONFIG = []  # ["vsetvli"]
A_REGS = [f'a{x}' for x in range(8)]
T_REGS = [f't{x}' for x in range(7)]
F_REGS = [f'f{x}' for x in range(12)]


# test_binary_name = "axpy_vector.exe"#"helloriscv.modify"
# translator generates target isa code, which needs the information of trampoline address and translated code blocks address to correct memory accessing
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
        self.jmptype = jmptype  # LONGJMP
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

    # TODO:what if a instruction accesses memory by sp+offset? we need to correct all instruction addressing by sp+offset
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
        # TODO:how to address the address of virtual vregister to temp register
        insttemplate = self.templatedict[inst.opcode]  # self.init_template(inst)
        insttemplate.update_oprand(inst.operand_extract)
        insttemplate.update_tempregs(self.get_tempregs(insttemplate.tempFREG_num, insttemplate.tempREG_num))
        return insttemplate.return_content()

    def remove_trampoline_reg(self):
        pass

    def choose_template_regs(self, cb):
        used_regs_list = []
        max_freg = 0
        max_reg = 0
        # TODO:handle argument registers
        for inst in cb.instructions:
            # TODO:config impelement
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
            # TODO:self.remove_trampoline_reg()
        if max_reg <= len(self.base_ISA_regs) and max_freg <= len(self.base_ISA_fregs):
            self.logue_regs = self.base_ISA_regs[0:max_reg] + self.base_ISA_fregs[0:max_freg]
            self.spoffset = len(self.logue_regs) * 4
            return
        # TODO: handle registers per instruction
        else:
            raise Exception("not enough registers!")

    def translate_ext_insts_stage1(self, cb):
        ext_inst_index = []
        self.choose_template_regs(cb)
        translated_cb_inst_list = []

        # 如果使用了gp寄存器，需要在开头增加恢复gp寄存器的指令
        if self.is_use_gp:
            print(self.is_use_gp)
            # 把值分解为两部分，高20位和低12位
            if self.is_use_gp.startswith('-'):
                high, low = self.is_use_gp[:-3], '-0x' + self.is_use_gp[-3:]
            else:
                high, low = self.is_use_gp[:-3], '0x' + self.is_use_gp[-3:]
            # high, low = int(high, 16), int(low, 16)
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
                translated_cb_inst_list.append(x)  # 不是扩展指令，直接加入，地址修正之后处理
        # 此时translated_cb_inst_list中既有str格式的反汇编指令，也有instruction对象
        end_list = self.add_eilogue().split("\n") + self.add_ret_trampoline().split("\n")
        translated_cb_inst_list.extend(end_list)
        # 需要将str格式转换为cb格式
        instructions = []
        fst_transed_inst, fst_transed_index = True, 0
        for i in translated_cb_inst_list[len(start_list):-len(end_list)]:
            if type(i) == str:  # 如果是str，说明是反汇编指令，需要将其转换为instruction对象
                if ' ' in i:
                    opcode, operand = i.split(" ", 1)
                    operand = operand.lstrip()
                else:
                    opcode = i
                    operand = ""
                instr = Instruction(opcode, operand)
                if fst_transed_inst:  # 遇到了第一个被翻译的指令
                    instr.cb_index = ext_inst_index[fst_transed_index]
                    fst_transed_inst = False
                    fst_transed_index += 1
                instructions.append(instr)
            else:  # 如果是instruction对象，直接加入
                instructions.append(i)
                fst_transed_inst = True  # 下一次遇到的翻译指令一定是第一个扩展指令被翻译的指令
        # 增加prologue和eilogue
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
        # return "\n".join(translated_cb_inst_list), CodeBlock(instructions) # 指令里面还有Instruction对象，无法返回str格式的了
        return CodeBlock(instructions)

    def translate_pcrelated_insts(self, cb0, cb1, offset: str):
        """
        给定code blocks ，偏移
        输出一个翻译后的code blocks，要求其中的地址相关指令做一个运算

        todo ：先找或者生成这样的一个用例，让它有足够的跳转且后面用到的时候比较频繁

        **函数接口：这里改了一下，func（原代码块cb0，现有的代码块cb1，
        一个字典用来标记cb1中增加了那些指令{cb0中指令的下标：cb1中指令的下标}注意一些指令因为被翻译不会出现在字典里，cb0和cb1初始位置的offset类型是十六进制的str）**

        一般来说，翻译指令会增加指令条数，比如一个向量加我把它拆成一堆标量相加，那这个向量指令下标就对应着n个标量的指令，先做这方面的。

        add = pc1 + imm1 = pc2 + imm2
        imm2 = imm1 + offset + delta_insts

        **cb0的地址属性可用，需要向cb1的属性中添加地址


        """

        def hex_add(*add_num):
            total = 0x0
            for num in add_num:
                if type(num) == str:
                    num = int(num, 16)
                total += num
            return hex(total)
        ext_inst_offset = [0x0]  # 因扩展指令翻译而产生的偏移,ext_inst_offset[i]表示cb1中第i条指令前有多少个因翻译产生的偏移
        auipc_inst_offset = [0x0]  # 因auipc相关的访存指令而产生的偏移,auipc_inst_offset[i]表示cb1中第i条指令前有多少个因访存产生的偏移
        # 处理auipc相关的访存指令
        instructions_copy = cb1.instructions.copy()
        j = 0
        for i, inst in enumerate(instructions_copy):
            # 如果指令为auipc，需要在其后添加一个addi指令
            if inst.opcode == "auipc" and not inst.is_logue:
                cb1.instructions.insert(i + j + 1, "addi")
                j += 1
                # imm = hex_add(-int(offset, 16), ext_inst_offset[inst.cb_index],
                #               auipc_inst_offset[inst.cb_index])
                # # 添加addi指令
                # # 更新auipc_inst_offset
                # auipc_inst_offset.append(auipc_inst_offset[-1] + 0x4)
                # cb1.insert(inst.cb_index + 1, Instruction("addi", imm))  # 需要实现
            # else:
            #     auipc_inst_offset.append(auipc_inst_offset[-1])
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
        # 实装addi指令
        for i, inst in enumerate(cb1.instructions):
            if type(inst) == str:
                imm = hex_add(-int(offset, 16), ext_inst_offset[i],
                              auipc_inst_offset[i])
                # logger.debug(f"{inst}, {ext_inst_offset[i]}, {auipc_inst_offset[i]}")
                # 将imm分为高20位和低12位，分别给auipc和addi使用
                imm_int = int(imm, 16)
                imm_flag = 1
                if imm_int < 0:
                    imm_int_abs = -imm_int
                    imm_flag = -1
                else:
                    imm_int_abs = imm_int
                imm_high20 = imm_int_abs >> 12
                imm_low12 = imm_int_abs & 0xfff
                # 拿到auipc的rd寄存器和立即数
                reg, imm_op = cb1.instructions[i - 1].operand_extract
                print(f"imm_high20:{imm_high20}, imm_op:{imm_op}, flag:{imm_flag}")
                imm_op = hex_to_signed_hex(imm_op)
                print(cb1.instructions[i-1].tostr(), imm_op)
                cb1.instructions[i - 1].operand_extract[-1] = hex_to_twos_complement(hex_add(imm_high20 * imm_flag, imm_op))
                cb1.instructions[i - 1].operand = ', '.join(cb1.instructions[i - 1].operand_extract)
                print(cb1.instructions[i-1].tostr())
                cb1.instructions[i] = Instruction("addi", f"{reg}, {reg}, {hex(imm_low12 * imm_flag)}")
                continue
        # 为cb1添加地址属性
        cb1.instructions[0].address = hex_add(cb0.instructions[0].address, offset)
        cb1.start_addr = hex_add(cb0.startaddr, offset)
        for i in range(1, len(cb1.instructions)):
            cb1.instructions[i].address = hex_add(cb1.instructions[i - 1].address, '0x4')

        # 处理跳转指令
        for inst in cb1.instructions:
            if inst.jump_to_instr != -1:
                # logger.debug(f"{inst.opcode}:{inst.jump_to_instr}")
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

        # jr_insts = {"j", "b", "auipc"}  # 跳转指令的起始
        # # 1 需要找到因为扩展而翻译的指令并且记录位置与指令条数
        # # 1.1 生成源代码块到目标代码块的映射关系
        # cb0_to_cb1_map = {hex_add(cb0.start_addr, offset): cb0.start_addr}
        # cb1.start_addr = hex_add(cb0.start_addr, offset)
        # i, j, flag = 0, 0, False  # flag表示是否还有未翻译的指令，初始值为False
        # while i < len(cb0.instructions) and j < len(cb1.instructions):
        #     # 若翻译前后opcode与oprand相同，则说明是相同的未翻译指令
        #     if cb0.instructions[i].opcode == cb1.instructions[j].opcode \
        #             and cb0.instructions[i].operand == cb1.instructions[j].operand:
        #         # 记录映射关系
        #         cb0_to_cb1_map[hex_add(cb0.instructions[i].address, offset)] = cb0.instructions[i].address
        #         i += 1
        #         j += 1
        #         flag = False  # 翻译后的指令都映射完毕了，此时flag置为False
        #     # 若不相同，说明是指令被翻译，此时需要将cb1中翻译后的指令映射到cb0中对应的指令上
        #     elif not flag:
        #         cb0_to_cb1_map[hex_add(cb0.instructions[i].address, offset)] = cb0.instructions[i].address
        #         i += 1
        #         j += 1
        #         flag = True  # 如果下一个指令还是不相等，说明翻译后的指令不止一条
        #     elif flag:  # 把翻译后的所有指令映射到翻译前的单个指令上，直到指令相等
        #         cb0_to_cb1_map[cb1.instructions[j].address] = cb0.instructions[i - 1].address
        #         j += 1

        # 1.2 填充目标代码块的相关信息，包括地址等
        # 2 需要找到跳转指令或auipc，一个cb至多只有一个跳转指令且在代码块末尾
        # 3 因为auipc添加的addi指令貌似仅影响一个跳转指令（运行完之后会跳回原地址执行之后的代码）

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
                    # TODO:shortjump还没实现
        if self.jmptype == Translator.SHORTJMP:
            return "\n".join(trampfilecontent)
        trampfilecontent = self.replace_jmpout_reg(trampfilecontent)
        return "\n".join(trampfilecontent)

    def translate_ext_insts(self, cb, offset):
        cb.init_cb_jump_to_instr()
        # logger.debug("init_cb_jump_to_instr completed! cb:{}".format(cb.startaddr))
        cb1 = self.translate_ext_insts_stage1(cb)
        cb = self.translate_pcrelated_insts(cb, cb1, offset)
        return cb.tostr()


if __name__ == "__main__":
    # b = Binary(test_objdump_path, test_binary_name, ["sh1add"])
    t = Translator({}, jmpout_reg="a0", is_use_gp="0x17ff00")
    # n = []
    # for c in b.find_source_ISAX():
    #     cb = t.translate_ext_insts(c)
    # s = ("li a0, 2\nauipc a1, 0x1\naddi a1, a1, 0x234\nsh1add a2, a0, a1\n"
    #      "lw a3, 0(a2)\nauipc a3, 0x20\nld a4, -0x28(a3)\njal 0xc")
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
