from Instr import Instruction, disam_binary, jmp_branch_instr, INDIRECT_JUMP_VERSION
from tools import hex_add
import logging
logging.basicConfig(level=logging.DEBUG,
                    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)
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
        self.hasExtInstr = False  
        self.hasIndirectJump = False  
        self.merged = False  
        self.original_startaddress = self.startaddr 
        self.trampoline_type = 0 
        if self.instructions[-1].isret:
            self.retblock = True
        self.use_gp_jump_in = False
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
                    if other_instr.address == to_addr:
                        logger.debug(f"jump to cb_index:{other_instr.cb_index}")
                        instr.jump_to_instr = other_instr.cb_index
                        break
                else:
                    logger.error("no jump target found")
def get_codeblocks_linear(instructions):
    i, j, k = 0, 0, 0  
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
def no_inner_jump(cb_a, cb_b, cbs, check_jump_into=True, check_jump_outof=True):
    no_jump_into, no_jump_outof = True, True
    start_idx, end_idx = cb_a.index, cb_b.index
    if end_idx - start_idx + 1 > MAX_BLOCK_NUM:
        return False
    start_addr, end_addr = int(cb_a.startaddr, 16), int(cb_b.endaddr, 16)
    if check_jump_into:
        for i in range(start_idx + 1, end_idx + 1):
            tmp_cb = cbs[i]
            for jumpfrom in tmp_cb.jumpfrom:
                if not start_addr <= int(jumpfrom, 16) <= end_addr:
                    no_jump_into = False
                    break
            if not no_jump_into:
                break
    if check_jump_outof:
        for i in range(start_idx, end_idx):
            tmp_cb = cbs[i]
            for jumpto in tmp_cb.jumpto:
                if not start_addr <= int(jumpto, 16) <= end_addr:
                    no_jump_outof = False
                    break
            if not no_jump_outof:
                break
    return no_jump_into and no_jump_outof
def find_target_codeblock(addr, cbs):
    for cb in cbs:
        if int(cb.startaddr, 16) <= int(addr, 16) <= int(cb.endaddr, 16):
            return cb
def no_consecutive_16bit_instruction(target_cb, cbs):
    if target_cb.instnum >= 2:
        return target_cb.instructions[0].instrlen != 1 and target_cb.instructions[1].instrlen != 1
    else:
        return target_cb.instructions[0].instrlen != 1 and cbs[target_cb.index + 1].instructions[0].instrlen != 1
def can_be_merged(cb_a, cb_b, text_offset, cbs):
    cb_a_addr = int(cb_a.startaddr, 16)
    for jumpto in cb_b.jumpto:
        if int(jumpto, 16) <= cb_a_addr and int(jumpto, 16) >= text_offset:
            target_cb = find_target_codeblock(jumpto, cbs)
            if not no_consecutive_16bit_instruction(target_cb, cbs):
                continue
            if no_inner_jump(target_cb, cb_b, cbs):
                logger.debug(cb_b.startaddr, " ~ ", cb_b.endaddr, " 的块跳跃地址是：", cb_b.jumpto, " 可以插在 ",
                             cb_a.startaddr, " ~ ", cb_a.endaddr, " 的块前面。")
                return target_cb.index, True
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
def split_codeblock(cb, idx):
    print("idx:", idx, ' ', len(cb.instructions))
    if idx <= 0 or idx == len(cb.instructions):
        cb1 = cb
        cb2 = None
        return cb1, cb2
    cb1 = CodeBlock(cb.instructions[:idx], cb.index)
    cb2 = CodeBlock(cb.instructions[idx:], cb.index)
    cb1.endaddr = cb1.instructions[-1].address
    cb2.startaddr = cb2.instructions[0].address
    cb1.addrange = [int(cb1.startaddr, 16), int(cb1.endaddr, 16)]
    cb2.addrange = [int(cb2.startaddr, 16), int(cb2.endaddr, 16)]
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
    cb1.instnum = len(cb1.instructions)
    cb2.instnum = len(cb2.instructions)
    cb1.retblock = True if cb1.instructions[-1].isret else False
    cb2.retblock = True if cb2.instructions[-1].isret else False
    cb1.original_startaddress = cb.original_startaddress
    cb2.original_startaddress = cb.original_startaddress
    cb1.trampoline_type = cb.trampoline_type
    cb2.trampoline_type = cb.trampoline_type
    return cb1, cb2
