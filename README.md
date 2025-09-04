# Chimera: Transparent and High-Performance ISAX Heterogeneous Computing via Binary Rewriting
Chimera is a transparent and high-performance heterogeneous computing system via binary rewriting. Chimera can run on RISC-V ISAX heterogeneous cpu platform and provides transparent task scheduling among cores with different extensions (e.g., RVGC cores and RVGCV cores).  Chimera has two parts:  Chimera-userspace (this repository) and Chimera-kernel(https://github.com/Eurosys26p57/Chimera-kernel).
## Abstract
ISAX heterogeneous processors integrate cores that share a common base ISA, with certain cores offering extensions ISAs (e.g., vector extension) to accelerate computation. ISAX balances performance and energy efficiency while facilitating the reuse of existing software ecosystems. RISC-V, which adopts the ISAX architecture, has gained extensive attention in both industry and academia. Binary translation via binary rewriting enables transparent ISAX heterogeneous computing by translating extension instructions when migrating a program to cores without extension support. However, current binary rewriting approaches still struggle to achieve both high performance and correctness.
We propose Chimera, an ISAX heterogeneous computing system via binary rewriting that achieves both correctness and high performance. Prior binary rewriting methods ensure correctness by proactive fault checking, incurring unnecessary runtime overhead in normal executions unlikely to encounter faults. Chimera introduces a new binary rewriting method that passively triggers fault-handling only when faults actually occur, minimizing runtime overhead. We evaluated Chimera and notable ISAX heterogeneous computing systems using real-world workloads. In mixed matrix computational workloads, Chimera achieved only 3.2% performance overhead compared to native compilation, and only 5.3% in real-world workloads like OpenBLAS. On SPEC CPU2017 benchmarks, our method achieved up to 42.5% performance improvement over existing binary rewriting approaches on average. 
## Features
Chimera is a transparent and high-performance heterogeneous computing system via binary rewriting. 
Through binary patching, Chimera enables the upgrade or downgrade of instructions in the original binary without requiring source code, thereby achieving transparent migration of instructions across different ISAX cores.
The binary patching technology CHBP (Correct and High-performance Binary Patching) employed by Chimera ensures high performance and correctness:
- By means of binary patching for binary rewriting, CHBP avoids using traps and instead adopts multiple instructions as long-range jump trampolines to guarantee its performance.
- Chimera ensures that all errors arising from multi-instruction trampolines can be detected and recovered through a passive error mechanism, thereby safeguarding correctness
## Getting Started
- Installation
- Run a demo
## Setup and Installation
## Pre-requistes
Chimera requires the following dependencies:
- RISC-V toolchains :
  - You can get RISC-V banana pi toolchains in https://archive.spacemit.com/toolchain/ 
  - You can get RISC-V official toolchians in https://github.com/riscv-collab/riscv-gnu-toolchain
- python3:
  - python3 (3.11.2+)
  - lief (0.16.0)
- Kernel prepare
  - Install Chimera kernel ( for Banana Pi BPI-F3) following Chimera-kernel](https://github.com/Eurosys26p57/Chimera-kernel)
## Get the Code and Build
First, install Chimera kernel from Chimera-kernel
Then, install Chimera
 git clone https://github.com/Eurosys26p57/Chimera
 cd Chimera
 pip install -r requirement.txt
 #compile binary tools
 cd binarytools
 make
## Run tests
You can run test cases to test if Chimera's runtime mechanism works well:
 cd Chimera
 cd Runtimetest
 ./runtest.sh
## Run a demo
Chimera performs a binary patching by applying a yaml file. We provide a demo in Chimera/CHBP/quickstart/qstart.yaml:

```
 #Compiler to compile disassembly to object file
 ccPath: "Path/to/clangorgcc"
 #Compile option, the compiler should support all the extensions
 ccOption: 
"--target=riscv64-unknown-linux-gnu"
"-march=rv64gcv"
"-c"
"-o"
#objdump path, CHBP now only support gnu objdump
objdumpPath: "/Path/to/objdump"
#objdump option
objdumpOption:
"-d"
"--target=riscv64-unknown-linux-gnu"
"-march=rv64gcv"
  #path of original binary
  binaryName: "test1"
  #output name
  rewrittenBianryName: "test1.rewritten"
  #tmp dir to save compiled object files
  objdir: "tmp"
  #name of the section storing target instructions
  translate_objname: "testobj"
  #if the original binary has C extension
  compressedExt: True
  #jump offset of each trampoline
  jumpoffset: "0x12ff1000"
  #final file after combining all object files
  target_objname: "final"
  #file to save error handling table
  errorHandlingTablePath: "faulttable"
  #file to save migration table
  migrationTablePath: "migrationtable"
  #MMViews of target instructions
  MMViews:
name: "RVB"
  upgradeISA: ""
  #additional instruction to be upgraded
  upgradeAddList: ""
  downgradeISA: "RVB"
  #addiitonal instruction to be downgraded
  downgradeAddList: ""
```

Then you can run a demo:

```
 #return to the CHBP dir
 cd ..
 #patching the original binary
 python3 CHBP.py quickstart/qstart.yaml
 #load the fault handling table
 cat faulttable > /proc/chimerafaulthandling
 #run the rewritten binary
 ./test1.rewritten
```

You can configure multiple MMViews and run the heterogeneous computing demo:
```
 python3 CHBP.py quickstart/qstart_heterogeneous.yaml
 cat faulttable > /proc/chimerafaulthandling
 Chimera/ChimeraLoder/ld test2.rewritten
```
## Extend CHBP
CHBP only support upgrading/downgrading RVB extension and a part of RVV1.0 extension. 
We support extending upgrading/downgrading rules of CHBP. 
Rules Matching
You can add a rules by adding a file named with the extension instruction and construct the one many rules in /Chimera/ext_tran/tranblocks/inst/.
Here is the example:

```
Filename: bext.s
Rules:
Extension:
; operand: REG0, REG1, REG2
; live REGS: REG0, REG1, REG2
; Temporaay REG:  
 bext REG0, REG1, REG2
 
 Base:
; operand: REG0, REG1, REG2
; live REGS: REG0, REG1, REG2
; Temporary REGS: REG3
zext.w REG3, REG2
add REG0, REG1, REG3
```

The CHBP performs instruction matching based on the instruction sequences of the extension and base, and conducts register matching according to the comments marked with ";". The rules for register matching are as follows:
- Operand: Refers to the registers used by the instruction.
- Live REGS: Denotes the live-in registers after translation.
- Temporary REGS: Represents the dead registers after translation.
Add a rule
The CHBP selects the required corresponding rules based on the instruction list in the input YAML file. Specifically, adding a new rule requires the following steps:
1. Add "yourrule.s" in the directory /Chimera/ext_tran/tranblocks/inst/.
2. Add "yourrule" to  upgradeAddList/downgradeAddlist in the input yaml.
