import logging
import parseconfig
from objdumpeg.tools import hex_add
CONFIG_PATH = "config.yaml"
FORMAT = '%(levelname)s - %(asctime)s - %(name)s - %(message)s'
logging.basicConfig(filename="patch.log", format=FORMAT, level=logging.DEBUG)
logger = logging.getLogger(__name__)
print("**********Start**********")
config = parseconfig.parse_from_yaml(CONFIG_PATH)
parseconfig.print_input_config(config)
from binarypatcher import binarypatcher
print("\n**********Init binarypatcher**********\n")
basic_offset = 0x200000
turns = 0
overlapping_cb = []
while True:
    if turns > 0:
        print("\n**********Reinit binarypatcher due to overlapping**********\n")
    bp = binarypatcher(config["objdir"], config["objdump_path"], config["cc_path"], config["binary_name"], config["source_ISA_list"], False)
    bp.update_cb_list(overlapping_cb)
    jumpoffset = hex_add(config["jumpoffset"], hex(turns * basic_offset), 16)
    bp.init_offset(jumpoffset)
    bp.init_trampoline_vaddr()
    use_gp_cb_list = bp.init_unc_obj_list(config["objdir"]+"/"+config["use_gp_data_name"]+"-"+str(turns)+".txt")
    bp.process_use_gp_cbs(use_gp_cb_list, turns)
    print("\n**********Generate objects stage1**********\n")
    bp.generate_objs_stage1(cc_option=config["cc_option"])
    print("\n**********Generate objects stage2**********\n")
    bp.generate_objs_stage2(cc_option=config["cc_option"])
    bp.get_final_codeblock(config["translate_objname"] + '-' + str(turns))
    info_to_binarytools = bp.get_pass_info("tmp/" + config["pass_name"] + '-' + str(turns) + '.json')
    print("\n**********Print binary patching info**********\n")
    print(info_to_binarytools)
    overlapping_cb = bp.overlapping_cb
    logger.info("Length of overlapping_cb: {}".format(len(overlapping_cb)))
    turns += 1
    if len(overlapping_cb) == 0:
        break
original_gp_list, tmp_gp_list = None, None
for turn in range(turns):
    f = open(config["objdir"]+"/"+config["use_gp_data_name"]+"-"+str(turn)+".txt", 'r')
    lines = f.readlines()
    for i in range(len(lines)):
        lines[i] = lines[i][len("Codeblock startaddr: "):].strip()
    if original_gp_list == None:
        original_gp_list = lines
    else:
        tmp_gp_list = lines
    f.close()
    if tmp_gp_list != None:
        for addr in tmp_gp_list:
            if addr in original_gp_list:
                original_gp_list.remove(addr)
for addr in original_gp_list:
    print("The address of GP data is: " + addr)
