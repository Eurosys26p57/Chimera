import json


def read_json_file(file_path):
    try:
        with open(file_path, 'r') as file:
            data = json.load(file)
            return data
    except FileNotFoundError:
        print("错误: 文件未找到!")
    except json.JSONDecodeError:
        print("错误: 文件不是有效的 JSON 格式!")
    except Exception as e:
        print(f"错误: 发生了一个未知错误: {e}")
    return None


if __name__ == "__main__":
    file_path = 'tmp/testobj_pass-0.json'  # 请确保该文件存在并包含有效的 JSON 内容
    content = read_json_file(file_path)
    addrs = [int(x[0], 16) for x in content["trampoline_addr"]]
    addrs.sort()
    print(addrs)
    print(hex(addrs[0]))
    print(hex(addrs[-1]))
