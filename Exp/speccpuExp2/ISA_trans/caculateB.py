#!/usr/bin/env python3

def parse_res_file(filename):
    """解析res.txt文件，提取各个配置的运行时间"""
    data = {}
    current_section = None
    
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
                
            if line.endswith(':'):
                # 新的配置节
                current_section = line[:-1]  # 去掉冒号
                data[current_section] = {}
            elif current_section and ':' in line:
                # 解析基准测试数据
                parts = line.split(':')
                if len(parts) == 2:
                    benchmark = parts[0].strip()
                    # 去掉数字前缀，只保留基准测试名称
                    if '.' in benchmark:
                        benchmark = benchmark.split('.', 1)[1]
                    time_value = float(parts[1].strip())
                    data[current_section][benchmark] = time_value
    
    return data

def calculate_ratios(data):
    """计算各个配置相对于base的比值"""
    ratios = {}
    base_data = data.get('base', {})
    
    # 需要计算的配置列表
    configs = ['armore', 'chimera', 'text1mb', 'fullat']
    
    for config in configs:
        if config in data:
            ratios[config] = {}
            for benchmark in base_data:
                if benchmark in data[config]:
                    base_time = base_data[benchmark]
                    config_time = data[config][benchmark]
                    ratios[config][benchmark] = config_time / base_time
    
    return ratios

def update_base_log(base_log_file, ratios, output_file):
    """更新base.log文件中的比值"""
    # 配置顺序（与base.log中的列对应）
    config_order = ['armore', 'chimera', 'text1mb', 'fullat']
    
    with open(base_log_file, 'r') as f_in, open(output_file, 'w') as f_out:
        for line in f_in:
            line = line.strip()
            if not line:
                f_out.write('\n')
                continue
                
            parts = line.split()
            if len(parts) >= 5:  # 至少有基准名称和4个比值
                benchmark = parts[0]
                
                # 构建新的比值行
                new_ratios = ['1']  # base比值总是1
                
                for config in config_order:
                    if config in ratios and benchmark in ratios[config]:
                        ratio = ratios[config][benchmark]
                        new_ratios.append(f"{ratio:.6f}")
                    else:
                        new_ratios.append("1")  # 如果没有数据，保持为1
                
                # 写入新行
                f_out.write(f"{benchmark}\t{' '.join(new_ratios)}\n")
            else:
                # 如果格式不对，保持原样
                f_out.write(line + '\n')

def main():
    # 解析res.txt文件
    data = parse_res_file('../logs/exp2b/res.txt')
    
    # 计算比值
    ratios = calculate_ratios(data)
    
    # 更新base.log文件
    update_base_log('log_data/benchmark_base.log', ratios, 'output.log')
    
    print("处理完成！结果已保存到 output.log")

if __name__ == "__main__":
    main()
