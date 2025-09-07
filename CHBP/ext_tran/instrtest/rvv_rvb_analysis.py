import re
import json
import sys

def parse_experiment(content):
    lines = [line.strip() for line in content.split('\n') if line.strip()]
    fib_data = {}
    matrix_data = {}
    total_latency = None
    current_section = None
    for line in lines:
        if line.startswith('[Fibonacci]'):
            current_section = 'fib'
        elif line.startswith('[Matrix]'):
            current_section = 'matrix'
        elif line.startswith('Total latency:'):
            match = re.search(r'Total latency:\s*([\d.]+)ms', line)
            if match:
                total_latency = float(match.group(1))
        else:
            match = re.match(r'^(.+?):\s*([\d.]+)ms$', line)
            if match:
                key = match.group(1).strip()
                value = float(match.group(2))
                if current_section == 'fib':
                    fib_data[key] = value
                elif current_section == 'matrix':
                    matrix_data[key] = value
    return {
        'fibonacci': fib_data,
        'matrix': matrix_data,
        'total_latency': total_latency
    }

def main(input_file, output_file):
    with open(input_file, 'r') as f:
        text = f.read()

    ratio_pattern = re.compile(r'=+\s*Ratio\s+([\d.]+)\s*=+(.*?)(?=\s*=+\s*Ratio|\Z)', re.DOTALL)
    ratio_blocks = ratio_pattern.findall(text)

    output = []
    for ratio, content in ratio_blocks:
        ratio_value = float(ratio)
        exp_sections = re.split(r'=== (Exp\d .*?) ===', content)
        experiments = []
        for i in range(1, len(exp_sections), 2):
            exp_name = exp_sections[i].strip()
            exp_content = exp_sections[i+1].strip()
            parsed = parse_experiment(exp_content)
            experiments.append({
                'name': exp_name,
                **parsed
            })
        output.append({
            'ratio': ratio_value,
            'experiments': experiments
        })

    with open(output_file, 'w') as f:
        json.dump(output, f, indent=2)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = input_file.replace('.txt', '_report.json')
    main(input_file, output_file)
