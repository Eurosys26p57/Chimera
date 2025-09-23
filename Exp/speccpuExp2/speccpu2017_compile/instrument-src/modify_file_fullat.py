import sys
import os

def insert_code_before_indirect_jump(file_path,code_to_insert):
    with open(file_path,'r') as f:
        lines=f.readlines()

    modified_lines=[]
    for line in lines:
        if 'jalr' in line or 'jr' in line:
            modified_lines.append(code_to_insert)
        modified_lines.append(line)

    with open(file_path,'w') as f:
        f.writelines(modified_lines)

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 modify_s_file.py <target_file.s> <text_size>")
        sys.exit(1)

    file_path=sys.argv[1]
    script_path = os.path.abspath(__file__) 
    script_dir = os.path.dirname(script_path)
    with open(f'{script_dir}/cost-fullat.s', 'r', encoding='utf-8') as file:
        code_to_insert = file.read()

    insert_code_before_indirect_jump(file_path, code_to_insert)

if __name__=='__main__':
    main()

