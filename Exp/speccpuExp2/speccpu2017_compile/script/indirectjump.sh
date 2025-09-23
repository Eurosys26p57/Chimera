#!/bin/sh

for f in ../rv64gcv-armore-O3/*; do
    count=$(~/kaiwen/opt/riscv64/bin/riscv64-unknown-linux-gnu-objdump -d "$f" | grep -E 'ecall$' | wc -l)
    echo "$f: $((count / 3))" >> "indirectjump.txt"
done

