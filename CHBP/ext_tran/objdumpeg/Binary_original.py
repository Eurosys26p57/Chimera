from Instr import disam_binary, get_text_offset
from tools import hex_add
from CodeBlock import get_codeblocks_linear, can_be_merged, merge_blocks, split_codeblock
from utils import riscv_instructions_regs_rules
import logging
logger = logging.getLogger(__name__)
BLOCK_INTERVAL = 20 
FIND_LIMIT = 500
find_count = 0
class Binary:
    def __init__(self, objdump_path, binary_name, source_ISAX_list, merge=True):
        self.objdump_path = objdump_path
        self.binary_name = binary_name
        self.source_ISAX_list = source_ISAX_list
        self.instructions, self.erradd = disam_binary(self.binary_name, self.objdump_path)
        self.inst_dist = {inst.address: i for i, inst in enumerate(self.instructions)}  
        self.inst_block_map, self.code_blocks = get_codeblocks_linear(self.instructions)  
        self.text_offset, self.text_size = get_text_offset(self.binary_name) 
        self.merge = merge
    def find_source_ISAX_stage1(self):
        sc = []
        for p in range(len(self.code_blocks)):
            c = self.code_blocks[p]
            for i in c.instructions:
                if self.is_source_ISAX(i):
                    self.code_blocks[p].hasExtInstr = True
                    sc.append(c)
                    break
        return sc
    def print_binary(self):
        print(self.binary_name, self.objdump_path, self.source_ISAX_list)
        print(self.instructions[:5])
        print(self.code_blocks[:5])
    def find_dead_register(self, addr, n, rd_regs):
        global find_count
        dead_regs = set()
        for inst in self.instructions[self.inst_dist[addr]:]:
            logger.debug(inst.tostr())
            inst_rule = riscv_instructions_regs_rules[inst.opcode]
            if not inst_rule:
                continue
            for i in range(len(inst_rule)):
                if inst_rule[i] == 1:
                    op_str = inst.operand_extract[i]
                    idx = op_str.find('(')
                    if idx == -1:
                        rd_regs.add(op_str)
                    else:
                        idx_r = op_str.find(')')
                        register = op_str[idx + 1:idx_r]
                        if register != 'sp':
                            rd_regs.add(register)
            logger.debug(rd_regs)
            if not inst_rule[0]:
                wr_reg = inst.operand_extract[0]
                if wr_reg not in rd_regs:
                    dead_regs.add(wr_reg)
                    if len(dead_regs) >= n:
                        return True, dead_regs
            logger.debug(dead_regs)
            if find_count >= FIND_LIMIT:
                return False, dead_regs
            if inst.jumpto != []:
                jump_to_regs = []
                for addr_jumpto in inst.jumpto:
                    find_count += 1
                    if find_count >= FIND_LIMIT:
                        return False, dead_regs
                    jump_to_regs.append(self.find_dead_register(addr_jumpto, n - len(dead_regs), rd_regs)[1])
                else:
                    dead_regs.update(jump_to_regs[0].intersection(*jump_to_regs))
                if len(dead_regs) >= n:
                    return True, dead_regs
        return False, dead_regs
    def is_source_ISAX(self, instr):
        if instr.opcode in self.source_ISAX_list:
            return True
        return False
    def checkIndirectJump(self):
        for p in range(len(self.code_blocks)):
            c = self.code_blocks[p]
            for i in c.instructions:
                if i.opcode[0:4] == 'jalr' or i.opcode[0:2] == 'jr':
                    self.code_blocks[p].hasIndirectJump = True
                    break
    def Merge(self):
        merged_blocks = []
        i = 0
        n = len(self.code_blocks)
        while i < n:
            cb_ext = self.code_blocks[i]
            merge = False
            if not cb_ext.hasExtInstr:
                i += 1
                continue
            logger.debug("Merging code block:{}".format(cb_ext.startaddr))
            if not self.merge:
                merge = False
            else:
                for j in range(i+1, len(self.code_blocks)):
                    cb_b = self.code_blocks[j]
                    idx, merge = can_be_merged(cb_ext, cb_b, self.text_offset, self.code_blocks)
                    if merge:
                        mcb = merge_blocks(idx, j, self.code_blocks)
                        merged_blocks.append(mcb)
                        i = j
                        break
            if not merge:
                merged_blocks.append(cb_ext)
            i += 1
        return merged_blocks
    def extract_from_merged_cbs(self, merged_cbs):
        print("extract from merge cbs")
        from Instr import branch_instr
        extracted = []
        for c in merged_cbs:
            if c.merged:
                extracted = extracted + [c]
                continue
            divided_cbs = []
            cb1, cb2, cb3 = None, None, None
            n = len(c.instructions)
            for i in range(n):
                if self.is_source_ISAX(c.instructions[i]):
                    logger.debug("Ext instr: {}".format(c.instructions[i].address))
                    if i + 1 < n and c.instructions[i+1].instrlen == 4:
                        cb1, cb2 = split_codeblock(c, i)
                    else:
                        cb1, cb2 = split_codeblock(c, i-1)
                    if cb2 != None:
                        if cb2.instructions[-1].opcode[0] == 'j':
                            cb2_1, cb2_2 = split_codeblock(cb2, len(cb2.instructions)-1)
                            cb2 = cb2_1
                            cb3 = cb2_2
                            break
                        elif cb2.instructions[-1].opcode in branch_instr:
                            for addr in cb2.instructions[-1].jumpto:
                                if addr > cb2.endaddr or addr < cb2.startaddr:
                                    cb2_1, cb2_2 = split_codeblock(cb2, len(cb2.instructions)-1)
                                    cb2 = cb2_1
                                    cb3 = cb2_2
                                    break
                    break
                elif i == len(c.instructions) - 1:
                    if c.instructions[i].opcode[0] == 'j':
                        cb1, cb2 = split_codeblock(c, i)
                        break
                    elif c.instructions[i].opcode in branch_instr:
                        for addr in c.instructions[i].jumpto:
                            if addr > c.endaddr or addr < c.startaddr:
                                cb1, cb2 = split_codeblock(c, i)
                                break
            if cb1 != None:
                for instr in cb1.instructions:
                    if self.is_source_ISAX(instr):
                        cb1.hasExtInstr = True
                        divided_cbs.append(cb1)
                        break
            if cb2 != None:
                for instr in cb2.instructions:
                    if self.is_source_ISAX(instr):
                        cb2.hasExtInstr = True
                        divided_cbs.append(cb2)
                        break
            if cb3 != None:
                for instr in cb3.instructions:
                    if self.is_source_ISAX(instr):
                        cb3.hasExtInstr = True
                        divided_cbs.append(cb3)
                        break
            else:
                divided_cbs = [c]
            extracted = extracted + divided_cbs
        return extracted
    def find_source_ISAX(self):
        self.find_source_ISAX_stage1()
        self.checkIndirectJump()
        logger.info("Merging...")
        merged_blocks = self.Merge()
        extracted = self.extract_from_merged_cbs(merged_blocks)
        return extracted
def print_codeblocks(cbs):
    print("共有"+str(len(cbs))+"个代码块。")
    for c in cbs:
        print("CodeBlock: ", c.startaddr, "~", c.endaddr, \
            " instr_num: ", c.instnum, " jumpfrom: ", c.jumpfrom, \
            " jumpto: ", c.jumpto, " hasExtInstr: ", c.hasExtInstr, \
            " hasIndirectJump: ", c.hasIndirectJump)
if __name__ == "__main__":
    b = Binary(test_objdump_path, test_binary_name, ["sh1add"])
    extracted = b.find_source_ISAX()
    print("Merging...")
    print_codeblocks(extracted)
