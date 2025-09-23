#!/bin/bash


find ../rv64gcv-O3 -type f | while read -r file; do

    if file "$file" | grep -q 'ELF'; then
        echo "$file"


        /home/jnkdog/kaiwen/opt/riscv64/bin/riscv64-unknown-linux-gnu-objdump -d "$file" | \
        awk -v fname="$file" '
        /^\s*[[:xdigit:]]+:/ {
            if ($3 ~ /^[a-zA-Z.]+$/) {
                count[$3]++
            }
        }
        END {
            for (instr in count) {
                printf "%s,%s,%d\n", fname, instr, count[instr]
            }
        }' 

    fi
done

