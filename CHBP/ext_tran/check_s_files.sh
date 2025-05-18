


for file in tmp/*.s
do
  
  last_line=$(tail -n 1 "$file")
  
  
  if [[ "$last_line" == *"gp"* ]]; then
    
    total_lines=$(wc -l < "$file")
    echo "文件: $file, 行数: $total_lines"
  fi
done
