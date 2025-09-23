#!/usr/bin/env python3
import re
import csv
import os

def parse_output_file(input_file):
    """
    解析输出文件，提取不同binary和ratio的性能数据
    """
    data = {}
    
    current_binary = None
    current_ratio = None
    
    ratio_pattern = re.compile(r'Ratio: (\d+)%')
    binary_patterns = {
        'FAM': re.compile(r'FAM:'),
        'MELF': re.compile(r'MELF:'),
        'Safer': re.compile(r'Safer:'),
        'Chimera': re.compile(r'Chimera:')
    }
    
    metric_patterns = {
        'fib_time': re.compile(r'fib time:\s*([\d.]+)'),
        'rvv_matrix_time': re.compile(r'rvv matrix_mul time:\s*([\d.]+)'),
        'scalar_matrix_time': re.compile(r'scalar matrix_mul time:\s*([\d.]+)'),
        'total_time': re.compile(r'total:\s*([\d.]+)')
    }
    
    with open(input_file, 'r') as f:
        lines = f.readlines()
    
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        
        # 检查是否是ratio行
        ratio_match = ratio_pattern.search(line)
        if ratio_match:
            current_ratio = int(ratio_match.group(1))
            i += 1
            continue
        
        # 检查是否是binary标识行
        for binary_name, pattern in binary_patterns.items():
            if pattern.search(line):
                current_binary = binary_name
                
                # 初始化数据结构
                if current_binary not in data:
                    data[current_binary] = {}
                if current_ratio not in data[current_binary]:
                    data[current_binary][current_ratio] = {}
                
                # 读取接下来的4行数据
                for j in range(1, 5):
                    if i + j < len(lines):
                        metric_line = lines[i + j].strip()
                        
                        # 匹配各种指标
                        for metric_name, metric_pattern in metric_patterns.items():
                            metric_match = metric_pattern.search(metric_line)
                            if metric_match:
                                data[current_binary][current_ratio][metric_name] = float(metric_match.group(1))
                                break
                
                i += 4  # 跳过已经处理的4行指标数据
                break
        
        i += 1
    
    return data

def create_csv_files(data, output_dir='csv_results'):
    """
    为每个binary创建CSV文件
    """
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    # 定义指标顺序
    metrics = ['fib_time', 'rvv_matrix_time', 'scalar_matrix_time', 'total_time']
    metric_titles = ['Fib_Time(ms)', 'RVV_Matrix_Time(ms)', 'Scalar_Matrix_Time(ms)', 'Total_Time(ms)']
    
    # 为每个binary创建CSV文件
    for binary_name, ratio_data in data.items():
        csv_filename = os.path.join(output_dir, f'{binary_name.lower()}_performance.csv')
        
        with open(csv_filename, 'w', newline='') as csvfile:
            writer = csv.writer(csvfile)
            
            # 写入标题行
            header = ['Ratio(%)'] + metric_titles
            writer.writerow(header)
            
            # 按ratio排序并写入数据
            ratios = sorted(ratio_data.keys())
            for ratio in ratios:
                row = [ratio]
                for metric in metrics:
                    value = ratio_data[ratio].get(metric, 'N/A')
                    row.append(value)
                writer.writerow(row)
        
        print(f"Created: {csv_filename}")

def create_summary_csv(data, output_dir='csv_results'):
    """
    创建一个汇总的CSV文件，包含所有binary的数据
    """
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    metrics = ['fib_time', 'rvv_matrix_time', 'scalar_matrix_time', 'total_time']
    metric_titles = ['Fib_Time(ms)', 'RVV_Matrix_Time(ms)', 'Scalar_Matrix_Time(ms)', 'Total_Time(ms)']
    
    csv_filename = os.path.join(output_dir, 'all_performance_summary.csv')
    
    with open(csv_filename, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        
        # 写入标题行
        header = ['Binary', 'Ratio(%)'] + metric_titles
        writer.writerow(header)
        
        # 写入所有数据
        binaries = sorted(data.keys())
        ratios = sorted(list(data[binaries[0]].keys())) if binaries else []
        
        for binary in binaries:
            for ratio in ratios:
                if ratio in data[binary]:
                    row = [binary, ratio]
                    for metric in metrics:
                        value = data[binary][ratio].get(metric, 'N/A')
                        row.append(value)
                    writer.writerow(row)
    

def main():
    input_file = ["./result/baseres.txt", "./result/extres.txt"]
    output_dir = ["./result/basecsv", "./result/extcsv"]   
    for i in range(len(input_file)):
        data = parse_output_file(input_file[i])
        create_csv_files(data, output_dir[i])
        create_summary_csv(data, output_dir[i])
    

if __name__ == "__main__":
    main()
