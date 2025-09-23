#!/bin/sh

for f in ../rv64gcv-type3/*; do
    count=$(~/kaiwen/opt/riscv64/bin/riscv64-unknown-linux-gnu-objdump -d "$f" | grep -E 'ecall$' | wc -l)
    echo "$f: $((count))" >> "chimera_insert_count.txt"
done

