from bcc import BPF
import subprocess
import time
import psutil
import os

prog = """
#include <uapi/linux/ptrace.h>

struct sys_enter_args {
    __u64 unused;
    __u64 id;
};

struct key_t {
    u32 pid;
    u32 id;
};

BPF_HASH(counts, struct key_t);

int trace_sys_enter(struct sys_enter_args *ctx) {
    u32 pid = bpf_get_current_pid_tgid() >> 32;
    u32 id = ctx->id;

    if (id != 998 && id != 999)
        return 0;

    struct key_t key = {};
    key.pid = pid;
    key.id = id;

    u64 *val = counts.lookup(&key);
    if (val) {
        (*val)++;
    } else {
        u64 one = 1;
        counts.update(&key, &one);
    }
    return 0;
}
"""

def get_all_children(pid):
    try:
        parent = psutil.Process(pid)
        children = parent.children(recursive=True)
        return set([c.pid for c in children] + [pid])
    except psutil.NoSuchProcess:
        return set()

b = BPF(text=prog)
b.attach_tracepoint(tp="raw_syscalls:sys_enter", fn_name="trace_sys_enter")

#benchmarks = ["500.perlbench_r","502.gcc_r","520.omnetpp_r","523.xalancbmk_r","507.cactuBSSN_r","510.parest_r","521.wrf_r","526.blender_r","527.cam4_r","538.imagick_r"] 
#benchmarks = ["500.perlbench_r"] 
#benchmarks = ["502.gcc_r"] 
benchmarks = ["527.cam4_r"]

for benchmark in benchmarks:
    print(f"Launching {benchmark}...")
    proc = subprocess.Popen(
        ["bash", "-c",
         f"cd ../speccpu2017 && source ./shrc && cd - && ulimit -s unlimited && runcpu --config=copy-rv64gcv-diff {benchmark} > ./log/{benchmark}.out"]
    )

    parent_pid = proc.pid
    print(f"Started runcpu with PID {parent_pid}")

    try:
        while proc.poll() is None:
            time.sleep(5)
            target_pids = get_all_children(parent_pid)
            counts = b.get_table("counts")

            os.system("clear")
            print(f"Tracking syscall(998/999) in PID {parent_pid} and its children...\n")
            print(f"{'PID':<10} {'SYSCALL':<10} {'COUNT':<10}")
            for k, v in counts.items():
                if k.pid in target_pids:
                    print(f"{k.pid:<10} {k.id:<10} {v.value:<10}")

    except KeyboardInterrupt:
        print("Interrupted by user.")
    finally:
        if proc.poll() is None:
            proc.terminate()
        print("Done.")

    with open(f"./log/output.txt", "a") as f:
        f.write(f"{benchmark}: \n")
        for k, v in counts.items():
            f.write(f"PID={k.pid}, SYSCALL={k.id}, COUNT={v.value}\n")
        f.write("\n")
