import yaml

def parse_from_yaml(file_path):
    with open(file_path, "r") as f:
        config = yaml.safe_load(f)
    config["file_path"] = file_path

    for k,v in config["MMViews"][0].items():
        config[k] = v 
    config["pass_name"] = config["fault_handling_name"]
    config["target_objname"] = config["output_binary"]

    
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
    '''.format(**config)
    print(info)



# if __name__ == "__main__":
#     config = parse_from_yaml("config.yaml")
#     print_input_config(config)
    



