#!/bin/bash

# 检查是否提供了参数
if [ $# -eq 0 ]; then
    echo "Usage: $0 <json_file>"
    exit 1
fi

JSON_FILE=$1

# 检查文件是否存在
if [ ! -f "$JSON_FILE" ]; then
    echo "Error: File $JSON_FILE does not exist"
    exit 1
fi

# 解析 JSON 文件
JUMPOFFSET=$(jq -r '.jumpoffset' "$JSON_FILE")
PATCH_START=$(jq -r '.patch_instr_vaddr_start' "$JSON_FILE")
TRAMPOLINE_ADDRS=$(jq -c '.trampoline_addr[]' "$JSON_FILE")

# 检查 jq 命令是否成功执行
if [ $? -ne 0 ]; then
    echo "Error: Failed to parse JSON file"
    exit 1
fi

# 准备写入 /proc/errorhandling 的数据
DATA=""
while IFS= read -r trampoline; do
    # 提取 trampoline 地址
    TRAMPOLINE_ADDR=$(echo "$trampoline" | jq -r '.[0]')
    
    # 计算 value (trampoline_addr + patch_instr_vaddr_start)
    TRAMPOLINE_DEC=$((TRAMPOLINE_ADDR))
    PATCH_DEC=$((PATCH_START))
    VALUE_DEC=$((TRAMPOLINE_DEC + PATCH_DEC))
    VALUE=$(printf "0x%x" $VALUE_DEC)
    
    # 添加到数据中
    DATA+="$TRAMPOLINE_ADDR $VALUE\n"
done <<< "$TRAMPOLINE_ADDRS"

# 写入 /proc/errorhandling
echo -e "$DATA" | sudo tee /proc/errorhandling > /dev/null

if [ $? -eq 0 ]; then
    echo "Successfully written to /proc/errorhandling:"
    echo -e "$DATA"
else
    echo "Error: Failed to write to /proc/errorhandling"
    exit 1
fi
