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
    current_addr_int = int(current_addr, 16)
    target_addr_int = int(target_addr, 16)
    total_offset = target_addr_int - current_addr_int
    auipc_imm = (total_offset + 0x800) >> 12  
    base_address = current_addr_int + (auipc_imm << 12)
    print(auipc_imm)
    if auipc_imm < 0: 
        auipc_imm += (1 << 20)
    jr_offset = target_addr_int - base_address
    return hex(auipc_imm), hex(jr_offset)
def hex_to_twos_complement(hex_str, bit_length=20):
    print("hex_str:", hex_str)
    hex_str = hex_str.lower()
    if hex_str.startswith('0x'):
        hex_str = hex_str[2:]
    elif hex_str.startswith('-0x'):
        hex_str = '-' + hex_str[3:]
    is_negative = hex_str.startswith('-')
    if is_negative:
        hex_str = hex_str[1:]  
    num = int(hex_str, 16)
    if is_negative:
        num = -num
    min_value = - (1 << (bit_length - 1))  
    max_value = (1 << (bit_length - 1)) - 1  
    if num < min_value or num > max_value:
        raise ValueError(f"The value {num} is out of the representable range for {bit_length}-bit two's complement:"
                         f" {min_value} to {max_value}")
    if num < 0:
        num = (1 << bit_length) + num
    bin_format = f'{num:0{bit_length}b}'
    result = hex(int(bin_format, 2))
    return result
def hex_to_signed_hex(hex_str, bits=20):
    value = int(hex_str, 16)
    if value & (1 << (bits - 1)):  
        value -= 1 << bits
    if value < 0:
        return f"-0x{-value:x}"
    else:
        return f"0x{value:x}"
