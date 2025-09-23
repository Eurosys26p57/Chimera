# speccpu2017

## Environment

- Host: RISC-V board, Banana Pi  
- Benchmark: SPEC CPU 2017 v1.1.9

## Workspace

Due to SPEC CPU licensing, we cannot distribute the benchmark. Please obtain SPEC CPU from the official website and place it in this directory.

- The folder `instrument-src` contains patch scripts for the armore, safer, and strawman patching methods.
- The folder `config` contains configurations for running different baselines.

After placing SPEC CPU in the workspace, run:

```
speccpu2017-compile/Makefile.defaults/Makefile.defaults.saferandarmore
speccpu2017-compile/Makefile.defaults/Makefile.defaults.text1mb
```

This will build the original SPEC CPU version as well as the armore, safer, and strawman patched versions.

Next, set the path to your Chimera installation:

```
export Chimera=/path/to/Chimera
```

Use Chimera to patch SPEC CPU:

```
./CHBPpatching.sh
```

## Exp2: CHBP binary rewriting performance

## Figure 10

Run:

```
./runExp2.sh
```

Results will be saved in ./figures.

## Table 2

Run:

```
script/table2.sh
```

Results will be saved to `./result`.

## Table 3

Install the patch in the kernel folder to track the number of error-handling occurrences:

```
cp -r kernelpatch path/to/Chimera/kernel/
git apply *.patch
make clean
make -j$(nproc)
make modules_install
make install
reboot
```

Then re-run the experiment:

```
python3 trace.py
```

The results will be written to `./log/output.txt`.
