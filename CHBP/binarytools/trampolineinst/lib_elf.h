#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <elf.h>
#include <fcntl.h>

#define MAX_SYNBOL_LENGTH 256 //暂时设定符号的最大长度为256个字节（没有查到符号最长可以多长）

typedef uint8_t byte;

Elf64_Shdr *section_header_table; //段表
Elf64_Sym *symbol_table; //符号表
char* string_table; //字符串表 .strtab，由于无法完全确定有多少个符号（因为所有的符号不全在.symtab里），因此这里保存元数据
char** section_string_table; //段名表 .shstrtab

//以上四个表的长度
unsigned int section_header_table_length;
unsigned int symbol_table_length;
unsigned int string_table_length;
unsigned int section_string_table_length;

void analyze_header(int *fd);
void *extract_section(int *fd, unsigned int size, unsigned int offset);
int disasm(byte* buf, unsigned int size, Elf64_Addr addr);
int length_recognize(byte* code);
size_t char2hex(char *str);
int writeElf(int *fd, void* code, size_t offset, int size);

void analyze_header(int *fd){
	Elf64_Ehdr ehdr_t;
	if(read(*fd, &ehdr_t, sizeof(ehdr_t)) != sizeof(ehdr_t)){
		printf("Failed to read ELF header.\n");
		close(*fd);
		exit(1);
	}

	section_header_table_length = ehdr_t.e_shnum;
	section_header_table = malloc(section_header_table_length*sizeof(Elf64_Shdr));
	//读段表
	lseek(*fd, ehdr_t.e_shoff, SEEK_SET);
	if(read(*fd, section_header_table, ehdr_t.e_shnum*sizeof(Elf64_Shdr)) != \
			ehdr_t.e_shnum*sizeof(Elf64_Ehdr)){
		printf("Failed to read section header table.\n");
		free(section_header_table);
		close(*fd);
		exit(1);
	}
	//遍历段表，把符号表找出来，并读段名表和字符串表
	//注意，段名表的第一项是\0！
	int cnt;
	section_string_table_length = ehdr_t.e_shnum;
	section_string_table = malloc(sizeof(char*)*section_string_table_length);
	char tmp[MAX_SYNBOL_LENGTH]; 
	for(cnt = 0; cnt < section_header_table_length; cnt++){
		Elf64_Shdr *shdr = &section_header_table[cnt];
		Elf64_Addr str_addr = section_header_table[ehdr_t.e_shstrndx].sh_offset + shdr->sh_name;
		lseek(*fd, str_addr, SEEK_SET);
		read(*fd, tmp, MAX_SYNBOL_LENGTH);
		unsigned int length = strlen(tmp);
		//printf("length[%d]: %d\n", cnt, length);
		section_string_table[cnt] = malloc(length+1);
		strcpy(section_string_table[cnt], tmp); //依次将段名字符串复制到段名表里
	
		if(tmp[0] != 0 && strlen(tmp) > 0 && strcmp(tmp, ".symtab") == 0){
			//发现了符号表
			symbol_table = extract_section(fd, section_header_table[cnt].sh_size, \
							section_header_table[cnt].sh_offset);
			symbol_table_length = section_header_table[cnt].sh_size / sizeof(Elf64_Sym);
		}
		else if(tmp[0] != 0 && strlen(tmp) > 0 && strcmp(tmp, ".strtab") == 0){
			//发现了字符串表
			string_table = extract_section(fd, section_header_table[cnt].sh_size, \
							section_header_table[cnt].sh_offset);
			string_table_length = section_header_table[cnt].sh_size;
		}
	}
}

void *extract_section(int *fd, unsigned int size, unsigned int offset){
	//抽取指定段的内容
	void *section_pt = malloc(size);
	lseek(*fd, offset, SEEK_SET);
	if(read(*fd, section_pt, size) != size){
		printf("Failed to extract the specified section.\n");
		free(section_pt);
	}
	return section_pt;
}


size_t char2hex(char *str){
	int len = strlen(str);
	int end = 0;
	unsigned ans = 0, factor = 1;
	if(str[0]=='0'&&str[1]=='x')end = 2;
	for(int i = len-1; i >= end; i--){
        if(str[i] >= '0' && str[i] <= '9')
		    ans += (str[i] - '0')*factor;
        else if(str[i] >= 'a' && str[i] <= 'z')
            ans += (str[i] - 'a' + 10)*factor;
        else if(str[i] >= 'A' && str[i] <= 'Z')
            ans += (str[i] - 'A' + 10)*factor;
		factor *= 16;
	}
	return ans;
}

int writeElf(int *fd, void* code, size_t offset, int size){
	lseek(*fd, offset, SEEK_SET);
	int count = write(*fd, code, size);
	return count;
}