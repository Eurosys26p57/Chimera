import subprocess
import re
from tools import hex_add
import lief
import logging
from utils import riscv_instructions_regs_rules
logger = logging.getLogger(__name__)
INDIRECT_JUMP_VERSION = 1
branch_instr = ('bgtz', 'beq', 'bne', 'beqz', 'bnez', 'blt', 'bge', 'bltz', 'bgez', 'bltu', 'bgeu', 'blez')
two_opr_branch_instr = ('bgtz', 'bnez', 'bltz', 'bgez', 'beqz', 'blez')
three_opr_branch_instr = ('beq', 'bne', 'blt', 'bge', 'bltu', 'bgeu')
jmp_branch_instr = ('j', 'jal', 'bgtz', 'beq', 'bne', 'beqz', 'bnez', 'blt', 'bge', 'bltz', 'bgez', 'bltu', 'bgeu', 'blez', 'jr')
direct_jump_instr = ('j', 'jal')
indirect_jump_instr = ('jr', 'jalr')
return_instr = ('ret')
OPTIONS = ['-d',  '--mattr=+v,+b']
def readfromdasm(dasm_str):
    dasmlist = dasm_str.split(' ')
    return Instruction(opcode=dasmlist[0], operand=dasmlist[1])
class Instruction:
    def __init__(self, opcode: str, operand: str,  address: str = "0x0000", machine_code: str = "0000"):
        self.address = address      
        self.opcode = opcode        
        self.operand = operand.split("<")[0]      
        self.operand_extract = extractop(self.operand)
        self.machine_code = machine_code  
        self.instrlen = int(len(machine_code)/2)
        self.jumpto = []
        self.has_dead_reg = self.has_dead_register()
        self.jump_to_instr = -1  
        self.jumpfrom = []
        self.isblockbegin = False
        self.isblockend = False
        self.isret = False
        self.cb_index = -1  
        self.is_logue = False
    def __repr__(self):
        return (f"Instruction(address='{self.address}', opcode='{self.opcode}', "
                f"operand='{self.operand}', machine_code='{self.machine_code}', "
                f"instrlen= '{self.instrlen}', jumpto='{self.jumpto}',"
                f"operand_extract = '{self.operand_extract}', jumpfrom='{self.jumpfrom}',"
                f"cb_index = '{self.cb_index}')")
    def tostr(self):
        return self.opcode +" "+ self.operand
    def has_dead_register(self):
        if self.opcode in return_instr:
            return True
        if self.opcode == 'ebreak' or self.opcode == 'unimp' or self.opcode == 'ecall' or self.opcode == 'nop':
            return False
        if self.opcode[0] == 'v' or self.opcode[0] == 'f':
            return False
        rules = riscv_instructions_regs_rules.get(self.opcode)
        if rules is not None and len(rules) > 0:
            if len(rules) == 1:
                return not rules[0]
            if not rules[0]:
                if self.operand_extract is not None and len(self.operand_extract) > 0:
                    for reg in self.operand_extract[1:]:
                        if self.operand_extract[0] in reg:
                            return False
            return not rules[0]
        else:
            return False
def extractop(operand):
    operandlist = [x.lstrip().strip() for x in operand.split(",")]
    return operandlist
def update_jump_aim(instructions, erraddr):
    for i in instructions.values():
        if i.isblockend:
            for j in i.jumpto:
                if j not in instructions.keys():
                    erraddr.append((i.address,j))
                else:
                    instructions[j].jumpfrom.append(i.address)
                    instructions[j].isblockbegin = True
def addjumpto(instr, base):
    if instr.opcode == 'j':
        instr.jumpto.append(instr.operand.split(' ')[0])
    elif instr.opcode =='jal':
        instr.jumpto.append(instr.operand.split(' ')[0])
    elif instr.opcode in two_opr_branch_instr:
        instr.jumpto.append(instr.operand_extract[1])
        instr.jumpto.append(hex_add(instr.address, str(instr.instrlen), base))
    elif instr.opcode in three_opr_branch_instr:
        instr.jumpto.append(instr.operand_extract[2])
        instr.jumpto.append(hex_add(instr.address, str(instr.instrlen), base))
def identify_short_direct_jump_type1(instr):
    base = 10
    jmp_branch_instr_tmp = jmp_branch_instr
    if INDIRECT_JUMP_VERSION == 1:
        jmp_branch_instr_tmp = jmp_branch_instr_tmp + tuple(("jalr", "jr"))
    if instr.opcode in jmp_branch_instr_tmp:
        instr.isblockend = True
        addjumpto(instr, base)
    elif instr.opcode in return_instr:
        instr.isblockend = True
        instr.isret = True
def parse_objdump_output_type1(output: str):
    instructions = {}
    lines = output.splitlines()
    erraddr = []
    for line in lines:
        match = re.search(r'[0-9a-f]+:[\s]+[0-9a-f]+[\s]*\t[0-9a-z.]+(?:\t[0-9a-z,.\(\)\-\s]+)?', line)
        if match:
            address = '0x' + match.group(0).split(':')[0]
            machine_code = re.findall(r'[0-9a-f]+', match.group(0).split(':')[1])[0]
            opcode = match.group(0).split(':')[1].split('\t')[1]
            if len(match.group(0).split(':')[1].split('\t')) > 2:
                operand = match.group(0).split(':')[1].split('\t')[2]
            else:
                operand = ""
            i = Instruction(opcode, operand, address, machine_code)
            identify_short_direct_jump_type1(i)
            instructions[address] = i
    update_jump_aim(instructions, erraddr)
    return instructions, erraddr
def addjumpto_relative(instr, base):
    if instr.opcode == 'j':
        instr.jumpto.append(hex_add(instr.address, instr.operand.split(' ')[0], base))
    elif instr.opcode =='jal':
        instr.jumpto.append(hex_add(instr.address, instr.operand.split(' ')[0], base))
    elif instr.opcode in two_opr_branch_instr:
        instr.jumpto.append(hex_add(instr.address, instr.operand.split()[1], base))
        instr.jumpto.append(hex_add(instr.address, str(instr.instrlen), base))
    elif instr.opcode in three_opr_branch_instr:
        instr.jumpto.append(hex_add(instr.address, instr.operand.split()[2], base))
        instr.jumpto.append(hex_add(instr.address, str(instr.instrlen), base))
def identify_short_direct_jump_type2(instr):
    base = 10
    if instr.opcode in jmp_branch_instr:
        instr.isblockend = True
        addjumpto_relative(instr, base)
    elif instr.opcode in return_instr:
        instr.isblockend = True
        instr.isret = True
def parse_objdump_output_type2(output: str):
    instructions = {}
    erraddr = []
    lines = output.splitlines()
    for line in lines:
        match = re.match(r'^\s*([0-9a-fA-F]+):\s+((?:[0-9a-fA-F]{2}\s+)+)\s+([\w.]+)\s*(.*)$', line)
        if match:
            address = '0x' + match.group(1)
            machine_code = ''.join(match.group(2).split()).lower()  
            opcode = match.group(3)                                
            operand = match.group(4).strip() if match.group(4) else ''  
            i = Instruction(opcode, operand, address, machine_code)
            identify_short_direct_jump_type2(i)
            instructions[address] = i
    update_jump_aim(instructions, erraddr)
    return instructions, erraddr
def extract_text_b(binary_name):
    binary = lief.parse(binary_name)
    text_section = binary.get_section('.text')
    if text_section is None:
        raise ValueError('.text section not found in the binary.')
    offset = text_section.offset
    size = text_section.size
    with open(binary_name, 'rb') as file:
        file.seek(offset)
        text_data = file.read(size)
    return text_data
def get_text_offset(binary_name):
    binary = lief.parse(binary_name)
    text_section = binary.get_section('.text')
    if text_section is None:
        raise ValueError('.text section not found in the binary.')
    offset = text_section.offset
    size = text_section.size
    return offset, size
def disam_binary(binary_name, objdump_path):
    instructions = []
    options = OPTIONS
    erraddr = []
    try:
        cmdlist = [objdump_path] + options + [binary_name]
        logger.debug("objdump cmd: {}".format(" ".join(cmdlist)))
        result = subprocess.run(cmdlist,  
                                capture_output=True, text=True, check=True)
        output = result.stdout
        instructions_dict, erraddr = parse_objdump_output_type1(output)
        for instruction in instructions_dict.values():
            instructions.append(instruction)
        logger.info("error jump addr of binary_name: {}".format(erraddr))
    except subprocess.CalledProcessError as e:
        print(f"Error executing command: {e}")
        print(f"Command output: {e.output}")
    return instructions, erraddr
if __name__ == "__main__":
    i1 = Instruction("ld", "a3, 0x1(a0)")
    i2 = Instruction("ret", "")
    print(i1.has_dead_reg, i2.has_dead_reg)
