# 整体工作的脚本。工作流程：
#1、调用ext_tran/main.py。
#2、调用patch.sh。
#3、读取每一轮的tmp下的testobj_pass-x.yaml文件。
#4、调用patchinst给obj加上跳板并写好偏移量。
from ext_tran import parseconfig
import os
import sys
import glob
import json

#！！！！！！！！！！！！！！！！！！！！！！！！！
#！！！！！！！！！！！！！！！！！！！！！！！！！
#！！！！！！！！！！！！！！！！！！！！！！！！！
#！！！！！！！！！！！！！！！！！！！！！！！！！
# 注意这个寄存器名-代号的映射关系，请记得检查一下。
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
    # 1、调用ext_tran/main.py。
    #os.system('cd ext_tran; python3 main.py')

    translate_objfiles = glob.glob(config['objdir']+'/'+config['translate_objname']+"-*")
    pass_files = glob.glob(config['objdir']+'/'+config['pass_name']+"-*.json")

    # 2、调用binarytools/sectioncopy/addtest.py
    f = open("binarytools/sectioncontents", "w+")
    for i in range(len(pass_files)):
        translate_objfile = translate_objfiles[i]
        os.system("mv {} {}".format(translate_objfile, "binarytools/"))
        f.write(translate_objfile.split('/')[-1]+"\n")
    f.close()

    os.system("cd binarytools; python3 sectioncopy/addtest.py {} {} {}".format(config['binary_name'], "sectioncontents", config['target_objname']))

    for i in range(len(pass_files)-1, -1, -1):
    #for i in range(len(pass_files)):
        translate_objfile = translate_objfiles[i]
        print("OBJFILE: {}".format(translate_objfile))
        pass_file = pass_files[i]
        # 3、读取每一轮的tmp下的testobj_pass-x.yaml文件。
        f = open(pass_file, 'r')
        testobj_pass = json.load(f)
        f.close()
        
        # 4、调用patch.sh。
        # 很愚蠢，patch.sh调用的python脚本必须要把带翻译好指令的文件挪到当前工作目录下才能正常工作。
        os.system("mv {} {}".format(translate_objfile, "binarytools/"))
        print("OFFSET: {}".format(testobj_pass['jumpoffset']))
        os.system('cd binarytools; ./patch.sh {} {} {} {}'.format(config['target_objname'], translate_objfile.split('/')[-1], testobj_pass['jumpoffset'], config['target_objname']))
        
        # 5、调用patchinst给obj加上跳板并写好偏移量。
        for instr_addr, reg, trampoline_type in testobj_pass['trampoline_addr']:
            print("REG: {} {}".format(reg, reg_dict[reg[0]]))
            print("OFFSET2: {}".format(testobj_pass['jumpoffset']))
            os.system('cd binarytools; trampolineinst/patchinst {} {} {} {} {}'.format(config['target_objname'], trampoline_type, reg_dict[reg[0]], testobj_pass['jumpoffset'], instr_addr))
        print('Patchobj ' + str(pass_file) + ' done.')
    
    print('All is done.')
