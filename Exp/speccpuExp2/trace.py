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

BPF_HASH(counts, u32); 

int trace_sys_enter(struct sys_enter_args *ctx) {
    u32 pid = bpf_get_current_pid_tgid() >> 32; 
    if (ctx->id == 999) { 
        u64 *val = counts.lookup(&pid); 
        if (val) { (*val)++; } 
        else { u64 one = 1; counts.update(&pid, &one); } 
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

#benchmarks = ["500.perlbench_r","502.gcc_r","520.omnetpp_r","523.xalancbmk_r","507.cactuBSSN_r","510.parest_r","521.wrf_r","526.blender_r","527.cam4_r","538.imagick_r","607.cactuBSSN_s","621.wrf_s","627.cam4_s","628.pop2_s","638.imagick_s","600.perlbench_s","602.gcc_s","620.omnetpp_s","623.xalancbmk_s"] 
#benchmarks = ["500.perlbench_r"] 
#benchmarks = ["502.gcc_r","627.cam4_s","602.gcc_s"] 
benchmarks = ["627.cam4_s"] 

for benchmark in benchmarks: 
    print(f"Launching {benchmark}...") 
    proc = subprocess.Popen( ["bash", "-c", f"cd ../speccpu2017 && source ./shrc && cd - && ulimit -s unlimited && runcpu --config=copy-rv64gcv-1ecall {benchmark} > ./log/{benchmark}.out"] )

parent_pid = proc.pid
print(f"Started runcpu with PID {parent_pid}")

try:
    while proc.poll() is None:
        time.sleep(5)
        target_pids = get_all_children(parent_pid)
        counts = b.get_table("counts")

        os.system("clear")
        print(f"Tracking syscall(999) in PID {parent_pid} and its children...\n")
        print(f"{'PID':<20}{'COUNT':<20}")
        for k, v in counts.items():
            if k.value in target_pids:
                print(f"{k.value},{v.value}")

except KeyboardInterrupt:
    print("Interrupted by user.")
finally:
    if proc.poll() is None:
        proc.terminate()
    print("Done.")

with open("./log/output0.txt", "a") as f:
    f.write(f"{benchmark}: \n")
    for k, v in counts.items():
        f.write(f"{k.value},{v.value}\n")
        f.write("\n")
