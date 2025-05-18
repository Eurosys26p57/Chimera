from ext_tran import parseconfig
import os
import sys
import glob
import json
import yaml
import textcorrect
def get_donotpatch():
    try:
        with open("notpatch.yaml", 'r', encoding='utf-8') as file:
            data = yaml.safe_load(file)
            if data["addr"]:
                return data["addr"]
    except yaml.YAMLError as e:
        print(f"错误: 解析 YAML 文件时出错: {e}")
    return []
reg_dict = {
    'zero': '0x0',
    'ra': '0x1',
    'sp': '0x2',
    'gp': '0x3',
    'tp': '0x4',
    't0': '0x5',
    't1': '0x6',
    't2': '0x7',
    's0': '0x8',
    's1': '0x9',
    'a0': '0xa',
    'a1': '0xb',
    'a2': '0xc',
    'a3': '0xd',
    'a4': '0xe',
    'a5': '0xf',
    'a6': '0x10',
    'a7': '0x11',
    's2': '0x12',
    's3': '0x13',
    's4': '0x14',
    's5': '0x15',
    's6': '0x16',
    's7': '0x17',
    's8': '0x18',
    's9': '0x19',
    's10': '0x1a',
    's11': '0x1b',
    't3': '0x1c',
    't4': '0x1d',
    't5': '0x1e',
    't6': '0x1f',
}
if __name__ == '__main__':
    config_yaml_dir = sys.argv[1]
    config = parseconfig.parse_from_yaml(config_yaml_dir)
    os.system('cd ext_tran; python3 main.py')
    translate_objfiles = glob.glob(config['objdir']+'/'+config['translate_objname']+"-*")
    pass_files = glob.glob(config['objdir']+'/'+config['pass_name']+"-*.json")
    f = open("binarytools/sectioncontents", "w+")
    for i in range(len(pass_files)):
        translate_objfile = translate_objfiles[i]
        os.system("cp {} {}".format(translate_objfile, "binarytools/"))
        f.write(translate_objfile.split('/')[-1]+"\n")
    f.close()
    print("cd binarytools; python3 sectioncopy/addtest.py {} {} {}".format(config['binary_name'], "sectioncontents", config['target_objname']))
    os.system("cd binarytools; python3 sectioncopy/addtest.py {} {} {}".format(config['binary_name'], "sectioncontents", config['target_objname']))
    original_text = textcorrect.get_text_segment_address(config['binary_name'])
    print("original:", original_text)
    new_text = textcorrect.get_text_segment_address("binarytools/" + config['target_objname'])
    print("new:", new_text)
    additional_offset = hex(int(new_text, 16) - int(original_text, 16))
    print("offset:", additional_offset)
    for translate_objfile in translate_objfiles:
        fname = "binarytools/" + translate_objfile.split('/')[-1]
        textcorrect.shift_file_content(fname, int(additional_offset, 16))
    os.system("cd binarytools; python3 sectioncopy/addtest.py {} {} {}".format(config['binary_name'], "sectioncontents", config['target_objname']))
    for i in range(len(pass_files)):
        translate_objfile = translate_objfiles[i]
        print("OBJFILE: {}".format(translate_objfile))
        pass_file = pass_files[i]
        f = open(pass_file, 'r')
        testobj_pass = json.load(f)
        f.close()
        os.system("cp {} {}".format(translate_objfile, "binarytools/"))
        print("OFFSET: {}".format(testobj_pass['jumpoffset']))
        print('cd binarytools; ./patch.sh {} {} {} {}'.format(config['target_objname'], translate_objfile.split('/')[-1], testobj_pass['jumpoffset'], config['target_objname']))
        os.system('cd binarytools; ./patch.sh {} {} {} {}'.format(config['target_objname'], translate_objfile.split('/')[-1], testobj_pass['jumpoffset'], config['target_objname']))
        donotpatch = get_donotpatch()
        for instr_addr, reg, trampoline_type in testobj_pass['trampoline_addr']:
            instr_addr = hex(int(instr_addr, 16) + int(additional_offset, 16))
            if instr_addr in donotpatch:
                print(instr_addr)
                continue
            os.system('cd binarytools; trampolineinst/patchinst {} {} {} {} {}'.format(config['target_objname'], trampoline_type, reg_dict[reg[0]], testobj_pass['jumpoffset'], instr_addr))
        print('Patchobj ' + str(pass_file) + ' done.')
    print('All is done.')
