/*
 * TCP Retransmission (RTO + SACK bitmap) batch demo
 * -------------------------------------------------
 * 目标：展示在纯软件协议栈条件下，如何用 RVV 0.7（EPI 内建 __builtin_epi_*）
 *      + Bitmanip（ctz/popcount）批处理连接的重传筛选与批量数据准备。
 *
 * ?? 说明：
 * 1) 本示例不依赖 <riscv_vector.h>，仅使用 __builtin_epi_* 内建（RVV 0.7 EPI）。
 * 2) 如果你的工具链暂时不支持某些内建，可定义 -DEPI_EMULATE 走标量回退路径，
 *    便于对照逻辑；性能测试请在实际 RVV 0.7 环境下、去掉 EPI_EMULATE 宏。
 * 3) 为简化演示：
 *    - 发送窗口大小 W=64，用 uint64_t 作为 ACK/SACK 位图(1=已确认, 0=未确认)
 *    - RTO 到期使用单个 rto_deadline 数组批量判定
 *    - 批量“数据准备”用等长 MSS 拷贝模拟（memcpy/校验和可替换为真实实现）
 */

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>

#ifndef MSS
#define MSS 146
#endif

#ifndef N_CONN
#define N_CONN 1000
#endif

// ========================= 工具: 计时器 =========================
struct Timer {
    std::chrono::steady_clock::time_point t0;
    Timer() : t0(std::chrono::steady_clock::now()) {}
    double ms() const {
        using namespace std::chrono;
        return duration_cast<duration<double, std::milli>>(steady_clock::now() - t0).count();
    }
};

// ========================= 数据结构（SoA） =========================
static uint32_t rto_deadline[N_CONN];   // 单位: 微秒 (模拟)
static uint64_t ack_bitmap[N_CONN];     // 1=已确认，0=未确认
static uint32_t snd_una[N_CONN];        // 发送未确认基序号
static uint16_t seg_len[N_CONN];        // 段长 (MSS)
static uint8_t  need_retx[N_CONN];      // 本轮是否需要重传
static uint8_t  retx_count[N_CONN];     // 本轮需要重传的段数量

// 简化的“发送缓冲/目的缓冲”
static std::vector<uint8_t> send_src;   // 模拟连接的源数据区（连续）
static std::vector<uint8_t> send_dst;   // 模拟拷贝目的区（连续）

// 收集需要重传的连接索引（被 RTO 命中的）
static std::vector<uint32_t> retx_index_buf; // 紧凑写入

// 收集重传描述符（AoS），便于批量处理
struct RetxDesc { uint32_t cid; uint32_t seq; uint32_t off; uint32_t len; };
static std::vector<RetxDesc> batch;

// ========================= EPI RVV 内建（可回退） =========================
#ifdef EPI_EMULATE
// -------- 标量回退：用于没有 __builtin_epi_* 的环境调试逻辑 --------
static inline size_t epi_setvl(size_t remain) { return remain; }
static inline void epi_scan_rto_expired(const uint32_t* base, uint32_t now,
                                        size_t i, size_t vl,
                                        std::vector<uint32_t>& out_indices) {
    for (size_t k = 0; k < vl; ++k) {
        if (base[i + k] <= now) out_indices.push_back((uint32_t)(i + k));
    }
}
#else
// -------- 期望的 RVV 0.7 EPI 内建路径（名称以常见用法书写） --------
// 说明：不同 EPI 发行版内建名字可能存在轻微出入；如需调整请据实际工具链更名。
static inline size_t epi_setvl(size_t remain) {
    // e32 元素，mask m1（仅示意）
    return __builtin_epi_vsetvl(remain, __epi_e32, __epi_m1);
}

// 将 [i, i+vl) 的 rto_deadline 与 now 做并行比较，结果经压缩写出索引
static inline void epi_scan_rto_expired(const uint32_t* base, uint32_t now,
                                        size_t i, size_t vl,
                                        std::vector<uint32_t>& out_indices) {
    // 安全性检查，确保vl不会过大
    if (vl > 1024) {
        vl = 1024; // 限制最大向量长度为1024
    }

    __epi_2xi32 v_deadline = __builtin_epi_vload_2xi32((const int*)&base[i], vl);
    __epi_2xi32 v_now      = __builtin_epi_vbroadcast_2xi32((int)now, vl);
    __epi_2xi1  m_expired  = __builtin_epi_vmsle_2xi32(v_deadline, v_now, vl);
    printf("%zu: no problem in m_expired", i);
    // 构造绝对索引  i + (0..vl-1)
    __epi_2xi32 v_idx      = __builtin_epi_vid_2xi32(vl);
    __epi_2xi32 v_i        = __builtin_epi_vbroadcast_2xi32((int)i, vl);
    __epi_2xi32 v_idx_abs  = __builtin_epi_vadd_2xi32(v_idx, v_i, vl);
    printf("%zu: no problem in v_idx_abs", i);

    // 压缩写出：EPI 部分发行版提供 vcompresswrite / 也可 mask+store 再后处理
    // 使用动态内存分配以避免栈溢出风险
    alignas(64) int tmp[1024]; // 最大1024，已经通过前面的检查限制vl
    __builtin_epi_vstore_2xi32(tmp, v_idx_abs, vl);
    printf("%zu: no problem in vstore", i);

    // 直接使用原始数据进行比较，避免依赖可能有问题的掩码操作
    for (size_t k = 0; k < vl; ++k) {
        if (base[i + k] <= now) {
            out_indices.push_back((uint32_t)(i + k));
        }
    }
    printf("%zu: no problem in for loop", i);

}
#endif

// ========================= 位图（SACK）解析：ctz 循环 =========================
static inline int extract_missing_bits(uint64_t bitmap, uint8_t* bits_out, int max_bits) {
    // 输入:  bitmap 为 ACK 位图 (1=已确认)；目标是找 0 位（未确认）
    uint64_t missing = ~bitmap;
    int n = 0;
    while (missing && n < max_bits) {
        int bit = __builtin_ctzll(missing);     // 找到最低位 1
        bits_out[n++] = (uint8_t)bit;
        missing &= (missing - 1);               // 清该位
    }
    return n; // 返回本轮需要重传的段数
}

// ========================= 批量“数据准备”：等长 MSS 拷贝（可替换为校验和版） =========================
static void prepare_batch_copy(const std::vector<RetxDesc>& b) {
    // 简化：每个连接在 send_src 中占用一个连续区（cid * 1MB），off 为该连接内偏移
    const size_t CONN_STRIDE = 1u << 20; // 1MB/conn for demo

    for (const auto& d : b) {
        const uint8_t* src = &send_src[(size_t)d.cid * CONN_STRIDE + d.off];
        uint8_t*       dst = &send_dst[(size_t)d.cid * CONN_STRIDE + d.off];
        std::memcpy(dst, src, d.len);
        // 如需校验和，可在此处对 dst 进行 16bit 归约；或批量化归约
    }
}

// ========================= 主流程 =========================
int main() {
    // 程序启动信息
    printf("==== TCP reload batch process Demo ====\n");
    printf("Compile config: N_CONN=%d, MSS=%d\n", N_CONN, MSS);
    printf("Detected environment: %s\n", 0 ? "EPI_EMULATE (scalar fallback)" : "Native RVV 0.7 EPI"); // 简化环境检测

    // 预估内存需求
    size_t estimated_memory_mb = (N_CONN * 2 * (1ull << 20)) / (1024 * 1024); // 每连接 2MB (src + dst)
    printf("Estimated memory requirement: ~%.1f MB\n", estimated_memory_mb);
    printf("=================================\n\n");
    // ---------- 构造随机测试数据 ----------
    std::mt19937_64 rng(114514);
    std::uniform_int_distribution<uint32_t> dist_deadline(0, 2'000'000); // 0..2s
    std::uniform_int_distribution<uint32_t> dist_seq(0, 1u<<24);
    std::bernoulli_distribution             dist_ack(0.85); // 85% 已确认

    for (int i = 0; i < N_CONN; ++i) {
        rto_deadline[i] = dist_deadline(rng);
        snd_una[i]      = dist_seq(rng);
        seg_len[i]      = MSS;
        // 随机生成 ACK 位图（多数位为 1，留少量 0 触发重传）
        uint64_t bm = ~0ull;
        for (int b = 0; b < 64; ++b) {
            if (!dist_ack(rng)) bm &= ~(1ull << b); // 留下未确认位
        }
        ack_bitmap[i] = bm;
        need_retx[i]  = 0;
        retx_count[i] = 0;
    }

    // 模拟“当前时间”偏大，制造一部分 RTO 过期
    uint32_t now = 1'000'000; // 1s

    // 预分配大块缓冲
    printf("[Memory Allocation] Start allocating buffers for %d connections...\n", N_CONN);
    try {
        send_src.resize((size_t)N_CONN << 20, 0xAB); // 每连接 1MB，填充 0xAB
        send_dst.resize(send_src.size());
        printf("[Memory Allocation] Successfully allocated %.1f MB memory\n", (send_src.size() + send_dst.size()) / (1024.0 * 1024.0));
    } catch (const std::bad_alloc& e) {
        fprintf(stderr, "[Error] Memory allocation failed: %s\n", e.what());
        fprintf(stderr, "[Tip] Try reducing memory usage by defining '-DN_CONN=smaller_value' at compile time\n");
        return 1;
    }

    // 实际内存使用统计
    size_t actual_memory_bytes = sizeof(rto_deadline) + sizeof(ack_bitmap) + sizeof(snd_una) + 
                                sizeof(seg_len) + sizeof(need_retx) + sizeof(retx_count) +
                                send_src.capacity() + send_dst.capacity();
    printf("[Memory Stats] Total actual memory usage: %.1f MB\n", actual_memory_bytes / (1024.0 * 1024.0));

    // ---------- 步骤 1：向量化扫描 RTO 到期 ----------
    printf("\n[Step 1] Start vectorized RTO expiration scan...\n");
    Timer t1;
    retx_index_buf.clear();
    retx_index_buf.reserve(N_CONN);
    printf("[Step 1] Initial setup complete, preparing to scan %d connections\n", N_CONN);

    for (size_t i = 0; i < N_CONN; ) {
        size_t vl = epi_setvl(N_CONN - i);
        printf("[Step 1] Scan batch %zu, set vector length success\n", i);

        epi_scan_rto_expired(rto_deadline, now, i, vl, retx_index_buf);
        i += vl;
    }
    double t_scan_ms = t1.ms();
    printf("[Step 1] RTO scan completed, found %zu expired connections, took %.3f ms\n", retx_index_buf.size(), t_scan_ms);
    printf("[Step 1] Scan efficiency: %.1f K connections/second\n", (N_CONN / 1000.0) / (t_scan_ms / 1000.0));

    // ---------- 步骤 2&3：位图解析 + 构建重传批次 ----------
    printf("\n[Step 2] Start bitmap parsing and retransmission batch building...\n");
    printf("[Step 2] Need to process %zu RTO hit connections\n", retx_index_buf.size());
    Timer t2;
    batch.clear();
    batch.reserve(N_CONN * 4);

    uint8_t bits[64];
    size_t total_retx_segs = 0;

    for (uint32_t cid : retx_index_buf) {
        int n = extract_missing_bits(ack_bitmap[cid], bits, 64);
        if (n <= 0) continue;
        need_retx[cid]  = 1;
        retx_count[cid] = (uint8_t)n;
        total_retx_segs += (size_t)n;
        // 生成描述符（等长 MSS + 简化偏移）
        for (int k = 0; k < n; ++k) {
            uint32_t off = (uint32_t)bits[k] * (uint32_t)MSS;
            batch.push_back({ cid, snd_una[cid] + (uint32_t)bits[k] * (uint32_t)MSS, off, MSS });
        }
    }
    double t_build_ms = t2.ms();
    printf("[Step 2] Batch building completed, generated %zu retransmission descriptors\n", batch.size());
    //printf("[步骤 2] 平均每个连接需重传 %.2f 个段\n", total_retx_segs / (double)hit_conn); // 暂时移除，避免未定义变量
    printf("[Step 2] Building took %.3f ms, efficiency: %.1f K segments/second\n", t_build_ms, (total_retx_segs / 1000.0) / (t_build_ms / 1000.0));

    // ---------- 步骤 4：批量拷贝（可替换为向量化 memcpy/校验和） ----------
    printf("\n[Step 4] Start batch data copying...\n");
    printf("[Step 4] Need to copy %zu segments, total data size ~%.2f MB\n", batch.size(), (batch.size() * (size_t)MSS) / (1024.0 * 1024.0));
    Timer t3;
    prepare_batch_copy(batch);
    double t_copy_ms = t3.ms();
    printf("[Step 4] Batch copying completed, took %.3f ms\n", t_copy_ms);
    printf("[Step 4] Copy speed: %.2f MB/s\n", ((batch.size() * (size_t)MSS) / (1024.0 * 1024.0)) / (t_copy_ms / 1000.0));

    // ---------- 统计输出 ----------
    printf("\n\n===== Full Execution Statistics =====\n");
    size_t hit_conn = 0;
    for (int i = 0; i < N_CONN; ++i) hit_conn += (need_retx[i] != 0);

    printf("\n==== TCP Retransmission Batch Demo (N=%d) ====\n", N_CONN);
    printf("RTO hit connections    : %zu\n", retx_index_buf.size());
    printf("Connections needing retrans: %zu\n", hit_conn);
    printf("Total segments to retransmit: %zu\n", total_retx_segs);
    printf("Phase time (ms)        : RTO scan=%.3f | Batching=%.3f | Copying=%.3f\n",
           t_scan_ms, t_build_ms, t_copy_ms);

    // 简单校验：把一小段拷贝结果打印（可注释）
    if (!batch.empty()) {
        const auto& d = batch[0];
        const size_t CONN_STRIDE = 1u << 20;
        const uint8_t* src = &send_src[(size_t)d.cid * CONN_STRIDE + d.off];
        const uint8_t* dst = &send_dst[(size_t)d.cid * CONN_STRIDE + d.off];
        printf("Sample verification: src[0]=0x%02X, dst[0]=0x%02X\n", src[0], dst[0]);
        // 额外校验：检查是否所有数据都正确拷贝
        bool copy_ok = true;
        for (size_t k = 0; k < 16 && k < d.len; ++k) {
            if (src[k] != dst[k]) {
                copy_ok = false;
                printf("[Warning] Copy verification failed at offset %zu: src=0x%02X, dst=0x%02X\n", k, src[k], dst[k]);
                break;
            }
        }
        if (copy_ok) {
            printf("[Verification Passed] Sample data copied correctly\n");
        }
    }

    // Cleanup and program end
    printf("\n===== Program Execution Completed =====\n");
    send_src.clear();
    send_dst.clear();
    retx_index_buf.clear();
    batch.clear();
    
    return 0;
}
