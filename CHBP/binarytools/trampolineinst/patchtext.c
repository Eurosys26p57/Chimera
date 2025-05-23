#include "lib_elf.h"




int get_text_id(int *target_file_fd){
    for(int i = 0; i < section_header_table_length; i++){
        if(strlen(section_string_table[i]) > 0 && 
                strcmp(section_string_table[i], ".text") == 0)
            return i;
    }
    return -1;
};

void *extract_text(int *target_file_fd, int target_file_text_idx){
    void *target_text = extract_section(target_file_fd, section_header_table[target_file_text_idx].sh_size, \
            section_header_table[target_file_text_idx].sh_offset);
    return target_text;
};

int main(unsigned int argc, char **argv){
    
    if(argc != 7){ 
        printf("Usage: trampoline/patchtext original_file patch_vaddr target_file offset trampoline_type reg_num\n");
        printf("Attention: please execute it in `binarytools` directory.\n");
        return 0;
    }

    char* original_file = argv[1]; 
    size_t patch_vaddr = char2hex(argv[2]); 
    char* target_file = argv[3]; 
    size_t offset = char2hex(argv[4]); 
    unsigned int trampoline_type = argv[5][0] - '0'; 
    size_t reg_num = char2hex(argv[6]); 
    char code[8] = {0,}; 
    int original_file_text_idx, target_file_text_idx;
    int original_file_text_size, target_file_text_size;
    void* target_text; 

    
    if(strlen(argv[5]) != 1 || trampoline_type > 3){ 
        printf("Invalid option.\n");
        return 0;
    }

    
    int target_file_fd = open(target_file, O_RDWR);
    analyze_header(&target_file_fd);
    target_file_text_idx = get_text_id(&target_file_fd);
    target_text = extract_text(&target_file_fd, target_file_text_idx);
    target_file_text_size = section_header_table[target_file_text_idx].sh_size;
    close(target_file_fd);
    printf("finish one.\n");

    
    int original_file_fd = open(original_file, O_RDWR);
    analyze_header(&original_file_fd);
    original_file_text_idx = get_text_id(&original_file_fd);
    original_file_text_size = section_header_table[original_file_text_idx].sh_size;
    void *tmp = malloc(sizeof(char) * original_file_text_size);
    memset(tmp, 0, original_file_text_size);
    memcpy(tmp, target_text, target_file_text_size);
    free(target_text); 
    target_text = tmp;
    printf("finish two.\n");

    
    size_t patch_faddr = section_header_table[original_file_text_idx].sh_offset + \
    (patch_vaddr - section_header_table[original_file_text_idx].sh_addr);
    printf("patch_faddr: 0x%x\n", patch_faddr);
    
    switch (trampoline_type) {
        case 1: {
            *(short*)code = 0x0001; 
            *(int*)(code + 2) = 0x17 + ((reg_num & 0x1f) << 7) + (offset & 0xfffff000); 
            *(short*)(code + 6) = 0x02 + ((reg_num & 0x1f) << 7) + (0x0 << 12) + (0x4 << 13); 
            int write_code = writeElf(&original_file_fd, code, patch_faddr, sizeof(code)); 
            printf("%d\n", write_code);
            break;
        }
        case 2: {
            *(int*)code = 0x17 + ((reg_num & 0x1f) << 7) + (offset & 0xfffff000); 
            *(int*)(code + 4) = 0x02 + ((reg_num & 0x1f) << 7) + (0x0 << 12) + (0x4 << 13); 
            int write_code = writeElf(&original_file_fd, code, patch_faddr, sizeof(code)); 
            printf("%d\n", write_code);
            break;
        }
    }
    close(original_file_fd);
    printf("finish three.\n");

    
    FILE *fp = fopen("tmpobj", "wb+");
    fwrite(target_text, 1, original_file_text_size, fp);
    fclose(fp);
    printf("finish four.\n");

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "./patch.sh %s tmpobj %s final", original_file, argv[4]);
    printf("cmd: %s\n", cmd);
    int result = system(cmd);
    printf("%d\n", result);
    
    close(original_file_fd);
    printf("finish five.\n");
}