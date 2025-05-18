import sys
import copy
import os
import subprocess
sys.path.append("objdumpeg")
from objdumpeg.CodeBlock import CodeBlock
from objdumpeg.Binary import Binary
from InstrTemplate import InstrTemplate
from Translator import Translator
from objdumpeg.tools import hex_add
from objdumpeg.Instr import get_text_offset, branch_instr, direct_jump_instr, indirect_jump_instr
from AddrAllocator import AddrAllocator
from objdumpeg.Object import Object
import os
import logging
import yaml
import json
logger = logging.getLogger(__name__)
test_binary_name = "axpy_vector.exe"
source_ISAX_list = [] 
class uncompiled_object:
    def __init__(self, cb, jumpoffset):
        self.cb = cb
        self.cb_store = copy.copy(cb)
        self.name = cb.startaddr
        self.startaddr = cb.startaddr
        self.jumpoffset = jumpoffset
        self.trampoline_type = cb.trampoline_type
    def init_translated_cb_addr(self, vaddr):
        self.translated_cb_addr = vaddr
    def init_content(self, content):
        self.content = content
    def get_address_info_unc_obj(self):
        address_info = {}
        address_info['trampoline_addr'] = self.cb.startaddr
        address_info['trampoline_addr_ret'] = hex_add(self.cb.endaddr, str(self.cb.instructions[-1].instrlen), 10)
        address_info['translated_cb_addr'] = hex_add(self.cb.startaddr, self.jumpoffset, 16)
        if self.trampoline_type == 1:
            print("{} {} {} {}".format(address_info['trampoline_addr'], address_info['translated_cb_addr'], type(address_info['trampoline_addr']), type(address_info['translated_cb_addr'])))
            address_info['trampoline_addr'] = hex_add(address_info['trampoline_addr'], '0x2', 16)
            address_info['translated_cb_addr'] = hex_add(address_info['translated_cb_addr'], '0x2', 16)
        return address_info
class binarypatcher:
    def __init__(self, objdirpath, objdump_path, cc_path, binary_name, source_ISAX_list, merge=True):
        self.binary = Binary(objdump_path, binary_name, source_ISAX_list, merge)
        self.original_cb_list = self.binary.code_blocks
        self.cb_list = self.binary.find_source_ISAX()
        self.objdirpath = objdirpath
        self.objdump_path = objdump_path
        self.source_ISAX_list = source_ISAX_list
        self.cc_path = cc_path
        self.addr_alloctor = None
        self.overlapping_cb = []
    def update_cb_list(self, overlapping_cb):
        if len(overlapping_cb) == 0:
            return
        tmp = []
        for cb in self.cb_list:
            if cb in overlapping_cb:
                tmp.append(cb)
        logger.info("overlapping_cb: {}".format(overlapping_cb))
        self.cb_list = tmp
    def init_offset(self, jumpoffset: str):
        self.jumpoffset = jumpoffset
        logger.info("binarypatcher init offset:{}".format(jumpoffset))
        self.jumptype = Translator.LONGJMP
        if self.jumpoffset:
            logger.info("jumptype:long jump")
        else:
            logger.info("jumptype:short jump")
        vaddr = hex_add(jumpoffset, hex(get_text_offset(self.binary.binary_name)[0]), 16)
        self.patch_instr_vaddr_start = vaddr
        logger.info("vaddr of additional codeblocks:{}".format(jumpoffset))
        self.trampoline_addr = []
        self.unc_obj_list = []
        self.obj_list = []
    def init_trampoline_vaddr(self):
        filed = open("y.txt", "w")
        for cb in self.cb_list:
            filed.write(repr(cb) + ':\n' + cb.tostr()+'\n')
        for cb in self.cb_list:
            res, reg = self.binary.find_dead_register(cb.startaddr, 1, set())
            if not res:
                logger.warning("not enough dead trampoline regs for {}".format(cb.startaddr))
                logger.warning("use gp for {}".format(cb.startaddr))
                reg = set(["gp"])
                cb.use_gp_jump_in = True
            self.trampoline_addr.append((cb.startaddr, list(reg), cb.trampoline_type))
            logger.debug("find trampoline ret addr:{}, dead reg:{}, trampoline type: {}".format(cb.startaddr, reg, cb.trampoline_type))
        logger.info("number of addr needed be patched:{}".format(len(self.trampoline_addr)))
        logger.debug("addr needed be patched:{}".format(self.trampoline_addr))
    def init_unc_obj_list(self, use_gp_data_addr):
        f = open(use_gp_data_addr, 'w+')
        unc_obj_list = []
        use_gp_cb_list = []
        for cb in self.cb_list:
            un_cb = uncompiled_object(cb, self.jumpoffset)
            ret_addr = hex_add(cb.endaddr, str(cb.instructions[-1].instrlen), 10)
            res, reg = self.binary.find_dead_register(ret_addr, 1, set())
            logger.debug("trampoline ret addr:{}, dead reg:{}".format(ret_addr, reg))
            if not res:
                logger.warning("not enough dead trampoline regs for {}".format(ret_addr))
                logger.warning("use gp for {}".format(ret_addr))
                reg = set(["gp"])
                f.write("Codeblock startaddr: {}\n".format(cb.startaddr))
                use_gp_cb_list.append(cb.original_startaddress)
            if cb.use_gp_jump_in:
                gp_trace, gp_value = self.binary.trace_gp_sequence()
                t = Translator({}, jmptype=self.jumptype, jmpout_reg=list(reg)[0], is_use_gp=gp_value )
            t = Translator({}, jmptype=self.jumptype, jmpout_reg=list(reg)[0])
            un_cb.init_content(t.translate_ext_insts(cb, self.jumpoffset))
            unc_obj_list.append(un_cb)
        self.unc_obj_list = unc_obj_list
        f.close()
        return use_gp_cb_list
    def generate_objs_stage1(self, cc_option):
        obj_list = []
        logger.info("generate objects to {}".format(self.objdirpath))
        for unc_obj in self.unc_obj_list:
            checkdir(self.objdirpath)
            objname = os.path.join(self.objdirpath, unc_obj.name) + ".o"
            self.compile_unobj(unc_obj, cc_option)
            obj = Object(self.objdump_path, objname, self.source_ISAX_list)
            obj.combine_uncobj(unc_obj)
            logger.debug("stage1 object {} generated".format(objname))
            obj_list.append(obj)
        self.obj_list = obj_list
    def generate_objs_stage2(self, cc_option):
        logger.info("generate objects for stage2")
        self.update_address4stg2()
        self.generate_objs_stage1(cc_option)
    def compile_unobj(self, unc_obj, cc_option):
        outpath_head = os.path.join(self.objdirpath, unc_obj.name)
        asm_file = generate_asm_file(outpath_head, unc_obj.content)
        try:
            logger.info("outpath_head: {}".format(outpath_head))
            cmdlist = [self.cc_path, asm_file] + cc_option + [outpath_head + ".o"]
            result = subprocess.run(cmdlist, 
                                    capture_output=True, text=True, check=True)
            output = result.stdout
            logger.debug("compile cmd:{}".format(" ".join(cmdlist)))
            logger.debug("{}".format(" ".join(output)))
        except subprocess.CalledProcessError as e:
            print(f"Error executing command: {e}")
            print(f"Command output: {e.output}")
    def update_address4stg2(self):
        logger.info("update objects for stage2")
        for o in self.obj_list:
            o.init_address_info(o.unc_obj.get_address_info_unc_obj())
        self.init_addr_allocator()
        for o in self.obj_list:
            correct_jmp_aim(o)
    def get_final_codeblock(self, outputpath):
        for o in self.obj_list:
            o.init_address_info(o.unc_obj.get_address_info_unc_obj())
        self.addr_alloctor.init_objects(self.obj_list)
        overlapping_unc_obj = self.addr_alloctor.merge_binary_files(os.path.join(self.objdirpath, outputpath))
        self.overlapping_cb = [unc_obj.cb_store for unc_obj in overlapping_unc_obj]
        for o in overlapping_unc_obj:
            if o.name in self.trampoline_addr:
                self.trampoline_addr.remove(o.name)
        logger.info("number of trampoline_addr in this turn {}".format(len(self.trampoline_addr)))
    def init_addr_allocator(self):
        logger.info("init addr AddrAllocator")
        self.addr_alloctor = AddrAllocator(self.jumptype, self.jumpoffset, self.patch_instr_vaddr_start)
        self.addr_alloctor.init_objects(self.obj_list)
    def get_pass_info(self, path):
        with open(path, 'w+') as f:
            json.dump({
                "jumpoffset": self.jumpoffset,
                "trampoline_addr": self.trampoline_addr,
                "patch_instr_vaddr_start": self.patch_instr_vaddr_start
            }, f)
        return self.jumpoffset, self.trampoline_addr, self.patch_instr_vaddr_start
    def process_use_gp_cbs(self, use_gp_cb_list, i):
        def get_cb(original_startaddress):
            for i in range(len(self.original_cb_list)):
                if self.original_cb_list[i].original_startaddress == original_startaddress:
                    self.original_cb_list[i].index = i
                    return self.original_cb_list[i], i
            raise ValueError("There is no codeblock with given start address found. Please check whether the given address is correct.")            
        def get_target_instr(instr):
            for cb in self.original_cb_list:
                if cb.startaddr <= instr.jumpto[0] <= cb.endaddr:
                    return cb
            return None
        f = open("tmp/gp-"+str(i)+".txt", "w+")
        for i in range(len(use_gp_cb_list)):
            original_startaddress = use_gp_cb_list[i]
            f.write("An Unmatched Codeblock: {} ".format(original_startaddress))
            cb, index = get_cb(original_startaddress)
            if cb.instructions[-1].opcode in direct_jump_instr:
                res, reg, addr = self.binary.find_dead_register(cb.instructions[-1].jumpto[0], 1, set(), True)
                if len(reg) != 0:
                    f.write("Dead Registers Found here: direct jump: {} {}\n".format(addr, list(reg)[0]))
                else:
                    res, ret, ext_len = self.binary.iter_binary(cb.instructions[-1].jumpto[0], self.binary.find_inst_with_deadreg)
                    if res:
                        f.write("Dead Registers can be extended to {}\n".format(len(ext_len)))
                    else:
                        f.write("can not find direct jump{}\n".format(cb.instructions[-1].jumpto[0]))
            elif cb.instructions[-1].opcode in indirect_jump_instr:
                f.write("Dead Registers is indirect jump\n")
            elif cb.instructions[-1].opcode in branch_instr:
                res1, reg1, addr1 = self.binary.find_dead_register(self.original_cb_list[index+1].startaddr, 1, set(), True)
                tmpcb = get_target_instr(cb.instructions[-1])
                flag = 0
                if tmpcb == None:
                    flag = 1
                    reg2 = set()
                else:
                    res2, reg2, addr2 = self.binary.find_dead_register(tmpcb.startaddr, 1, set(), True)
                if len(reg1) == 0 and len(reg2) == 0:
                    flag = 1
                    pass
                elif len(reg1) != 0 and len(reg2) != 0:
                    f.write("Dead Registers Found here: Codeblock1: {} {} Codeblock2: {} {}\n".format(addr1, list(reg1)[0], addr2, list(reg2)[0]))
                else:
                    flag = 1
                    pass
                if flag:
                    res, ret, ext_len = self.binary.iter_binary(cb.instructions[-1].address, self.binary.find_inst_with_deadreg)
                    if res:
                        f.write("Dead Registers can be extended to {} ".format(len(ext_len)))
                        for i in ext_len:
                            inst = self.binary.instructions[i]
                            f.write("ext_inst {}:, {} ".format(inst.address, inst.tostr()))
                        f.write("\n")
                    else:
                        f.write("can not find branch jump{}\n".format(cb.instructions[-1].jumpto[0]))
            else:
                res, ret, ext_len = self.binary.iter_binary(cb.instructions[-1].address, self.binary.find_inst_with_deadreg)
                if res:
                    f.write("Dead Registers can be extended to {}\n".format(len(ext_len)))
                else:
                    f.write("can not find {}\n".format(cb.instructions[-1].address))
        f.close()
def correct_jmp_aim(obj):
    unc_obj = obj.unc_obj
    auipc_imm = obj.patch_ret_offset[0]
    jmp_offset = obj.patch_ret_offset[1]
    contentlist = unc_obj.content.split("\n")
    contentlist[-2] = contentlist[-2].replace(" 0", " "+auipc_imm)
    contentlist[-1] = contentlist[-1].replace("0(", jmp_offset+'(')
    unc_obj.content = "\n".join(contentlist)
    logger.debug("replace info :obj {}, replace: {} {}".format(unc_obj.name, contentlist[-2], contentlist[-1]))
def generate_asm_file(outpath_head, content):
    with open(outpath_head + '.s', "w") as f:
        f.write(content)
    return outpath_head + '.s'
def checkdir(dirpath):
    if not os.path.abspath(dirpath):
        logger.error("obj dir need abs path{}".format(dirpath))
        raise Exception("obj dir need abs path")
    if not os.path.exists(dirpath):
        logger.error("obj dir not exists{}".format(dirpath))
        raise Exception("obj dir not exists")
