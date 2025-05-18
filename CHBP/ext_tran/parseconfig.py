import yaml
def parse_from_yaml(file_path):
    with open(file_path, "r") as f:
        config = yaml.safe_load(f)
    config["file_path"] = file_path
    return config
def print_input_config(config):
    info = '''
Get config from {file_path}
Target binary name: {binary_name}
Instructions needed be translated: {source_ISA_list}
Compiler path: {cc_path}
Compiler options: {cc_option}
Objdump path: {objdump_path}
Object tmp dir: {objdir}
Jump offset: {jumpoffset}
