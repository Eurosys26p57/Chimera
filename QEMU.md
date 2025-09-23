# QEMU Guidance (RISC-V)

If you do not have RISC-V hardware available, you can test Chimera using QEMU. This document describes how to install QEMU, download a RISC-V Ubuntu image, run the provided script, and optionally run QEMU manually. 

---

## Prerequisites
- A Linux host (Ubuntu/Debian recommended).
- Sufficient disk space for the image (several GB).
- Root or sudo privileges to install packages.

---

## 1. Install QEMU

Run:
```
sudo apt update
sudo apt install qemu-system-misc
```

Notes:
- `qemu-system-misc` usually provides `qemu-system-riscv64`. If your distribution uses different package names, you may need `qemu-system` or other qemu packages.
- If you plan to use OpenSBI or U-Boot from packages, install them in the next step.

---

## 2. Download dependencies and the Ubuntu RISC-V image

Install additional packages used by common run scripts:
```
sudo apt install qemu-system-misc opensbi u-boot-qemu qemu-utils
```

Download and extract the Ubuntu preinstalled server image:
```
wget https://old-releases.ubuntu.com/releases/focal/ubuntu-20.04.2-preinstalled-server-riscv64.img.xz
unxz -k ubuntu-20.04.2-preinstalled-server-riscv64.img.xz
```

Notes:
- `unxz -k` keeps the original `.xz` archive and produces `ubuntu-20.04.2-preinstalled-server-riscv64.img`.
- Installed packages:
  - `opensbi` provides firmware/firmware blobs often referenced by the run script.
  - `u-boot-qemu` provides U-Boot images for certain QEMU setups.
  - `qemu-utils` includes tools like `qemu-img`.

---

## 3. Run using the provided script

If the project provides a script `run_qemu_riscv.sh`, run:
```
chmod +x run_qemu_riscv.sh
./run_qemu_riscv.sh start
```

Notes:
- Ensure the script is executable.
- If the script fails, open it to see the exact `qemu-system-riscv64` command used. You can adapt or run that command manually (see the manual example below).

---

## 4. Ubuntu image login credentials

```
Username: ubuntu
Password: abc123456
```

- These credentials work for console login and for SSH if you forward ports or enable networking.

---

## 5. Manual QEMU start command (example)

If you want to run QEMU manually or modify parameters, here is a typical example command:
```
qemu-system-riscv64 \
  -machine virt -nographic \
  -m 4G -smp 4 \
  -bios /path/to/opensbi-fw.bin \
  -device virtio-blk-device,drive=hd0 \
  -drive file=ubuntu-20.04.2-preinstalled-server
```
