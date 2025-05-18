from Instr import disam_binary, get_text_offset, INDIRECT_JUMP_VERSION
from tools import hex_add
from CodeBlock import get_codeblocks_linear, can_be_merged, merge_blocks, split_codeblock
from utils import riscv_instructions_regs_rules, riscv_reg_names
import logging
logger = logging.getLogger(__name__)
BLOCK_INTERVAL = 20 
FIND_LIMIT = 300
find_count = 0
find_v_ext = True
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
    def trace_gp_sequence(self):
            s = s.lstrip('
            raw = int(s, 16)
            imm_20bit = raw & 0xFFFFF         
            if imm_20bit & 0x80000:           
                imm_20bit -= 0x100000         
            return imm_20bit
        def parse_common_imm(s):
        addr：起始地址
        n：dead寄存器数量
        rd_regs: 已经读过的寄存器，一般初始化为空集合set()
        return_instr_address: 是否返回有死寄存器的指令地址，默认为False，即不返回
        return：死寄存器名称列表，和对应指令的地址（如果需要返回）
