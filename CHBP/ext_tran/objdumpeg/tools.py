def hex_add(hex_str1, hex_str2, base):
    num1 = int(hex_str1, 16)  
    if base == 10:
        num2 = int(hex_str2, 10)
    elif base == 16:
        num2 = int(hex_str2, 16)
    else:
        raise Exception("base not support!")
    result = num1 + num2
    
    return hex(result)

def auipc_caculate(current_addr, target_addr):
    print("addrs:", current_addr, " ", target_addr)
    if not (current_addr.startswith('0x') and target_addr.startswith('0x')):
        current_addr = '0x' + current_addr
        target_addr = '0x' + target_addr

    # 将地址字符串转换为整数
    current_addr_int = int(current_addr, 16)
    target_addr_int = int(target_addr, 16)

    # 计算AUIPC的立即数：将偏移值的高20位作为立即数
    total_offset = target_addr_int - current_addr_int
    auipc_imm = (total_offset + 0x800) >> 12  # 将偏移值右移12位并加上0x800以便四舍五入

    # 计算基于AUIPC后的基地址
    base_address = current_addr_int + (auipc_imm << 12)
    print(auipc_imm)
    if auipc_imm < 0: # 补码
        auipc_imm += (1 << 20)

    # 计算从基地址到目标地址所需要的实际跳转偏移量
    jr_offset = target_addr_int - base_address

    return hex(auipc_imm), hex(jr_offset)


def hex_to_twos_complement(hex_str, bit_length=20):
    print("hex_str:", hex_str)
    # 取出前缀 '0x' 或 '-0x'
    hex_str = hex_str.lower()
    if hex_str.startswith('0x'):
        hex_str = hex_str[2:]
    elif hex_str.startswith('-0x'):
        hex_str = '-' + hex_str[3:]

    # 检查是否是负数
    is_negative = hex_str.startswith('-')
    if is_negative:
        hex_str = hex_str[1:]  # 移除负号

    # 将16进制转换为整数
    num = int(hex_str, 16)

    # 如果原数是负数，再将其变回负数
    if is_negative:
        num = -num

    # 定义20位补码的范围
    min_value = - (1 << (bit_length - 1))  # -2^(19)
    max_value = (1 << (bit_length - 1)) - 1  # 2^19 - 1

    # 检查数值是否在20位补码的范围内
    if num < min_value or num > max_value:
        raise ValueError(f"The value {num} is out of the representable range for {bit_length}-bit two's complement:"
                         f" {min_value} to {max_value}")

    # 计算补码
    if num < 0:
        # 对于负数，计算补码
        num = (1 << bit_length) + num

    # 将结果格式化为二进制字符串
    bin_format = f'{num:0{bit_length}b}'
    result = hex(int(bin_format, 2))

    return result


def hex_to_signed_hex(hex_str, bits=20):
    """将16进制补码转换为带符号的16进制字符串。

    Args:
    hex_str (str): 16进制字符串，例如 "FFFE"。
    bits (int): 位数，默认为16位。

    Returns:
    str: 转换后的带符号的16进制字符串。
    """
    # 将16进制字符串转换为整数
    value = int(hex_str, 16)

    # 判断是否是负数
    if value & (1 << (bits - 1)):  # 检查符号位
        # 计算负数值
        value -= 1 << bits

    # 将结果格式化为带符号的16进制字符串
    if value < 0:
        return f"-0x{-value:x}"
    else:
        return f"0x{value:x}"

#
# # 示例使用
# hex_number = "ed1ec"
# signed_int = hex_to_signed_hex(hex_number)
# print(signed_int)  # 输出：-2