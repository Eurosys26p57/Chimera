#!/bin/bash

# 创建结果目录
mkdir -p result

# 输出文件
OUTPUT_FILE="./result/extres.txt"

# 清空输出文件
echo "Experiment Results" > $OUTPUT_FILE
echo "==================" >> $OUTPUT_FILE
echo "" >> $OUTPUT_FILE

# 实验参数
TOTAL_TASKS=1000
N=2048
FIB_N=10000

echo "Starting experiments..."
echo "Total tasks per run: $TOTAL_TASKS"
echo "Matrix size: $N x $N"
echo "Fibonacci number: $FIB_N"

# 运行三个实验
    
for RATIO in 0 10 20 30 40 50 60 70 80 90 100; do
    RATIO_FLOAT=$(echo "$RATIO / 100" | bc)
    echo "  Ratio: $RATIO% "
    echo " FAM: "
    bins/matrix_ext $EXP_ID $TOTAL_TASKS $RATIO_FLOAT 0 >> $OUTPUT_FILE 2>&1        
    echo " MELF: "
    bins/matrix_ext $EXP_ID $TOTAL_TASKS $RATIO_FLOAT 1 >> $OUTPUT_FILE 2>&1        
    echo " Safer: "
    bins/matrix_ext_rewritten $EXP_ID $TOTAL_TASKS $RATIO_FLOAT 1 >> $OUTPUT_FILE 2>&1        
    echo " Chimera: "
    bins/matrix_ext_rewritten2 $EXP_ID $TOTAL_TASKS $RATIO_FLOAT 1 >> $OUTPUT_FILE 2>&1        
done

echo "All experiments completed! Results saved to $OUTPUT_FILE"

echo "Summary generated in $OUTPUT_FILE"
