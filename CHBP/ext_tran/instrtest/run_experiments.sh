#!/bin/bash
output_file="experiment_results.txt"


> "$output_file"


for ratio in $(seq 0 0.1 1 | awk '{printf "%.1f\n", $1}')
do
    echo "ÕýÔÚ²âÊÔ ratio = $ratio ..."
    echo "==================== Ratio $ratio ====================" >> "$output_file"
    

    timeout 60 ./matrix_test2 1000 "$ratio" 409600 >> "$output_file" 2>&1
    

    echo -e "\n\n" >> "$output_file"
    

    echo "ratio $ratio test done"
done

echo "test all done and save to $output_file"