from Instr import disam_binary, get_text_offset, INDIRECT_JUMP_VERSION
from tools import hex_add
from CodeBlock import get_codeblocks_linear, can_be_merged, merge_blocks, split_codeblock
from utils import riscv_instructions_regs_rules, riscv_reg_names
import logging
logger = logging.getLogger(__name__)
#currently not merge blocks because "ret" instr would make the range of function unclear
BLOCK_INTERVAL = 20 #blocks can be merged if the interval is less than BLOCK_INTERVAL
FIND_LIMIT = 300
find_count = 0
find_v_ext = True


class Binary:
    def __init__(self, objdump_path, binary_name, source_ISAX_list, merge=True):
        """
        初始化函数。

        Args:
            objdump_path (str): objdump工具的路径。
            binary_name (str): 二进制文件的名称。
            source_ISAX_list (list): 源ISA-X列表。
            merge: 是否合并块

        Attributes:
            objdump_path (str): objdump工具的路径。
            binary_name (str): 二进制文件的名称。
            source_ISAX_list (list): 源ISA-X列表。
            instructions (list): 反汇编得到的指令列表。
            erradd (list): 反汇编过程中出现的错误地址列表。
            code_blocks (list): 代码块列表，通过线性扫描得到。
        """
        self.objdump_path = objdump_path
        self.binary_name = binary_name
        #opcode need to be translated
        self.source_ISAX_list = source_ISAX_list
        logger.debug("objdump!")
        self.instructions, self.erradd = disam_binary(self.binary_name, self.objdump_path)
        logger.debug(self.instructions)
        self.inst_dist = {inst.address: i for i, inst in enumerate(self.instructions)}  # 存储指令地址和指令下标的映射关系
        #print(f"{self.instructions[self.inst_dist['0x1b48c0']]}")
        self.inst_block_map, self.code_blocks = get_codeblocks_linear(self.instructions)  # 添加了指令与代码块之间的映射，原用于避免死循环，但还是避免不了，此处无实际用处，但可视后续优化应用，遂保留
        self.text_offset, self.text_size = get_text_offset(self.binary_name) # 获取.text段的偏移和大小
        self.merge = merge

    def find_source_ISAX_stage1(self):
        sc = []
        for p in range(len(self.code_blocks)):
            c = self.code_blocks[p]
            for i in c.instructions:
                if self.is_source_ISAX(i):
                    self.code_blocks[p].hasExtInstr = True
                    print(i)
                    sc.append(c)
                    # 跳出当前指令的循环，因为已经找到了符合条件的代码块，无需继续检查其他指令
                    break
        # 返回包含以'v'开头的操作码的指令所在的代码块列表
        return sc

    def handle_loops(self):
        for cb in self.code_blocks:
            cb.has_loop()


    def print_binary(self):
        print(self.binary_name, self.objdump_path, self.source_ISAX_list)
        print(self.instructions[:5])
        print(self.code_blocks[:5])

    def trace_gp_sequence(self):
        """精确处理auipc的20位补码立即数"""
        def parse_auipc_imm(s):
            """专用于auipc的立即数解析"""
            s = s.lstrip('#').lower()  # 移除反汇编器前缀
            raw = int(s, 16)
            
            # 提取20位补码（符号扩展处理）
            imm_20bit = raw & 0xFFFFF         # 取低20位
            if imm_20bit & 0x80000:           # 检查符号位
                imm_20bit -= 0x100000         # 转换为有符号数
            return imm_20bit

        def parse_common_imm(s):
            """通用立即数解析（支持十进制/十六进制）"""
            s = s.strip().lower()
            sign = 1
            
            # 提取符号
            if s.startswith('-'):
                sign = -1
                s = s[1:]
            elif s.startswith('+'):
                s = s[1:]
            
            # 识别进制
            if s.startswith('0x'):
                return sign * int(s[2:], 16)
            else:
                return sign * int(s, 10)

        sequence = []
        current_gp = None
        active = False
        
        for instr in self.instructions:
            ops = [o.strip().lower() for o in instr.operand_extract]
            
            # 状态机控制
            if not active:
                if ops and ops[0] in ('gp', 'x3'):
                    active = True
                else:
                    continue
            elif not (ops and ops[0] in ('gp', 'x3')):
                break  # 遇到非连续写立即终止

            # 执行计算
            try:
                op = instr.opcode.lower()
                addr = int(instr.address, 16)
                
                if op == 'auipc':
                    imm = parse_auipc_imm(ops[1])
                    current_gp = (imm << 12) + addr
                elif op == 'lui':
                    imm = parse_common_imm(ops[1])
                    current_gp = imm << 12
                elif op == 'addi':
                    imm = parse_common_imm(ops[2])
                    current_gp += imm
                elif op == 'add' and all(r in ('gp','x3') for r in ops[1:3]):
                    current_gp *= 2
                else:
                    raise ValueError(f"不支持的指令: {op}")
                
                sequence.append((
                    instr.address,
                    f"{op} {', '.join(ops)}",
                    f"0x{current_gp:x} ({current_gp})"
                ))
            except Exception as e:
                sequence.append((
                    instr.address,
                    f"{op} {', '.join(ops)}",
                    f"⚠️ 错误: {type(e).__name__}({str(e)})"
                ))
                break  # 遇到错误立即终止
        print(sequence)
        if current_gp:        
            return sequence, hex(current_gp)
        else:
            return sequence, False
    
    def find_dead_register(self, addr, n, rd_regs, return_instr_address=False):
        """
        addr：起始地址
        n：dead寄存器数量
        rd_regs: 已经读过的寄存器，一般初始化为空集合set()
        return_instr_address: 是否返回有死寄存器的指令地址，默认为False，即不返回

        return：死寄存器名称列表，和对应指令的地址（如果需要返回）
        """
        global find_count
        dead_regs = set()
        print(f"begin finding deadreg from {addr}")
        if not self.inst_dist.get(addr, False):
            print("------------error begining addr-------------")
            if return_instr_address:
                return False, dead_regs, None
            else:
                return False, dead_regs
        #
        # print(self.instructions[self.inst_dist['0x3be520']])
        # print(self.inst_block_map[self.inst_dist['0x3be520']])
        # print(self.instructions[self.inst_dist['0x3be510']])
        # print(self.inst_block_map[self.inst_dist['0x3be510']])
        # print(self.instructions[self.inst_dist['0x3be54c']])
        # print(self.inst_block_map[self.inst_dist['0x3be54c']])
        # print(self.instructions[self.inst_dist['0x3be55c']])
        # print(self.inst_block_map[self.inst_dist['0x3be55c']])
#        for inst in self.instructions[self.inst_dist[addr]:]:
        i_index = self.inst_dist[addr]
        while True:
            inst = self.instructions[i_index]

            # CodeBlock(startaddr='0x1c2fea', endaddr='0x1c2ff6',jumpto='[]', instr_num='5':
            # zext.w a5, a5
            # slli a5, a5, 0x1
            # add a5, a5, a3
            # lhu a4, 0x0(a4)
            # sh a4, 0x0(a5)
            if inst.opcode == "j" or inst.opcode == "jal":
                logger.debug("find jump inst:" + inst.tostr())
                if not self.inst_dist.get(inst.jumpto[0], False):
                    print("error jump addr")
                    if return_instr_address:
                        return False, dead_regs, None
                    else:
                        return False, dead_regs
                i_index = self.inst_dist[inst.jumpto[0]]
                continue

            # if inst.operand_extract[0] is '':
            # 对每个指令进行处理，拿到指令的读写寄存器规则，0表示该寄存器为写，1表示为读
            inst_rule = riscv_instructions_regs_rules.get(inst.opcode)
            logger.debug(f"inst_rule {inst_rule}")
            if not inst_rule:
                # inst_rule为空列表，说明该指令没有操作数，直接跳过
                if inst_rule == []:
                    i_index += 1
                    continue
                # 为None说明字典中不存在该指令，将该指令的所有操作数加入到rd_regs中
                elif inst_rule == None:
                    logger.debug(f"{inst.opcode} is not in riscv_instructions_regs_rules+++++++++")
                    print(f"{inst.opcode} is not in riscv_instructions_regs_rules+++++++++")
                    for i in range(len(inst.operand_extract)):
                        op_str = inst.operand_extract[i]
                        # print(op_str)
                        idx = op_str.find('(')
                        if idx == -1:
                            rd_regs.add(op_str)
                        else:
                            idx_r = op_str.find(')')
                            register = op_str[idx + 1:idx_r]
                            if register != 'sp':
                                rd_regs.add(register)
                    i_index += 1
                    continue

            # print(len(inst.operand_extract), len(inst_rule))
            # 将指令中被读的寄存器加入到regs中 result = [b[i] for i in range(len(a)) if a[i] == 1]
            for i in range(len(inst_rule)):
                if inst_rule[i] == 1:
                    op_str = inst.operand_extract[i]
                    # print(op_str)
                    idx = op_str.find('(')
                    if idx == -1:
                        rd_regs.add(op_str)
                    else:
                        idx_r = op_str.find(')')
                        register = op_str[idx + 1:idx_r]
                        if register != 'sp':
                            rd_regs.add(register)
            # rd_regs = rd_regs.union({inst.operand_extract[i] for i in range(len(inst_rule)) if inst_rule[i] == 1})
            # 检查是否有写操作，如果有，则检查rd_regs中是否有对应的寄存器，如果没有，则其为dead register
            if not inst_rule[0]:
                logger.debug(f"inst_rule[0] {inst_rule[0]}")
                wr_reg = inst.operand_extract[0]
                if '0x' in wr_reg:
                    wr_reg = 'ra'
                if wr_reg not in rd_regs and wr_reg in riscv_reg_names:
                    dead_regs.add(wr_reg)
                    if len(dead_regs) >= n:
                        if return_instr_address:
                            return True, dead_regs, inst.address
                        else:
                            return True, dead_regs
            logger.debug(f"inst:{inst.tostr()}, finding deadregs:{dead_regs}, times: {find_count}")
            if find_count >= FIND_LIMIT:
                if return_instr_address:
                    return False, dead_regs, None
                else:
                    return False, dead_regs
            # 检查是否有跳转操作，如果有，则递归调用find_dead_register函数
            if inst.jumpto != []:
                jump_to_regs = []
                for addr_jumpto in inst.jumpto:
                    # 多个分支，将所有分支的dead寄存器取交集
                    # 检查jumpto的指令是否与当前指令的代码块相同，防止陷入死循环，首先判断是否跳转目标不存在
                    if not self.inst_dist.get(addr_jumpto, False):
                        print("error jump addr")
                        if return_instr_address:
                            return False, dead_regs, None
                        else:
                            return False, dead_regs
                    if self.inst_block_map[self.inst_dist[addr_jumpto]] == self.inst_block_map[self.inst_dist[inst.address]]:
                        if return_instr_address:
                            return False, dead_regs, None
                        else:
                            return False, dead_regs  # 再执行一遍该代码块与其他循环后取交集的dead寄存器一定在该代码块中已经找到，因此无需再进行
                    
                    find_count += 1
                    if find_count >= FIND_LIMIT:
                        if return_instr_address:
                            return False, dead_regs, None
                        else:
                            return False, dead_regs
                    jump_to_regs.append(self.find_dead_register(addr_jumpto, n - len(dead_regs), rd_regs)[1])
                else:
                    dead_regs.update(jump_to_regs[0].intersection(*jump_to_regs))
                if len(dead_regs) >= n:
                    if return_instr_address:
                        return True, dead_regs, inst.address
                    else:
                        return True, dead_regs
            logger.debug(f"i_index: {i_index}")
            i_index += 1

        if return_instr_address:
            return False, dead_regs, None
        else:
            return False, dead_regs

        

    def is_source_ISAX(self, instr):
        #logger.info("{} INSTRUCTION OPCODE IS: {}".format(instr.address, instr.opcode))
        #print(instr.opcode[0])
        if find_v_ext and instr.opcode[0] == 'v':
            return True
        if instr.opcode in self.source_ISAX_list:
            #this is a temporary impelementation for v ext
        #if i.opcode[0] == 'v' and i.opcode[1] == 'f':
            return True
        return False

    # 将代码块做合并操作。
    # 思路：遍历每一个有扩展指令的基本块，然后从其后找到第一个回溯的基本块，然后将这两个基本块中间的所有块合并。 
    def Merge(self):
        merged_blocks = []
        i = 0
        n = len(self.code_blocks)
        while i < n:
            #print("i: ", i)
            cb_ext = self.code_blocks[i]
            merge = False
            if not cb_ext.hasExtInstr:
                # merged_blocks.append(cb_ext)
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

    def extend_loop_cb(self, cb):
        res, ret, ext_index_lists = self.iter_binary(cb.instructions[-1].address, self.find_inst_with_deadreg)
        logger.debug(f"index_lists: {ext_index_lists}")
        if not res:
            return False
        inst_lists = self.convert_index_2_inst(ext_index_lists)
        logger.debug(f"index_lists: {inst_lists}")
        res, inst_tree = self.init_tree([cb.instructions] + inst_lists)
        if not res:
            return False
        inst_tree.get_final_inst_list()
        logger.debug(f"tree:{inst_tree.to_str()}")
        #TODO:处理跳板问题
        cb.update_instructions(inst_tree.final_inst_list)

    # 从合并后的代码块中提取出我们想要翻译的内容
    # 更正：需要在这里处理好dead reg的问题,即处理好deadreg对应的数据结构
    def extract_from_merged_cbs(self, merged_cbs):
        
        #简化版，希望别出事情
        def judge_trampoline_naive(cb):
            if len(cb.instructions) < 3:
                raise ValueError(f"cb {cb.startaddr} is too short: {len(cb)}")
            begining_len = cb.instructions[0].instrlen + cb.instructions[1].instrlen
            if begining_len == 8:
                cb.trampoline_type = 1
            elif begining_len == 6:
                cb.trampoline_type = 0
            elif begining_len == 4:
                if begining_len + cb.instructions[2].instrlen == 6:
                    cb.trampoline_type = 0
                elif begining_len + cb.instructions[2].instrlen == 8:
                    cb.trampoline_type = 1
            else:
                raise ValueError(f"cb {cb.startaddr} begining_len error {begining_len}")


        # 找到代码块里是否有扩展指令，是则返回指令的下标
        def find_source_ISAX(instructions):
            for i in range(len(instructions)):
                if self.is_source_ISAX(instructions[i]):
                    return i
            return -1
        
        # 看最后一条指令是不是跳转指令，或者是调到外面的分支跳转指令
        def last_jump_or_jumpout_branch(c):
            from Instr import branch_instr
            if c.instructions[-1].opcode[0] =='j':
                return True
            if c.instructions[-1].opcode in branch_instr:
                for addr in c.instructions[-1].jumpto:
                    if addr > c.endaddr or addr < c.startaddr:
                        return True
            return False
        
        # 检查扩展指令的下一条是不是32bit的指令
        def next_is_32bit(c, idx):
            if idx + 1 < c.instnum and c.instructions[idx + 1].instrlen == 4:
                return True
            return False
        
        # 从后往前找到第一个has_dead_reg为True的指令下标
        # 如果在找到之前，找到了一个扩展指令，那也返回它的下标+1（因为这是扩展指令，要保留它到最终的块中）
        # 没有返回-1
        def has_dead_registers(c):
            idx = c.instnum - 1
            while idx >= 0:
                if c.instructions[idx].has_dead_reg:
                    #logger.info("find dead reg in {} Instr: {} idx: {}".format(c.instructions[0].address, c.instructions[idx].tostr(), idx))
                    return idx
                elif self.is_source_ISAX(c.instructions[idx]):
                    #logger.info("find isax instr before dead reg in {} Instr: {} idx: {}".format(c.instructions[0].address, c.instructions[idx], idx))
                    return -1
                idx -= 1
            return -1
        
        # 看代码块的最后一条指令是否为间接跳转指令
        def final_indirect_jump(c):
            if INDIRECT_JUMP_VERSION == 1:
                if c.instructions[-1].opcode[0:4] == 'jalr' or c.instructions[-1].opcode[0:2] == 'jr':
                    print('indirect jmp: ', c.instructions[-1].opcode)
                    return True
            else:
                if c.instructions[-1].opcode[0:2] == 'jr':
                    print('indirect jmp: ', c.instructions[-1].opcode)
                    return True
            return False
        
        
        def update(splits, cb1, cb2, left_cb1 = True, left_cb2 = True):
            splits = splits[:-1]
            
            if cb1 != None and cb2 == None:
                splits.append(cb1)
                return splits

            if cb1 != None and left_cb1:
                splits.append(cb1)
            if cb2 != None and left_cb2:
                splits.append(cb2)
            return splits

        print("extract from merge cbs")
        extracted = []
        logger.debug(f"length:{len(merged_cbs)}")
        n = 0
        k = 0
        m = 0
        for c in merged_cbs:
            if c.is_loop:
                n += 1
            if len(c.instructions[-1].jumpto) == 2:
                m += 1
                if int(c.instructions[-1].jumpto[0], 16) < int(c.instructions[-1].address, 16) and not c.is_loop:
                    logger.debug(f"branch ahead addr:{c.instructions[-1].address}, startaddr: {c.startaddr}")
                    k += 1
        logger.debug(f"loop length:{n}")
        logger.debug(f"branch length:{m}")
        logger.debug(f"branch ahead length:{k}")
        #exit()

        for c in merged_cbs:

            idx = find_source_ISAX(c.instructions)
            # 没有就不管了
            if idx == -1:
                # extracted.extend(splits)
                continue
            #循环暂时不处理
            if c.is_loop:
                self.extend_loop_cb(c)
                judge_trampoline_naive(c)
                extracted.append(c)
                continue

            logger.info("START ADDRESS BEFORE PROCESSING: {}".format(c.startaddr))
            for instr in c.instructions:
                logger.info(f"inst: {instr.address}, {instr.tostr()}")

            if c.merged:
                # 这个块被合并过，那就不切了
                extracted.append(c)
                continue
            # 切块
            # need_split = True
            splits = [c,]
            # 首先找到里面是否有待翻译的扩展指令

            # 有扩展指令，检查扩展指令的下一条是不是32bit的指令
            if next_is_32bit(splits[-1], idx):
            # 如果是则从该扩展指令处分割
                cb1, cb2 = split_codeblock(splits[-1], idx)
                if cb2 == None:
                    tmp = cb1
                    cb1 = cb2
                    cb2 = tmp
                cb2.trampoline_type = 1
                splits = update(splits, cb1, cb2, False, True)
            else:
            # 如果不是，那么在该扩展指令的上一条指令处分割
                cb1, cb2 = split_codeblock(splits[-1], idx-1)
                if cb2 == None:
                    tmp = cb1
                    cb1 = cb2
                    cb2 = tmp
                if cb2.instructions[0].instrlen == 2:
                    cb2.trampoline_type = 2
                else:
                    cb2.trampoline_type = 1
                splits = update(splits, cb1, cb2, False, True)

            logger.info("START ADDRESS AFTER FIRST PROCESSING: {}".format(splits[-1].startaddr))
            for instr in splits[-1].instructions:
                logger.info(f"inst: {instr.address}, {instr.tostr()}")

            # 检查切后块最后一条指令是不是跳转指令，或者是跳到外面的分支跳转指令，如果是就从该指令处切分
            if last_jump_or_jumpout_branch(splits[-1]):
                cb1, cb2 = split_codeblock(splits[-1], splits[-1].instnum-1)
                splits = update(splits, cb1, cb2, True, False)

                idx2 = has_dead_registers(splits[-1])
                if idx2 != -1:
                    #logger.info("More than 1 branch")
                    cb1, cb2 = split_codeblock(splits[-1], idx2)
                    # patch this true and false?
                    splits = update(splits, cb1, cb2, True, False)
            else:
                # 找到最后一条has_dead_reg=True的代码块，使扩展指令在切后前面的代码块里
                if has_dead_registers(splits[-1]) != -1:
                    cb1, cb2 = split_codeblock(splits[-1], has_dead_registers(splits[-1]))
                    splits = update(splits, cb1, cb2, True, False)

            logger.info("START ADDRESS AFTER SECOND PROCESSING: {}".format(splits[-1].startaddr))
            for instr in splits[-1].instructions:
                logger.info(f"inst: {instr.address}, {instr.tostr()}")

            # 前面get_codeblocks_linear的操作能保证如果有间接跳转指令，那么它一定是最后一条指令，因此这里只需检查最后一条指令，如果是间接跳转就丢掉它。
            if final_indirect_jump(splits[-1]):
                cb1, cb2 = split_codeblock(splits[-1], splits[-1].instnum-1)
                splits = update(splits, cb1, cb2, True, False)

            # logger.info("Splits: {}".format(len(splits)))
            # for instr in splits[0].instructions:
            #     logger.info(instr.tostr())

            logger.info("START ADDRESS AFTER FINAL PROCESSING: {}".format(splits[-1].startaddr))
            for instr in splits[-1].instructions:
                logger.info(f"inst: {instr.address}, {instr.tostr()}")

            extracted.extend(splits)
        
        # i = 0
        # for cb in extracted:
        #     logger.info("After cutting {}:".format(i))
        #     for instr in cb.instructions:
        #         logger.info(instr.tostr())
        #     i += 1
        #exit()
        print("v trampoline number:")
        print(len(extracted))
            
        return extracted
            
    # 检查基本块中是否有间接跳转指令
    # version 1.切分Codeblock时，将间接跳转指令(jalr, jr)放到前一个Codeblock中，在第3步裁时把间接跳转裁剪掉。裁剪结束后，做优化：找到最后一条has_dead_reg=True的代码块，（优先保证这个条件）使扩展指令在切后前面的代码块里（see whether exists jalr or jr）
    # version 2.切分Codeblock时，如果遇到jalr或jal指令，不处理（即只处理jr）(see whether exists jr)，剩下的和version 1一样
    def checkIndirectJump(self):
        for p in range(len(self.code_blocks)):
            c = self.code_blocks[p]
            for i in c.instructions:
                # 版本1，切分Codeblock时，间接跳转指令(jalr, jr)会被切分到前一个Codeblock中，在第3步裁时把间接跳转裁剪掉。裁剪结束后，做优化：找到最后一条has_dead_reg=True的代码块，（优先保证这个条件）使扩展指令在切后前面的代码块里
                if (i.opcode[0:4] == 'jalr' or i.opcode[0:2] == 'jr') and INDIRECT_JUMP_VERSION == 1:
                    self.code_blocks[p].hasIndirectJump = True
                    break
                elif (i.opcode[0:2] == 'jr') and INDIRECT_JUMP_VERSION == 2:
                # 版本2, 切分Codeblock时，如果遇到jalr或jal指令，不处理（即只处理jr）
                    self.code_blocks[p].hasIndirectJump = True
                    break

    def find_source_ISAX(self):
        self.find_source_ISAX_stage1()
        self.handle_loops()
        self.checkIndirectJump()
        #print_codeblocks(b.code_blocks)
        logger.info("Merging...")
        #注意：这里在merge()这个函数里面挑选有扩展的代码段
        merged_blocks = self.Merge()
        #print_codeblocks(merged_blocks)
        extracted = self.extract_from_merged_cbs(merged_blocks)
        # 为了对后期没有死寄存器只能用gp的代码块做处理，需要知道这些块未被extract裁剪前的样子，因此这个函数再添加一个返回原样代码块的列表
        return extracted

    def safe_get_index(self, addr):
        if not self.inst_dist.get(addr, False):
            return False, -1
        index = self.inst_dist[addr]
        return True, index 

    def append_branch(self, jumpto, queue):
        for addr in jumpto:
            exist, index = self.safe_get_index(addr)
            if not exist:
                return False, []
            queue.append(index)
        return True, queue


    
    #顺序执行，handle_func是基于这个顺序执行的处理过程
    #ret用于记录每次handle_func的返回值（如果需要的话）
    def iter_binary(self, addr, handle_func):
        exec_queue = []
        ret = []
        exist, index = self.safe_get_index(addr)
        if not exist:
            return False, [], []
        exec_queue.append(index)
        res = True
        iteration = 0
        #最大搜索四层
        max_iteration = 4
        has_iterred_list = []
        while len(exec_queue):
            i = exec_queue.pop(0)
            has_iterred = []
            while True:
                inst = self.instructions[i]
                if len(inst.jumpto) != 0:
                    logger.debug(f"{inst.address} jumpto is {inst.jumpto} ")
                    res, exec_queue = self.append_branch(inst.jumpto, exec_queue)
                    has_iterred.append(i)
                    if not res:
                        return False, [], []
                    break
                handle_res = handle_func(inst, ret)
                has_iterred.append(i)
                if handle_res:
                    break
                i += 1
                loop, list_index = if_iterred(i, has_iterred_list)
                if loop:
                    #暂时默认循环都拿进extratext里面
                    #在这里合并循环的代码块
                    has_iterred_list[list_index] += has_iterred
                    has_iterred = []
                    break
            iteration += 1
            logger.debug(f"{addr} in iteration: {iteration}'s exec_queue is {exec_queue}")
            has_iterred_list.append(has_iterred)
            if iteration > max_iteration:
                res = False
                break

        has_iterred_list = [x for x in has_iterred_list if len(x) > 0]
        for insts in has_iterred_list:
            insts.sort()
                    
        return res, ret, has_iterred_list
    
    #上述的一个handle_func的一个实例
    #用于在迭代过程中找到一个可以产生dead寄存器的指令
    def find_inst_with_deadreg(self, inst, ret):
        if inst.has_dead_reg:
            ret.append(inst)
            return True
        return False


    def convert_index_2_inst(self, index_lists):
        insts_lists = []
        for index_list in index_lists:
            insts_list = [self.instructions[i] for i in index_list]
            insts_lists.append(insts_list)
        return insts_lists


    def init_leaf_list(self, inst_lists):
        leaf_list = []
        for inst_list in inst_lists:
            leaf_list.append(inst_list_leaf(inst_list))
        return leaf_list

    def init_tree(self, inst_lists):
        print(inst_lists_to_str(inst_lists))
        leaf_list = self.init_leaf_list(inst_lists)
        tree = inst_list_tree()
        num = -1
        last_num = len(leaf_list)
        print("last_num:", last_num)
        while num != 0:
            if last_num == num:
                logger.debug(f"find {num} isolate leaf")
                return False, None
            last_num = num
            num = 0
            for leaf in leaf_list:
                res = tree.append_leaf(leaf)
                if not res:
                    num += 1
        return True, tree





        



        

def inst_lists_to_str(insts_lists):
    content = ""
    for inst_list in insts_lists:
        content += "codeblock:\n"
        for inst in inst_list:
            content += f"{inst.address}: {inst.tostr()}\n"
    return content

def inst_list_to_str(inst_list):
    content = ''
    for inst in inst_list:
        content += f"{inst.address}: {inst.tostr()}\n"
    return content



#codeblock用来做编译和翻译相关的功能，inst_list_tree用来实现控制流相关
#这里假设都是没有被压缩的指令用来计算地址

class inst_list_leaf:
    
    def __init__(self, sorted_inst_list):
        self.inst_list = sorted_inst_list
        self.start_address = self.inst_list[0].address
        self.jump_to_address, self.next_address = self.get_jump_address()
        #判断自己的块内部是否是循环
        self.is_loop = self.if_loop()
        self.next = None
        self.jump = None
        
    def get_jump_address(self):
        inst = self.inst_list[-1]
        jumpto = inst.jumpto
        if len(jumpto) == 0:
            return None, hex_add(inst.address, str(inst.instrlen), 16)
        elif len(jumpto) == 1:
            return None, jumpto[0]
        elif len(jumpto) == 2:
            return jumpto[0], jumpto[1]

    def if_loop(self):
        if self.jump_to_address == None:
            return False
        for inst in self.inst_list:
            if inst.address == self.jump_to_address:
                return True
        return False



class inst_list_tree:
    
    def __init__(self):
        self.content = ""
        self.head = None
        #为了方便遍历，将所有叶子放在列表里面
        self.leaf_list = []
        #用来将tree的所有指令进行整合
        self.final_inst_list = []
        #这里会处理jumpoffset的问题(似乎不能在这里处理)
        #self.jump_offset = None
        #exit_trampoline中的元素:([指令的索引], 返回的地址)
        self.exit_trampoline = []
        self.original_start_address = None

    
    def append_leaf(self, inst_list_leaf):
        if inst_list_leaf in self.leaf_list:
            return True

        if self.head == None:
            self.head = inst_list_leaf
            self.leaf_list.append(inst_list_leaf)
            return True

        #整体流程先找头，对外面的输入的数组遍历两遍
        if self.head.start_address == inst_list_leaf.next:
            tmp = self.head
            self.head = inst_list_leaf
            self.head.next = tmp
            self.leaf_list.append(inst_list_leaf)
            return True

        if self.head.start_address == inst_list_leaf.jump:
            tmp = self.head
            self.head = inst_list_leaf
            self.head.jump = tmp
            self.leaf_list.append(inst_list_leaf)
            return True

        #TODO:这里没有做地址的检查，希望不会出问题
        for leaf in self.leaf_list:
            if leaf.next_address == inst_list_leaf.start_address:
                leaf.next = inst_list_leaf
                self.leaf_list.append(inst_list_leaf)
                return True
            elif leaf.jump_to_address == inst_list_leaf.start_address:
                leaf.jump = inst_list_leaf
                self.leaf_list.append(inst_list_leaf)
                return True
        return False

    #广度优先
    def to_str(self):
        print_queue = []
        content = ''
        print_queue.append(self.head)
        while True:
            flag = 0
            leaf = print_queue.pop(0)
            inst_list = leaf.inst_list
            content += inst_list_to_str(inst_list)
            if leaf.next:
                print_queue.append(leaf.next)
                flag += 1
            if leaf.jump:
                print_queue.append(leaf.jump)
                flag += 1
            if flag == 0:
                content += "end\n"
            if len(print_queue) == 0:
                break
        return content

    def get_final_inst_list(self):
        print_queue = []
        print_queue.append(self.head)
        while True:
            flag = 0
            leaf = print_queue.pop(0)
            inst_list = leaf.inst_list
            self.final_inst_list += [x for x in inst_list]
            if leaf.next:
                print_queue.append(leaf.next)
                flag += 1
            if leaf.jump:
                print_queue.append(leaf.jump)
                flag += 1
            #TODO:更新跳板信息
            if flag == 0:
                pass
            if len(print_queue) == 0:
                break






def if_iterred(index, has_iterred_list):
    for i, x in enumerate(has_iterred_list):
        if index in x:
            return True, i
    return False, -1



def print_codeblocks(cbs):
    print("共有"+str(len(cbs))+"个代码块。")
    for c in cbs:
        print("CodeBlock: ", c.startaddr, "~", c.endaddr, \
            " instr_num: ", c.instnum, " jumpfrom: ", c.jumpfrom, \
            " jumpto: ", c.jumpto, " hasExtInstr: ", c.hasExtInstr, \
            " hasIndirectJump: ", c.hasIndirectJump)

if __name__ == "__main__":
    b = Binary(test_objdump_path, test_binary_name, ["sh1add"])
    extracted, _ = b.find_source_ISAX()
    print("Merging...")
    print_codeblocks(extracted)
    #print(b.find_dead_register('0x105b6', 3, set()))  # 6个就找不到了
