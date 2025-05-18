import Translator
import os
from objdumpeg.tools import hex_add
from objdumpeg.tools import auipc_caculate
from objdumpeg.Object import Object
import logging
logger = logging.getLogger(__name__)
class AddrAllocator:
    def __init__(self, jmptype, jmpoffset: str, patch_instr_vaddr_start: str):
        self.jmptype = jmptype
        self.jmpoffset = jmpoffset
        self.patch_instr_vaddr_start = patch_instr_vaddr_start
    def init_objects(self, obj_list):
        self.obj_list = sorted(obj_list, key = lambda x: int(x.trampoline_addr, 16))
        for o in self.obj_list:
            o.patch_ret_offset = self.caculate_jumpoffset(o)
    def caculate_jumpoffset(self, obj):
        if self.jmptype == Translator.Translator.SHORTJMP:
            return [self.jmpoffset]
        objvaddr = hex_add(obj.trampoline_addr, self.jmpoffset, 16)
        auipcaddr = hex_add(objvaddr, str(obj.auipclen), 10)
        auipc_imm, jr_offset = auipc_caculate(auipcaddr, obj.trampoline_addr_ret)
        return [auipc_imm, jr_offset]
    def merge_binary_files(self, output_file_path):
        logger.info("merge to {}".format(output_file_path))
        overlapping_unc_obj = []
        for o in self.obj_list:
            logger.debug("get .text section of {}".format(o.binary_name))
            o.extract_objtext_b()
        try:
            with open(output_file_path, 'wb') as output_file:
                for x in range(int(self.patch_instr_vaddr_start, 16) - 
                        int(self.jmpoffset, 16)):
                        output_file.write(b'\x00')
                fdoffset = 0
                for o in self.obj_list:
                    cb_addr = int(o.translated_cb_addr, 16) 
                    vaddr_start = int(self.patch_instr_vaddr_start, 16)
                    logger.debug("obj name:{}, cb_start_addr:{}, current vaddr:{}".format(o.binary_name, hex(cb_addr), hex(vaddr_start)))
                    if cb_addr < vaddr_start:
                        raise Exception("cb vaddr error!")
                    cb_fdoffset = cb_addr - vaddr_start
                    cb_seek_len = cb_fdoffset - fdoffset
                    logger.debug("obj:{}, cb_seek_len:{}, fdoffset:{}".format(o.binary_name, hex(cb_seek_len), hex(fdoffset)))
                    if cb_seek_len < 0:
                        logger.info("overlapping: {}".format(o.unc_obj.name))
                        overlapping_unc_obj.append(o.unc_obj)
                        continue
                    for x in range(cb_seek_len):
                        output_file.write(b'\x00')
                    logger.info("已合并{}到 {}".format(o.binary_name,output_file_path))
                    content_len = output_file.write(o.textcontent)
                    fdoffset = cb_fdoffset + content_len
        except Exception as e:
            print(f"发生错误: {e}")
        return overlapping_unc_obj
