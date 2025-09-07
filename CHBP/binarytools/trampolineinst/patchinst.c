#include "lib_elf.h"

int main(unsigned int argc, char **argv){
    /*
    Usage:
    ./patchinst reg_num auipc_offset instr_vaddr
    */
    char* file = argv[1];
    char code[8] = {0,}; //指令的编码
    size_t reg_num; //寄存器编号，以十六进制格式输入
    size_t auipc_offset; //aupic指令使用的立即数，共32位，以十六进制格式输入
    size_t jal_offset; //jal指令使用的立即数，共32位，以十六进制格式输入
    size_t instr_vaddr; //想把指令patch到哪个地址（虚拟地址），共20位，以十六进制格式输入
    
    // 选项，是一个数，根据数字大小选择patch的类型。
    // 1：c.nop + aupic + c.jr  长度为c.nop(2)+aupic(4)+c.j(2)=8，对应4+4类型
    // 2：auipc + c.jr  长度为aupic(4)+c.j(2)+填充(2)=8，对应2+4类型
    // 3：jal 长度为jal=4
    unsigned int option = argv[2][0] - '0';
    
    if(strlen(argv[2]) != 1 || option > 3){
        printf("Invalid option.\n");
        return 0;
    }
    if(option <= 2){
        reg_num = char2hex(argv[3]);
        auipc_offset = char2hex(argv[4]);
        instr_vaddr = char2hex(argv[5]);
    }
    else if(option == 3){
        reg_num = char2hex(argv[3]);
        jal_offset = char2hex(argv[4]);
        instr_vaddr = char2hex(argv[5]);
    }
    
    int plt_idx, text_idx;
    //打开文件，提取出plt节和text节在节表中的索引
    int fd = open(file, O_RDWR); 

    analyze_header(&fd);
    for(int i = 0; i < section_header_table_length; i++){
        if(strlen(section_string_table[i]) > 0 && 
           strcmp(section_string_table[i], ".plt") == 0)
            plt_idx = i;
        else if(strlen(section_string_table[i]) > 0 && 
                strcmp(section_string_table[i], ".text") == 0)
            text_idx = i;
    }
    size_t instr_faddr = section_header_table[text_idx].sh_offset + \
                        (instr_vaddr - section_header_table[text_idx].sh_addr);

    //printf("reg_num=0x%x aupic_offset=0x%x\n", reg_num, auipc_offset);
    switch (option) {
        case 1: {
            *(short*)code = 0x0001; //nop指令
            *(int*)(code + 2) = 0x17 + ((reg_num & 0x1f) << 7) + (auipc_offset & 0xfffff000); //aupic指令
            *(short*)(code + 6) = 0x02 + ((reg_num & 0x1f) << 7) + (0x0 << 12) + (0x4 << 13); //c.jr指令
            int write_code = writeElf(&fd, code, instr_faddr, sizeof(code)); //写到ELF文件里
            printf("%d\n", write_code);
            break;
        }
        case 2: {
            *(int*)code = 0x17 + ((reg_num & 0x1f) << 7) + (auipc_offset & 0xfffff000); //aupic指令
            *(int*)(code + 4) = 0x02 + ((reg_num & 0x1f) << 7) + (0x0 << 12) + (0x4 << 13); //c.jr指令
            int write_code = writeElf(&fd, code, instr_faddr, sizeof(code)); //写到ELF文件里
            printf("%d\n", write_code);
            break;
        }
        case 3: {
            *(int*)code =  0x67 + ((jal_offset & 0x100000) << 11) + ((jal_offset & 0x7fe) << 20) + \
                            ((jal_offset & 0x800) << 9) + (jal_offset & 0xff000) + \
                            ((reg_num & 0x1f) << 7); //jar指令
            int write_code = writeElf(&fd, code, instr_faddr, 4); //写到ELF文件里
            printf("%d\n", write_code);
            break;
        }
    }

    /*for(int i = 0; i < sizeof(code); i++){
        printf("%02x ", code[i]);
    }*/

    close(fd);
    return 0;
}
