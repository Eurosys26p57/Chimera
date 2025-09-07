import lief
import sys

def copy_text_section_to_end(elf_path, sectioncontent, output_path):
    binary2 = lief.parse(sectioncontent)
    aimtextse = binary2.get_section(".text")
    binary = lief.parse(elf_path)

    text_section = binary.get_section(".text")
    print(binary.header.machine_type)

    if text_section is None:
        print("can not find .text")
        return

    new_text_section = lief.ELF.Section(".textcopy")
    new_text_section.size = text_section.size
    new_text_section.content = aimtextse.content

    new_text_section.type = lief.ELF.Section.TYPE.PROGBITS
    new_text_section.flags = lief.ELF.Section.FLAGS.ALLOC | lief.ELF.Section.FLAGS.EXECINSTR
    #load_address = binary.header.entrypoint + 0x8000
    load_address = 0x120000

    binary.add(new_text_section)
    new_text_section.virtual_address = 0x120000
    print(new_text_section.virtual_address)
    print(f"new ELF binary: {output_path}")
    # 更新程序头
    new_segment = lief.ELF.Segment()
    new_segment.type=lief.ELF.Segment.TYPE.LOAD
    new_segment.add(lief.ELF.Segment.FLAGS.X)
    new_segment.add(lief.ELF.Segment.FLAGS.R)
    new_segment.content = new_text_section.content
#    new_segment.add_section(new_text_section)

    # 设置新的段
    new_segment.virtual_address = load_address
    new_segment.physical_address = load_address
    new_segment.physical_size = text_section.size
    #new_segment.memory_size = new_text_section.size
#    new_segment.add_section(new_text_section)
#    binary.add(new_segment)
    print(load_address)
    print(new_segment.virtual_address)
    binary.write(output_path)

# 使用示例
if len(sys.argv) < 2:
    print("Usage: addvma.py Binary SectionContent.o outputbinary")
    exit()
input_elf_path = sys.argv[1]
sectioncontent = sys.argv[2]
output_elf_path = sys.argv[3]
copy_text_section_to_end(input_elf_path, sectioncontent, output_elf_path)
#binary = lief.parse(output_elf_path)
#for x in binary.segments:
#    print("0x{:x}".format(x.virtual_address))
