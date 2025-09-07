riscv_instructions_regs_rules = {
    # ���������������߼�ָ��
    "add": [0, 1, 1],
    "slt": [0, 1, 1],
    "sltu": [0, 1, 1],
    "sub": [0, 1, 1],
    "and": [0, 1, 1],
    "or": [0, 1, 1],
    "xor": [0, 1, 1],
    "sll": [0, 1, 1],
    "srl": [0, 1, 1],
    "sra": [0, 1, 1],
    "csr": [1, 1],
    "csrr": [1, 1],

    # ����������ָ��
    "addi": [0, 1],
    "slti": [0, 1],
    "sltiu": [0, 1],
    "seqz": [0, 1],
    "andi": [0, 1],
    "ori": [0, 1],
    "xori": [0, 1],
    "slli": [0, 1],
    "srli": [0, 1],
    "srai": [0],
    "lui": [0],
    "auipc": [0],

    # ��֧����תָ��
    "beq": [1, 1],
    "bne": [1, 1],
    "blt": [1, 1],
    "bge": [1, 1],
    "bgez": [1, 1],
    "bgtz": [1],
    "beqz": [1],  # ��Ӧ�� beq rs, x0, offset
    "bnez": [1],  # ��Ӧ�� bne rs, x0, offset
    "blez": [1],
    "bltz": [1],
    "bltu": [1],
    "bgeu": [1],
    "snez": [0, 1],
    "negw": [0, 1],
    "mulw": [0, 1, 1],
    "divw": [0, 1, 1],
    "remw": [0, 1, 1],
    "sgtz": [0, 1],
    "jal": [0],
    "jalr": [1],  # [0, 1]��������
    "jr": [1],

    # ������洢ָ��
    "ld": [0, 1],
    "lb": [0, 1],
    "lh": [0, 1],
    "lw": [0, 1],
    "lbu": [0, 1],
    "lhu": [0, 1],
    "lwu": [0, 1],
    "sd": [1, 1],
    "sb": [1, 1],
    "sh": [1, 1],
    "sw": [1, 1],
    
    # 32-bit �������߼������� - rv64 ��չ
    "addw": [0, 1, 1],
    "subw": [0, 1, 1],
    "sllw": [0, 1, 1],
    "srlw": [0, 1, 1],
    "sraw": [0, 1, 1],

    # 32-bit ����������ָ�� - rv64 ��չ
    "addiw": [0, 1],
    "slliw": [0, 1],
    "srliw": [0, 1],
    "sraiw": [0, 1],
    
    # No Operation
    "nop": [],  # ��Ӧ�� addi x0, x0, 0

    # Load Immediate 
    "li": [0],  # �����������ص�Ŀ��Ĵ�����������Ҫ����Ļ���ָ�

    # Move 
    "mv": [0, 1],  # ��Ӧ�� addi rd, rs, 0

    # Not 
    "not": [0, 1],  # ��Ӧ�� xori rd, rs, -1

    # Negate 
    "neg": [0, 1],  # ��Ӧ�� sub rd, x0, rs

    # Branch if Equal to Zero and Branch if Not Equal to Zero


    # Jump
    "j": [],  # ��Ӧ�� jal x0, offset

    # Return from Subroutine
    "ret": [1],  # ��Ӧ�� jalr x0, ra, 0

    # �����ȸ�������ָ��
    "fadd.s": [0, 1, 1],
    "fsub.s": [0, 1, 1],
    "fmul.s": [0, 1, 1],
    "fdiv.s": [0, 1, 1],
    "fabs.s": [0, 1],
    "fneg.s": [0, 1],
    "fsqrt.s": [0, 1],
    "fmin.s": [0, 1, 1],
    "fmax.s": [0, 1, 1],
    "fcvt.l.s": [0, 1],
    "fcvt.d.s": [0, 1],
    "fcvt.s.l": [0, 1],
    "fcvt.s.d": [0, 1],
    "fsgnj.s": [0, 1, 1],
    "fsgnjn.s": [0, 1, 1],
    "fsgnjx.s": [0, 1, 1],

    # ˫���ȸ�������ָ��
    "fadd.d": [0, 1, 1],
    "fsub.d": [0, 1, 1],
    "fmul.d": [0, 1, 1],
    "fdiv.d": [0, 1, 1],
    "fabs.d": [0, 1],
    "fneg.d": [0, 1],
    "fsqrt.d": [0, 1],
    "fmin.d": [0, 1, 1],
    "fmax.d": [0, 1, 1],
    "fcvt.l.d": [0, 1],
    "fcvt.d.l": [0, 1],
    "fsgnj.d": [0, 1, 1],
    "fsgnjn.d": [0, 1, 1],
    "fsgnjx.d": [0, 1, 1],
    "fcvt.d.lu": [0, 1],  # fd 是目标浮点寄存器，rs 是源无符号长整数寄存器
    "fcvt.lu.d": [0, 1],
    "divuw": [0, 1, 1],
    "fmv.d.x": [0, 1],
    "fmv.d": [0, 1],
    "fmv.s": [0, 1],

    "frflags": [0],
    "fsflags": [1],

    # �����ȸ���Ƚ�ָ��
    "feq.s": [0, 1, 1],
    "flt.s": [0, 1, 1],
    "fle.s": [0, 1, 1],

    # ˫���ȸ���Ƚ�ָ��
    "feq.d": [0, 1, 1],
    "flt.d": [0, 1, 1],
    "fle.d": [0, 1, 1],

    # ����-����ת��ָ��
    "fcvt.w.s": [0, 1],
    "fcvt.s.w": [0, 1],
    "fcvt.w.d": [0, 1],
    "fcvt.d.w": [0, 1],

    # ���������洢ָ��
    "flw": [0, 1],
    "fsw": [1, 1],
    "fld": [0, 1],
    "fsd": [1, 1],
    "fmv.x.d": [0, 1],


    # �������غʹ洢ָ��
    "vle32.v": [0, 1],  # Load vector
    "vle64.v": [0, 1],  # Load vector
    "vse32.v": [1, 1],  # Store vector
    "vse64.v": [1, 1],  # Store vector
    "vle.v": [0, 1],  # Load vector
    "vse.v": [1, 1],  # Store vector

    # ��������ָ��
    "vadd.vv": [0, 1, 1],  # Vector-vector addition
    "vsub.vv": [0, 1, 1],  # Vector-vector subtraction
    "vmul.vv": [0, 1, 1],  # Vector-vector multiplication

    # �������������볣ֵ����ָ��
    "vadd.vi": [0, 1],     # Vector-immediate addition
    "vsub.vi": [0, 1],     # Vector-immediate subtraction

    # �����߼�ָ��
    "vand.vv": [0, 1, 1],  # Vector-vector bitwise AND
    "vor.vv": [0, 1, 1],   # Vector-vector bitwise OR
    "vxor.vv": [0, 1, 1],  # Vector-vector bitwise XOR

    # �����Ƚ�ָ��
    "vmslt.vv": [0, 1, 1],  # Vector-vector set if less than
    "vmseq.vv": [0, 1, 1],  # Vector-vector set if equal

    # ������λָ��
    "vsll.vv": [0, 1, 1],  # Vector-vector logical left shift
    "vsrl.vv": [0, 1, 1],  # Vector-vector logical right shift

    # �������ָ��
    "vmerge.vv": [0, 1, 1],  # Conditional merge between two vectors

    # ��������ָ��
    "vfadd.vv": [0, 1, 1],  # Vector-vector floating-point addition
    "vfsub.vv": [0, 1, 1],  # Vector-vector floating-point subtraction

    # ����ʾ�����������ú���������
    "vsetvli": [0, 1],      # Set vector length and configuration

    # �������ܵ�����ָ��
    "vdiv.vv": [0, 1, 1],   # Vector-vector division (�������)
    "vrem.vv": [0, 1, 1],   # Vector-vector remainder (�������)
    "vmulhsu.vv": [0, 1, 1], # Vector-vector multiply high signed-unsigned (�������)

    # ѹ������ָ��
    "c.addi": [0, 1],
    "c.add": [0, 1, 1],
    "c.sub": [0, 1, 1],

    # ѹ�����غʹ洢ָ��
    "c.lw": [0, 1],
    "c.sw": [1, 1],

    # ѹ����תָ��
    "c.j": [],
    "c.jr": [1],
    "c.jal": [0],
    "c.jalr": [0, 1],

    # ѹ����ָ֧��
    "c.beqz": [1],
    "c.bnez": [1],

    # ѹ������������ָ��
    "c.li": [0],
    "c.lui": [0],

    # ѹ��ջ����ָ��
    "c.lwsp": [0],
    "c.swsp": [1],

    # �ƶ����޲���
    "c.mv": [0, 1],
    "c.nop": [],

    # �����˷�ָ��
    "mul": [0, 1, 1],
    "mulh": [0, 1, 1],
    "mulhsu": [0, 1, 1],
    "mulhu": [0, 1, 1],

    # ��������ָ��
    "div": [0, 1, 1],
    "divu": [0, 1, 1],
    "rem": [0, 1, 1],
    "remu": [0, 1, 1],

    # ԭ��װ�غʹ洢ָ��
    "lr.w": [0, 1],
    "sc.w": [0, 1, 1],

    # ԭ������ָ��
    "amoswap.w": [0, 1, 1],
    "amoadd.w": [0, 1, 1],
    "amoxor.w": [0, 1, 1],
    "amoand.w": [0, 1, 1],
    "amoor.w": [0, 1, 1],
    "amomin.w": [0, 1, 1],
    "amomax.w": [0, 1, 1],
    "amominu.w": [0, 1, 1],
    "amomaxu.w": [0, 1, 1],
    
    # FENCE ͬ��ָ��
    "fence": [],  # ���漰�Ĵ�������
    
    # Zero-Extend Halfword (16λ��32λ��
    "zext.h": [0, 1],  # rd ��Ŀ��Ĵ�����rs ��Դ�Ĵ���
    
    "sext.w": [0, 1],

    # Zero-Extend Byte (8λ��32λ)
    "zext.b": [0, 1],  # rd ��Ŀ��Ĵ�����rs ��Դ�Ĵ���
    "zext.w": [0, 1], 

    # Truncate Word (64λ��32λ)
    "trunc.w": [0, 1],  # rd ��Ŀ��Ĵ�����rs ��Դ�Ĵ���

    "csrsi": [0,0,0],
    "csrs": [0,0,0],

    "sh1add": [0, 1, 1],
    "sh1add.uw": [0, 1, 1],
    "sh2add.uw": [0, 1, 1],
    "sh2add": [0, 1, 1],
    "sh3add": [0, 1, 1],
    "sh3add.uw": [0, 1, 1],
    "slli.uw": [0, 1],
    
    "vsetivli":   [0],         # 仅写 rd (vl/vtype)
    "vmv.v.i":    [0],         # 仅写 vd
    "vmv.v.x":    [0, 1],      # 写 vd，读 rs1
    "vfmv.f.s":   [0, 1],      # 写 rd (浮点寄存器)，读 vs1[0]
    "vse8.v":     [1, 1],      # 读 vs1（数据）和 rs1（地址）
    "vs1r.v":     [1, 1],      # 读 vs1（数据）和 rs1（地址）
    "vl1re32.v":  [0, 1],      # 写 vd，读 rs1（地址）
    "vid.v":      [0],         # 仅写 vd（生成 ID）
    "vle8.v":     [0, 1]       # 写 vd，读 rs1（地址）

}


riscv_reg_names = {
    "zero",  # 零寄存器，硬编码为 0
    "ra",    # 返回地址寄存器
    "sp",    # 栈指针寄存器
    "gp",    # 全局指针寄存器
    "tp",    # 线程指针寄存器
    "t0",    # 临时寄存器 0
    "t1",    # 临时寄存器 1
    "t2",    # 临时寄存器 2
    "s0",    # 保存寄存器 0 / 帧指针
    "fp",    # 帧指针（与 s0 相同）
    "s1",    # 保存寄存器 1
    "a0",    # 函数参数/返回值寄存器 0
    "a1",    # 函数参数/返回值寄存器 1
    "a2",    # 函数参数寄存器 2
    "a3",    # 函数参数寄存器 3
    "a4",    # 函数参数寄存器 4
    "a5",    # 函数参数寄存器 5
    "a6",    # 函数参数寄存器 6
    "a7",    # 函数参数寄存器 7
    "s2",    # 保存寄存器 2
    "s3",    # 保存寄存器 3
    "s4",    # 保存寄存器 4
    "s5",    # 保存寄存器 5
    "s6",    # 保存寄存器 6
    "s7",    # 保存寄存器 7
    "s8",    # 保存寄存器 8
    "s9",    # 保存寄存器 9
    "s10",   # 保存寄存器 10
    "s11",   # 保存寄存器 11
    "t3",    # 临时寄存器 3
    "t4",    # 临时寄存器 4
    "t5",    # 临时寄存器 5
    "t6",    # 临时寄存器 6
}

