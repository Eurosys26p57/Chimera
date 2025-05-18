#include "lib_elf.h"

int main(unsigned int argc, char **argv){
    
    char* file = argv[1];
    char code[8] = {0,}; 
    size_t reg_num; 
    size_t auipc_offset; 
    size_t jal_offset; 
    size_t instr_vaddr; 
    
    
    
    
    
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

    
    switch (option) {
        case 1: {
            *(short*)code = 0x0001; 
            *(int*)(code + 2) = 0x17 + ((reg_num & 0x1f) << 7) + (auipc_offset & 0xfffff000); 
            *(short*)(code + 6) = 0x02 + ((reg_num & 0x1f) << 7) + (0x0 << 12) + (0x4 << 13); 
            int write_code = writeElf(&fd, code, instr_faddr, sizeof(code)); 
            printf("%d\n", write_code);
            break;
        }
        case 2: {
            *(int*)code = 0x17 + ((reg_num & 0x1f) << 7) + (auipc_offset & 0xfffff000); 
            *(int*)(code + 4) = 0x02 + ((reg_num & 0x1f) << 7) + (0x0 << 12) + (0x4 << 13); 
            int write_code = writeElf(&fd, code, instr_faddr, sizeof(code)); 
            printf("%d\n", write_code);
            break;
        }
        case 3: {
            *(int*)code =  0x67 + ((jal_offset & 0x100000) << 11) + ((jal_offset & 0x7fe) << 20) + \
                            ((jal_offset & 0x800) << 9) + (jal_offset & 0xff000) + \
                            ((reg_num & 0x1f) << 7); 
            int write_code = writeElf(&fd, code, instr_faddr, 4); 
            printf("%d\n", write_code);
            break;
        }
    }

    

    close(fd);
    return 0;
}
