#!/bin/bash

# 检查命令行参数
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <ELF-file> AIMADDR <SECTION-NAME>"
    exit 1
fi

ELF_FILE=$1

# 使用 readelf -S 提取 .textcopy 段的文件偏移量
#TEXTCOPY_OFFSET=$(readelf -S "$ELF_FILE" | awk '/^ *\[.*\] +\.textcopy/ {print "0x"$5}')
SECTION_NAME="$3"

# 使用 readelf 提取段信息，并通过 awk 查找指定段的文件偏移量
TEXTCOPY_OFFSET=$(readelf -S "$ELF_FILE" | awk -v section_name=".$SECTION_NAME" '
    /^ *\[.*\] +/ {
        if ($2 == section_name) {
            print "0x"$5
        }
    }'
)
# 检查是否找到偏移量
if [ -z "$TEXTCOPY_OFFSET" ]; then
    echo "$SECTION_NAME section not found in $ELF_FILE"
    exit 1
fi

echo "The file offset for .textcopy section is: $TEXTCOPY_OFFSET"

# 使用程序头信息，通过偏移量找到对应的虚拟地址
VADDR=$(readelf -l "$ELF_FILE" | awk -v offset="$TEXTCOPY_OFFSET" '
    /LOAD/ {
        if (strtonum(offset) == strtonum($2)) {
            print $3
            exit
        }
    }
')

# 回显找到的虚拟地址
if [ -z "$VADDR" ]; then
    echo "No virtual address found for .textcopy section with offset $TEXTCOPY_OFFSET"
else
    echo "The virtual address for .textcopy section is: $VADDR"
fi
elfdiet/testphad $ELF_FILE $VADDR $2
