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

# Artifact Evaluation Guidance

## Artifact Check-list

- Code link: https://github.com/Eurosys26p57/Chimera
- OS Version: Bianbu 2.0.4 or Ubuntu 22.04
- Hardware: Banana Pi BPI-F3
- Python version: >= 3.11.2
- Metrics: latency of heterogeneous computing workloads and performance overhead of rewritten binaries
- Expected runtime:
  - Experiment 1: about 10 minutes
  - Experiment 2A: about 5 hours

> **Note: Experiments allow only one person to run at a time. Reviewers should coordinate time slots.**

---

# Experiments

## Connect to the server

1. Run `ssh -p 59348 eurosys2026@210.73.43.1` using the password in ` Eurosys2026@Chimera`

2. Run **tmux** (As our experiments may take a long time, please run all experiments in tmux). **Please strictly follow the tmux commands used in each experiment.**

3. Each experiment will generate a figure in the `./figures` directory. You can download all generated figures to your computer by running `scp -r -P  59348 eurosys2026@210.73.43.1:~/figures figures` with the password in hotcrp **on your computer,** which starts an ssh tunnel and copies all files in `./figures` to your computer.


## Experiment 1: End-to-end heterogeneous computing performance

This experiment runs \texttt{Chimera/MELF/Safer/FAM} with matrix and integer tasks and reports each system's end-to-end latency. This experiment will take about 10 minutes.

**Run experiment:**

- Run tmux: `tmux new -s eurosys2026exp1`
- Run Command: `/home/eurosys2026/runExp1.sh`
- After that, you can detach tmux by  by typing `Ctrl B+D`and log out of the remote terminal by running `exit` (the session will continue running in the background).
- To check the results later, you can simply run `ssh -p 59348 eurosys2026@210.73.43.1` using the password in hotcrp to log into the server and run `tmux attach -t eurosys2026exp1` to reattach to this session.
- Finally, please run `exit` to **exit tmux** and run `exit` to log out the server.

**Output:**

- A pdf file named `./figure/end2endperformance.pdf`, containing the throughput vs. latency of Chimera and baseline systems.
- You can find the log file for generating figures in `./logs/exp1/`

**Expected results:**

- `Chimera` achieves lower latency than `Safer` and `FAM`.
- `Chimera` exhibits similar performance compared to compilation-based `MELF`.

---

## Experiment 2A: Performance of rewritten binaries ("Blender" benchmark in SPEC CPU2017)

This experiment runs "Blender" benchmark in SPEC CPU2017 rewritten by `CHBP/Safer/ARMore/strawman patching` and reports the performance overhead of each rewritten binary relative to the original, which **takes about 5 hours**.

We selected this benchmark for 3 reasons:

- Its indirect jump counts and extension instruction counts are both mid-range among all benchmarks, so using it as the baseline allows a fair comparison of Chimera and the other baselines' performance.
- Its code size is relatively large (7.31 MB) and requires SMILE trampolines to perform long jumps.
- It runs relatively quickly compared to the other benchmarks.

**We recommend running only this experiment**; if you need to run the full SPEC CPU2017 suite, please follow experiment 2B, which requires roughly one week.

**Run experiment:**

- Run tmux: `tmux new -s eurosys2026exp2A`
- Run Command: `/home/eurosys2026/runExp2A.sh`
- After that, you can detach tmux by typing `Ctrl B+D` and log out of the remote terminal by running `exit` (the session will continue running in the background).
- To check the results later, you can simply run `ssh -p 59348 eurosys2026@210.73.43.1` using the password in hotcrp to log into the server and run `tmux attach -t eurosys2026exp2A` to reattach to this session.
- Finally, please run `exit` to **exit tmux** and run `exit` to log out the server.

**Output:**

- A pdf file named `./figure/binaryperformance.pdf`, containing the performance overhead of Chimera and baseline systems.
- You can find the log file for generating figures in `./logs/exp2a/`

**Expected results:**

- Because only the Blender benchmark was run, the final result chart is expected to be a subset of the full results.
- `CHBP` achieves the lowest performance overhead among all baselines.
- Performance overhead of `CHBP` is only about 5%.

---

## Experiment 2B: Performance of rewritten binaries (Entire SPEC CPU2017)

This experiment runs the entire SPEC CPU2017 benchmarks to evaluate `Chimera` and other baselines, which **requires roughly one week**.

**Run experiment:**

- Run tmux: `tmux new -s eurosys2026exp2B`
- Run Command: `/home/eurosys2026/runExp2B.sh`
- After that, you can detach tmux by typing `Ctrl B+D`and log out of the remote terminal by running `exit` (the session will continue running in the background).
- To check the results later, you can simply run `ssh -p 59348 eurosys2026@210.73.43.1` using the password in hotcrp to log into the server and run `tmux attach -t eurosys2026exp2B` to reattach to this session.
- Finally, please run `exit` to **exit tmux** and run `exit` to log out the server.

This experiment will take about one week to complete. The output and expected results are the same as in Experiment 2A.

## Getting Started (if you don't use our server)

- Installation
- Run a demo

## Setup and Installation

## Pre-requistes

Chimera requires the following dependencies:

- RISC-V toolchains :
  - You can get RISC-V banana pi toolchains in https://archive.spacemit.com/toolchain/ 
  - You can get RISC-V official toolchians in https://github.com/riscv-collab/riscv-gnu-toolchain
  - Or use llvm: https://llvm.org/docs/RISCVUsage.html
- python3:
  - python3 (3.11.2+)
  - lief (0.16.0)
- Kernel prepare
  - Install Chimera kernel ( for Banana Pi BPI-F3) following Chimera-kernel](https://github.com/Eurosys26p57/Chimera-kernel)

## Get the Code and Build

First, install Chimera kernel from Chimera-kernel
Then, install Chimera

```shell
 git clone https://github.com/Eurosys26p57/Chimera
 cd Chimera
 #compile binary tools
 cd binarytools/elfdiet 
 make clean && make
 cd ../trampolineinst 
 make clean && make
 #back to Chimera/CHBP
 cd ../..
 ./loadmod.sh
```

## Run a demo

Chimera performs a binary patching by applying a yaml file. We provide a demo in Chimera/CHBP/quickstart/qstart.yaml:

```yaml
#Compiler to compile disassembly to object file
cc_Path: "Path/to/clangorgcc"
#Compile option, the compiler should support all the extensions
cc_option: 
  - "--target=riscv64-unknown-linux-gnu"
  - "-march=rv64gcvb"
  - "-c"
  - "-o"
#objdump path
objdumpPath: "/Path/to/objdump"
#objdump option
objdumpOption:
  - "-d"
  - "--target=riscv64-unknown-linux-gnu"
  - "-march=rv64gcv"
#path of original binary
binary_name: "test1"
#output name
Outputbinary: "test1.rewritten"
#tmp dir to save compiled object files
objdir: "tmp"
#if the original binary has C extension (default: True)
compressedExt: True
#final file after combining all object files
target_objname: "final"
#file to save error handling table
fault_handling_name: "faulttable"
#MMViews of target instructions
MMViews:
- name: "RVVB"
  #jump offset of each trampoline
  jumpoffset: "0x12ff1000"
  target_ISA: RVI
  source_ISA: RVB
  #additional source instructions
  source_ISA_list:
    - "vle64.v"
    - "vfadd.vv"
    - "vsetvli"
    - "vse64.v"
```

Then you can run a demo:

```shell
 #return to the CHBP dir
 cd ..
 #patching the original binary
 python3 CHBP.py quickstart/qstart.yaml
 #load the fault handling table
 ./loadhash.sh PathToFaulttable
 #run the rewritten binary
 ./test1.rewritten
```

You can configure multiple MMViews and run the heterogeneous computing demo:

```shell
 python3 CHBP.py quickstart/qstart_heterogeneous.yaml
./loadhash.sh PathToFaulttable
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
; operand: REG0, REG1, REG2
; live REGS: REG0, REG1, REG2
; Temporary REGS: REG3
zext.w REG3, REG2
add REG0, REG1, REG3
```

Or you can define more complex rules:

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
2. Add "yourrule" to  source_ISA_list in the input yaml.
