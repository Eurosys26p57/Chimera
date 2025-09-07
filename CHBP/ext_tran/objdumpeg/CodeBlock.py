from Instr import Instruction, disam_binary, jmp_branch_instr, INDIRECT_JUMP_VERSION
from tools import hex_add

import logging
logging.basicConfig(level=logging.DEBUG,
                    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# 一个合并块的最大基本块数，如果合并的块数超过此数值，就不再合并
MAX_BLOCK_NUM = 10

class CodeBlock:
    def __init__(self, instructions, index=0):
        self.index = index
        self.instructions = instructions
        self.startaddr = instructions[0].address
        self.endaddr = instructions[-1].address
        self.addrange = [int(self.startaddr, 16), int(self.endaddr, 16)]
        self.jumpto = instructions[-1].jumpto
        self.jumpfrom = instructions[0].jumpfrom
        self.instnum = len(self.instructions)
        self.retblock = False
        self.hasExtInstr = False  # 该基本块内是否有待翻译的拓展指令
        self.hasIndirectJump = False  # 该基本块内是否有间接跳转指令
        self.merged = False  # 该代码块是否被合并过
        self.original_startaddress = self.startaddr # 如果这是个被裁剪过的块，该值负责记录这个块本来所属的代码块的开始地址
        self.trampoline_type = 0 # 0:该块没有跳板。1：该块是4+4型跳板。2：该块是4+2类型跳板
        if self.instructions[-1].isret:
            self.retblock = True
        self.use_gp_jump_in = False
        #用这个来表示这个代码块是否通过跨分支扩展的方式来选择dead寄存器
        self.if_use_branch_deadreg = False
        self.if_use_jr_deadreg = False
        self.is_loop = False

    def update_instructions(self, instructions):
        self.instructions = instructions
        self.startaddr = instructions[0].address
        self.endaddr = instructions[-1].address
        self.addrange = [int(self.startaddr, 16), int(self.endaddr, 16)]
        self.jumpto = instructions[-1].jumpto
        self.jumpfrom = instructions[0].jumpfrom
        self.instnum = len(self.instructions)
        #original_startaddress不变的原因是第二轮是相同的流程
        self.is_loop = self.has_loop()
        if self.instructions[-1].isret:
            self.retblock = True


    def __repr__(self):
        return (
            f"CodeBlock(startaddr='{self.startaddr}', endaddr='{self.endaddr}',jumpto='{self.jumpto}', instr_num='{self.instnum}'")

    def tostr(self):
        return "\n".join([f"{x.tostr()}" if type(x) != str else x for x in self.instructions])

    def __eq__(self, value):
        if isinstance(value, CodeBlock):
            return self.startaddr == value.startaddr and self.endaddr == value.endaddr

    def init_cb_jump_to_instr(self):
        for instr in self.instructions:
            if instr.jumpto:
                logger.debug(f"******jump******")
                to_addr = instr.jumpto[-1]
                for other_instr in self.instructions:

                    # logger.debug(f"to_addr: {to_addr} and other_instr: {other_instr.address}")
                    if other_instr.address == to_addr:
                        logger.debug(f"jump to cb_index:{other_instr.cb_index}")
                        instr.jump_to_instr = other_instr.cb_index
                        break
                else:
                    logger.error("no jump target found")
                    #raise Exception("jump target is not in the codeblock.")

    def has_loop(self):
        int_start = int(self.startaddr, 16)
        int_end = int(self.endaddr, 16)
        for inst in self.instructions:
            if len(inst.jumpto) == 0:
                continue
            addr = inst.jumpto[0]
            int_addr = int(addr, 16)
            if int_addr >= int_start and int_addr <= int_end:
                self.is_loop = True
                return True
        self.is_loop = False
        return False




    
    #在判断是否可以使用branch作为dead寄存器后，会给codeblocks中增加新的指令inst_list
    #inst_list已经被整合成不同的代码块，件Binary.py中的iter_binary
    #在添加后需要对指令块中的指令序列以及直接跳转进行修正
    #在这个函数调用前有两点保证：
    #1.添加的指令序列在当前instructions的后面
    #2.instructions中的所有直接跳转不会跳转到指令块的外面
    #使用这个函数后需要有一点保证：
    #1.保证所有在extratext中的代码都没有使用c扩展编译（都是4B）
    #在这个函数里面顺带处理了每个分支跳转的dead reg?

    def update_insts_4_branch_deadreg(self, inst_list):
        inst_blocks_list = split_insts(inst_list)
        pass
        






# stage1 extract blocks linearly
def get_codeblocks_linear(instructions):
    # instructions = list(instructions.values())
    #    instructions = [Instruction(str(x), "fd", "dss", "fds") for x in range(10)]
    #    instructions[1].isblockbegin = True
    #    instructions[5].isblockbegin = True
    #    instructions[8].isblockbegin = True
    #    instructions[4].isblockend = True
    #    instructions[2].isblockend = True
    i, j, k = 0, 0, 0  # k用于记录每个cb内的index
    idx = 0
    blocks = []
    inst_block_map = {}
    while j < len(instructions):
        if instructions[j].isblockend:
            blocks.append(instructions[i:j + 1])
            instructions[j].cb_index = k
            k = 0
            i = j + 1
            j += 1
        elif instructions[j].isblockbegin and i != j:
            k = 0
            blocks.append(instructions[i:j])
            i = j
        else:
            instructions[j].cb_index = k
            j += 1
            k += 1
    if i != j:
        blocks.append(instructions[i:j])
    codeblocks = []
    for ins in blocks:
        codeblocks.append(CodeBlock(ins, idx))
        idx += 1
    base = 0
    #key:指令的索引，value：codeblock的
    for a in range(len(blocks)):
        for b in range(len(blocks[a])):
            inst_block_map[b + base] = a
        base += len(blocks[a])

    return inst_block_map, codeblocks


def build_cb_from_strings(strings):
    instructions_str = strings.split('\n')
    instructions = []
    for i, x in enumerate(instructions_str):
        if ' ' not in x:
            instr = Instruction(x, '')
        else:
            opcode, operands = x.split(' ', 1)
            instr = Instruction(opcode, operands)
            if opcode in jmp_branch_instr:
                instr.jumpto.append(operands.strip().split(',')[-1].strip())
                logger.debug(f"{opcode} {operands} ``````1111111{instr.jumpto}")
        instr.cb_index = i
        instr.address = hex(4*i)
        instructions.append(instr)
    return CodeBlock(instructions)


# if two blocks connected and first block may jumpto 
# def can_be_merged(cb_a, cb_b):
#     if cb_b.range[0] - cb_a.range[1] > 4:
#         return False
#     acount = 0
#     bcount = 0

#     for x in cb_a.jumpto:
#         x_int = int(x, 16)
#         if x_int >= cb_b.range[0] or x_int <= cb_b.range[1]:
#             return True

#     for x in cb_b.jumpto:
#         x_int = int(x, 16)
#         if x_int >= cb_a.range[0] or x_int <= cb_a.range[1]:
#             return True

# 判断是否没有外部跳转指令进入两个块及中间所有块的内部，以及是否没有内部的跳转指令跳出[cb_a, cb_b]外面(除了第一块的第一条指令可以被外部指令跳进去，和最后一块的最后一条指令可以跳出)，如果没有返回True，有返回False

### ATTENTION!!! ATTENTION!!! ATTENTION!!!
### 重要的事情说三遍！！！
### 注意此处的check_jump_into和check_jump_outof参数。它们默认为True，当关闭check_jump_into时，那就不管跳到里面的指令；当关闭check_jump_outof时，那就不管跳到外面的指令。

# 注意此处默认cb_a的地址比cb_b的地址小
def no_inner_jump(cb_a, cb_b, cbs, check_jump_into=True, check_jump_outof=True):
    no_jump_into, no_jump_outof = True, True
    start_idx, end_idx = cb_a.index, cb_b.index
    # 判断待合并基本块数量的操作也在这里进行
    if end_idx - start_idx + 1 > MAX_BLOCK_NUM:
        return False
    start_addr, end_addr = int(cb_a.startaddr, 16), int(cb_b.endaddr, 16)
    if check_jump_into:
        for i in range(start_idx + 1, end_idx + 1):
            tmp_cb = cbs[i]
            # 除了第一个块，不能有外部指令跳到中间块的内部
            for jumpfrom in tmp_cb.jumpfrom:
                if not start_addr <= int(jumpfrom, 16) <= end_addr:
                    no_jump_into = False
                    break
            if not no_jump_into:
                break
    if check_jump_outof:
        for i in range(start_idx, end_idx):
            tmp_cb = cbs[i]
            # 除了最后一个块，不能有中间块的指令跳到外部
            for jumpto in tmp_cb.jumpto:
                if not start_addr <= int(jumpto, 16) <= end_addr:
                    no_jump_outof = False
                    break
            if not no_jump_outof:
                break
    return no_jump_into and no_jump_outof


# 根据地址找到其所属的基本块
def find_target_codeblock(addr, cbs):
    for cb in cbs:
        if int(cb.startaddr, 16) <= int(addr, 16) <= int(cb.endaddr, 16):
            return cb


# 合并原则2：cb_b跳的地址所在的前2条指令不可以是两个连续的16bit指令
def no_consecutive_16bit_instruction(target_cb, cbs):
    if target_cb.instnum >= 2:
        return target_cb.instructions[0].instrlen != 1 and target_cb.instructions[1].instrlen != 1
    else:
        return target_cb.instructions[0].instrlen != 1 and cbs[target_cb.index + 1].instructions[0].instrlen != 1


def can_be_merged(cb_a, cb_b, text_offset, cbs):
    # 合并原则：cb_b可以跳到cb_a之前（但不能跳到.text段的前面），且从该cb_b跳的位置所属的基本块到cb_b之间，只有一个出口和一个入口
    cb_a_addr = int(cb_a.startaddr, 16)
    for jumpto in cb_b.jumpto:
        if int(jumpto, 16) <= cb_a_addr and int(jumpto, 16) >= text_offset:
            target_cb = find_target_codeblock(jumpto, cbs)
            # 合并原则2：cb_b跳的地址所在的前2条指令不可以是两个连续的16bit指令
            if not no_consecutive_16bit_instruction(target_cb, cbs):
                continue
            if no_inner_jump(target_cb, cb_b, cbs):
                logger.debug(cb_b.startaddr, " ~ ", cb_b.endaddr, " 的块跳跃地址是：", cb_b.jumpto, " 可以插在 ",
                             cb_a.startaddr, " ~ ", cb_a.endaddr, " 的块前面。")
                return target_cb.index, True
    # print(cb_b.startaddr, " ~ ", cb_b.endaddr, " 的块跳跃地址是：", cb_b.jumpto, " 不能插在 ", cb_a.startaddr, " ~ ", cb_a.endaddr, " 的块前面。")
    return cb_a.index, False


def merge_blocks(start_idx, end_idx, cbs):
    new_cb = None
    for i in range(start_idx, end_idx + 1):
        cb = cbs[i]
        if new_cb == None:
            new_cb = cb
        else:
            new_cb = merge_two_blocks(new_cb, cb)
    return new_cb


# 合并两个块（合并块或基本块）。注意此处默认cb_x的地址比cb_y的地址小
def merge_two_blocks(cb_x, cb_y):
    if cb_x.endaddr < cb_y.startaddr:
        cb_a, cb_b = cb_x, cb_y
    elif cb_y.endaddr < cb_x.startaddr:
        cb_a, cb_b = cb_y, cb_x
    instructions = cb_a.instructions + cb_b.instructions
    start = cb_a.startaddr
    end = cb_b.endaddr
    newjumpto = []
    newjumpfrom = []
    for x in cb_a.jumpto + cb_b.jumpto:
        x_int = int(x, 16)
        if x_int < int(start, 16) or x_int > int(end, 16):
            newjumpto.append(x)
    if (not len(newjumpto)) and (not new_cb.retblock):
        raise Exception("control flow circle")
    for x in cb_a.jumpfrom + cb_b.jumpfrom:
        x_int = int(x, 16)
        if x_int < int(start, 16) or x_int > int(end, 16):
            newjumpfrom.append(x)
    new_cb = CodeBlock(instructions, cb_y.index)
    new_cb.startaddr = start
    new_cb.endaddr = end
    new_cb.addrange = [int(start, 16), int(end, 16)]
    new_cb.jumpto = newjumpto
    new_cb.jumpfrom = newjumpfrom
    new_cb.instnum = len(instructions)
    new_cb.hasExtInstr = cb_a.hasExtInstr or cb_b.hasExtInstr
    new_cb.retblock = cb_a.retblock or cb_b.retblock
    new_cb.merged = True
    new_cb.original_startaddress = cb_a.original_startaddress
    new_cb.trampoline_type = cb_a.trampoline_type
    return new_cb


# 将一个代码块在第idx个指令（从0开始）的位置拆成两块，即：[0, idx-1]和[idx, n]两部分。
# 这里的难点主要在于jumpto和jumpfrom的处理。
# 如果分割完，有一个块的最后一条指令是jmp那就不能分割。
# 如果分割完使处理更加困难，即不满足基本块的要求，就不分割。
def split_codeblock(cb, idx):
    print("idx:", idx, ' ', len(cb.instructions))
    if idx <= 0 or idx == len(cb.instructions):
        cb1 = cb
        cb2 = None
        return cb1, cb2
    # index就先不管了，两个都设置成原来的index
    cb1 = CodeBlock(cb.instructions[:idx], cb.index)
    cb2 = CodeBlock(cb.instructions[idx:], cb.index)
    # 修改起始和结束地址
    cb1.endaddr = cb1.instructions[-1].address
    cb2.startaddr = cb2.instructions[0].address
    # 修改代码块地址范围
    cb1.addrange = [int(cb1.startaddr, 16), int(cb1.endaddr, 16)]
    cb2.addrange = [int(cb2.startaddr, 16), int(cb2.endaddr, 16)]
    # 修改jumpto和jumpfrom
    # 怎么改？暂时设计的是代码块里所有指令能跳的块外地址
    for instr in cb1.instructions:
        for addr in instr.jumpto:
            if (int(addr, 16) > int(cb1.endaddr, 16) or int(addr, 16) < int(cb1.startaddr,
                                                                            16)) and addr not in cb1.jumpto:
                cb1.jumpto.append(addr)
        for addr in instr.jumpfrom:
            if (int(addr, 16) > int(cb1.endaddr, 16) or int(addr, 16) < int(cb1.startaddr,
                                                                            16)) and addr not in cb1.jumpfrom:
                cb1.jumpfrom.append(addr)
    for instr in cb2.instructions:
        for addr in instr.jumpto:
            if (int(addr, 16) > int(cb2.endaddr, 16) or int(addr, 16) < int(cb2.startaddr,
                                                                            16)) and addr not in cb2.jumpto:
                cb2.jumpto.append(addr)
        for addr in instr.jumpfrom:
            if (int(addr, 16) > int(cb2.endaddr, 16) or int(addr, 16) < int(cb2.startaddr,
                                                                            16)) and addr not in cb2.jumpfrom:
                cb2.jumpfrom.append(addr)
    # 修改instnum
    cb1.instnum = len(cb1.instructions)
    cb2.instnum = len(cb2.instructions)
    # 修改retblock
    cb1.retblock = True if cb1.instructions[-1].isret else False
    cb2.retblock = True if cb2.instructions[-1].isret else False
    # 修改original_startaddress
    cb1.original_startaddress = cb.original_startaddress
    cb2.original_startaddress = cb.original_startaddress
    # 修改trampoline_type
    cb1.trampoline_type = cb.trampoline_type
    cb2.trampoline_type = cb.trampoline_type
    # 修改hasExtInstr的操作挪到Binary里了
    return cb1, cb2


if __name__ == "__main__":
    inst, _ = disam_binary(test_binary_name, test_objdump_path)
    bm, cbs = get_codeblocks_linear(inst)
    for i in range(len(cbs)):
        cbs[i].init_cb_jump_to_instr()
        for ins in cbs[i].instructions:
            if ins.jump_to_instr != -1:
                print(ins.jump_to_instr)
        print(cbs[i])
        print('--------------------------------')
    for ins in cbs[-3].instructions:
        print(ins)
    for ins in inst:
        print(ins)
    # get_codeblocks_stage1(inst)
    # s = ("li t0, 2\nauipc t1, 0x1addi t1, t1, 0x234\nsh1add t2, t0, t1, 2\n"
    #      "lw t3, 0(t2)\nauipc t3, 0x20\nld t4, -0x28(t3)")
    # cb = build_cb_from_strings(s)
