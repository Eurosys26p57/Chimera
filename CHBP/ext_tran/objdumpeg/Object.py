from Instr import disam_binary
import os
from Instr import extract_text_b
from tools import hex_add
from CodeBlock import get_codeblocks_linear
import logging
logger = logging.getLogger(__name__)

# extract information from .o(compiled by .s)
class Object:
    def __init__(self, objdump_path, binary_name, source_ISAX_list):
        self.objdump_path = objdump_path
        self.binary_name = binary_name
        if not os.path.isabs(binary_name):
            raise Exception("path of object must be abs path")
        self.source_ISAX_list = source_ISAX_list
        self.instructions, self.erraddr = disam_binary(self.binary_name, self.objdump_path)
        self.code_blocks = get_codeblocks_linear(self.instructions)
        #All values that need to be added to an address are string type, and those representing lengths are in decimal
        self.objlen = sum([int(i.instrlen) for i in self.instructions])
        self.auipclen = self.objlen - self.instructions[-2].instrlen - self.instructions[-1].instrlen
        #the first ele is auipc offset, second one is jr offset
        self.patch_ret_offset = []
        self.trampoline_type = None

    def init_address_info(self, address_info):
        self.trampoline_addr = address_info["trampoline_addr"]
        self.trampoline_addr_ret = address_info["trampoline_addr_ret"]
        self.translated_cb_addr = address_info["translated_cb_addr"]
        logger.debug("addrinfo of object {}: trampoline_addr {}, trampoline_addr_ret {}, translated_cb_addr {}".format(self.binary_name, self.trampoline_addr, self.trampoline_addr_ret, self.translated_cb_addr))

    def extract_objtext_b(self):
        self.textcontent = extract_text_b(self.binary_name)

    def combine_uncobj(self, unc_obj):
        self.unc_obj = unc_obj
        

if __name__ == "__main__":
    b = Object(test_objdump_path, test_binary_name, [])
    b.extract_objtext_b()
    print(b.instructions)
    print(b.objlen)
    print(b.textcontent)
