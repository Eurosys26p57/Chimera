#!/bin/bash

# 检查传递的参数个数
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <ELF-file> <SECTION-NAME>"
    exit 1
fi

ELF_FILE=$1
SECTION_NAME=$2

# 使用 readelf 提取段信息，并通过 awk 查找 .textcopy 段的文件偏移量
#OFFSET=$(readelf -S "$ELF_FILE" | awk '
#    /^ *\[.*\] +\.textcopy/ {
#        getline # 读取并处理下一行
#        print $1
#    }'
#)
OFFSET=$(readelf -S "$ELF_FILE" | awk -v section_name=".$SECTION_NAME" '
    /^ *\[.*\] +/ {
        if ($2 == section_name) {
            getline
            print $1
        }
    }'
)

# 检查是否找到偏移量
if [ -z "$OFFSET" ]; then
    echo "$2 section not found in $ELF_FILE"
else
    echo "$OFFSET"
fi
