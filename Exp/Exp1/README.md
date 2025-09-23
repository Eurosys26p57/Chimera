# Exp1: Heterogeneous workload

Host: RISC-V board (Banana Pi)
Kernel: Chimera-kernel

## Run workload

Run latency tests (Figure 8b, Figure 8d):

```
./runexp1base.sh
./runexp1extension.sh
```

Results will be saved to the ./result directory.

Run execution-time statistics (Figure 8a, Figure 8c, Figure 9):

```
python statisticres.py
```

Results will also be saved to the ./result directory.
