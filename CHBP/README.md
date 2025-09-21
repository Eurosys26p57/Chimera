# CHBP

## Overview

Chimera’s core is the binary-rewriting technology CHBP. CHBP consists of two stages:

1. Analysis and translation stage  
2. Binary writing stage

The binary translation and analysis components are under `Chimera/CHBP/ext_tran`. Main workflow:

- Disassemble the binary and identify source instructions that require translation.
- Translate those source instructions into target instructions.
- Choose locations in the binary to place trampolines.
- Select dead registers that can be used when jumping back to the original code region.
- Compute the overall program layout and generate trampoline placement information.

The binary writing stage is implemented under `Chimera/CHBP/binarytools`. Main workflow:

- Write the target instructions into the binary according to the computed address layout.
- Insert trampolines into the binary according to CHBP’s generated placement information.

---

## CHBP Binary Analysis and Translation

The binary translation and analysis components reside in `CHBP/ext_tran`. The modules and their responsibilities are described below.

### objdumpeg

`objdumpeg` defines the data types used to represent disassembly results: instruction type, code block type, and binary type.

- Instruction type:
  - Contains operands, operator (mnemonic), address, and the machine code bytes.
  - Contains the fields:
    - `jumpfrom`: potential predecessor of this instruction (either the previous linear instruction or a branch/jump that targets this instruction).
    - `jumpto`: the successor of this instruction (the next linear instruction, or for branches/jumps the branch target).
  - Records whether the instruction can produce a dead register (a register that is written but never read afterwards).

- Code block type:
  - Processes disassembly results, fills each instruction’s `jumpto` and `jumpfrom` fields, and performs dead-register analysis for instructions and blocks.

- Binary type:
  - Holds the partitioned code blocks and annotates which code blocks need translation and which code blocks may be merged.

### Translator

After `objdumpeg` processing, the Translator takes instructions marked for translation in the Binary and rewrites them according to transformation rules under the `tranblocks` directory. CHBP currently supports upgrading/downgrading the RVB extension and part of the RVV1.0 extension; its rule set can be extended by adding new rule files.

Rules are placed in `Chimera/ext_tran/tranblocks/inst/`. Each rule file corresponds to an instruction or extension and can contain one or more rewrite rules.

Example simple rule file:

```s
Filename: bext.s
; operand: REG0, REG1, REG2
; live REGS: REG0, REG1, REG2
; Temporary REGS: REG3
zext.w REG3, REG2
add REG0, REG1, REG3
```

Example with Extension and Base sections:

```s
Filename: bext.s
Rules:
Extension:
; operand: REG0, REG1, REG2
; live REGS: REG0, REG1, REG2
; Temporary REG:
bext REG0, REG1, REG2

Base:
; operand: REG0, REG1, REG2
; live REGS: REG0, REG1, REG2
; Temporary REGS: REG3
zext.w REG3, REG2
add REG0, REG1, REG3
```

CHBP performs sequence matching between the Extension and Base blocks and then performs register matching according to the comments lines beginning with `;`. The comment fields convey:

- `operand`: registers used by the instruction(s).
- `live REGS`: registers that must be live after translation.
- `Temporary REGS`: registers that are available as temporaries (dead after translation).

#### Adding a rule

To add a new rule:

1. Add `yourrule.s` to `Chimera/ext_tran/tranblocks/inst/`.
2. Add `"yourrule"` to `source_ISA_list` in the input YAML.

### Fixing address-related accesses and maintaining correctness

Because instruction addresses inside translated code blocks change, additional steps are required to ensure correctness. Translated code blocks must handle several issues:

- Preserve and restore registers used during translation:  
  During translation, additional registers may be overwritten. To ensure correctness, the translated code saves the values of those registers on the stack and restores them at the end of the translated block.

- Correct branch/jump offsets:  
  Some code blocks contain direct control-flow instructions (branches and jumps). After translation, the relative positions of instructions may change, so offsets in these branch or jump instructions must be adjusted during translation to reflect the new layout.

- Fix PC-relative instructions (PC-related addressing):  
  When the positions of instructions change, the semantics of PC-relative addressing instructions may change as well. In particular, instructions such as `jalr` and `auipc` must be updated: during translation the implementation recomputes their immediate values and any dependent offsets based on the final locations of the translated instructions.

### Binary Patcher

After slicing and translating the original binary code, the Binary Patcher controls the overall patching process. Its responsibilities include:

1. Marking all locations where trampolines must be placed, and selecting the appropriate trampoline types (this is important for handling compressed instruction extensions).
2. Extending code blocks as necessary to find exit registers that can be used to return to the original code.
3. Allocating addresses for all translated code blocks and setting up their return/continuation behavior.
4. Generating the translated code blocks in binary/assembly form.

**Trampoline selection:**  
The current implementation treats the compressed instruction extension specially. When compressed instructions are enabled, one practical technique to raise deterministic exceptions is to force an erroneous execution into the middle of an instruction; this will produce an unrecognized-instruction (or illegal-instruction) fault that is handled by the fault handler. This mechanism does not rely on causing a segmentation fault through other means, and it allows using dead registers at trampoline sites to perform the jump, which reduces the overhead involved in restoring registers such as the global pointer (gp). Therefore, when a trampoline location has a dead register available, CHBP prefers to use that trampoline strategy.

**Exit-register selection:**  
Not every translated code block will naturally contain a dead register that can be used to return to the original flow. To address this, the Binary Patcher may expand a translated code block until it contains an instruction that produces a dead register. For branching and jumping constructs, different branch targets can be included in the same expanded code block, and different dead registers may be used for returning from different branches. In some cases, an indirect jump can also be used to return directly to the original code region.

After these analyses and selections, the Binary Patcher generates assembly (`.s`) files from the translated blocks and calls the toolchain to assemble/compile the `.s` files into `.o` object files (object generation — stage 1). Once these `.o` files are produced, the Binary Patcher invokes the Address Allocator to allocate addresses for all translated blocks and to merge them into a single combined object.

### Address Allocator

The Address Allocator computes addresses for translated code blocks based on trampoline placements and records each code block’s final address.

- If the translation process creates overlapping address ranges, the allocator assigns different offsets to the overlapping blocks and updates their final addresses accordingly.
- It computes the offsets required for each translated code block to return to the original binary control flow.
- These computed offsets are written back into the `.s` assembly files and the files are recompiled to produce updated `.o` object files (object generation — stage 2).
- Finally, the allocator extracts and merges the code sections from all object files into a single merged code file, and passes this merged code file along with trampoline placement addresses and register information to the binary tools component.

## Binary tools

The binary tools `CHBP/binarytools` component performs the final binary-level modifications:

- Replace original binary locations with trampolines according to the provided trampoline placement information.
- Write the merged object file (containing the translated code) into the binary image.
- Create a separate segment in the modified binary for loading the injected code at the appropriate address/segment.

 
