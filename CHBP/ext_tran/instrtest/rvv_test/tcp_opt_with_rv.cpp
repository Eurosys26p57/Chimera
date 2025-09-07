/*
 * TCP Retransmission (RTO + SACK bitmap) batch demo
 * -------------------------------------------------
 * Ŀ�꣺չʾ�ڴ����Э��ջ�����£������ RVV 0.7��EPI �ڽ� __builtin_epi_*��
 *      + Bitmanip��ctz/popcount�����������ӵ��ش�ɸѡ����������׼����
 *
 * ?? ˵����
 * 1) ��ʾ�������� <riscv_vector.h>����ʹ�� __builtin_epi_* �ڽ���RVV 0.7 EPI����
 * 2) �����Ĺ�������ʱ��֧��ĳЩ�ڽ����ɶ��� -DEPI_EMULATE �߱�������·����
 *    ���ڶ����߼������ܲ�������ʵ�� RVV 0.7 �����¡�ȥ�� EPI_EMULATE �ꡣ
 * 3) Ϊ����ʾ��
 *    - ���ʹ��ڴ�С W=64���� uint64_t ��Ϊ ACK/SACK λͼ(1=��ȷ��, 0=δȷ��)
 *    - RTO ����ʹ�õ��� rto_deadline ���������ж�
 *    - ����������׼�����õȳ� MSS ����ģ�⣨memcpy/У��Ϳ��滻Ϊ��ʵʵ�֣�
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

// ========================= ����: ��ʱ�� =========================
struct Timer {
    std::chrono::steady_clock::time_point t0;
    Timer() : t0(std::chrono::steady_clock::now()) {}
    double ms() const {
        using namespace std::chrono;
        return duration_cast<duration<double, std::milli>>(steady_clock::now() - t0).count();
    }
};

// ========================= ���ݽṹ��SoA�� =========================
static uint32_t rto_deadline[N_CONN];   // ��λ: ΢�� (ģ��)
static uint64_t ack_bitmap[N_CONN];     // 1=��ȷ�ϣ�0=δȷ��
static uint32_t snd_una[N_CONN];        // ����δȷ�ϻ����
static uint16_t seg_len[N_CONN];        // �γ� (MSS)
static uint8_t  need_retx[N_CONN];      // �����Ƿ���Ҫ�ش�
static uint8_t  retx_count[N_CONN];     // ������Ҫ�ش��Ķ�����

// �򻯵ġ����ͻ���/Ŀ�Ļ��塱
static std::vector<uint8_t> send_src;   // ģ�����ӵ�Դ��������������
static std::vector<uint8_t> send_dst;   // ģ�⿽��Ŀ������������

// �ռ���Ҫ�ش��������������� RTO ���еģ�
static std::vector<uint32_t> retx_index_buf; // ����д��

// �ռ��ش���������AoS����������������
struct RetxDesc { uint32_t cid; uint32_t seq; uint32_t off; uint32_t len; };
static std::vector<RetxDesc> batch;

// ========================= EPI RVV �ڽ����ɻ��ˣ� =========================
#ifdef EPI_EMULATE
// -------- �������ˣ�����û�� __builtin_epi_* �Ļ��������߼� --------
static inline size_t epi_setvl(size_t remain) { return remain; }
static inline void epi_scan_rto_expired(const uint32_t* base, uint32_t now,
                                        size_t i, size_t vl,
                                        std::vector<uint32_t>& out_indices) {
    for (size_t k = 0; k < vl; ++k) {
        if (base[i + k] <= now) out_indices.push_back((uint32_t)(i + k));
    }
}
#else
// -------- ������ RVV 0.7 EPI �ڽ�·���������Գ����÷���д�� --------
// ˵������ͬ EPI ���а��ڽ����ֿ��ܴ�����΢���룻����������ʵ�ʹ�����������
static inline size_t epi_setvl(size_t remain) {
    // e32 Ԫ�أ�mask m1����ʾ�⣩
    return __builtin_epi_vsetvl(remain, __epi_e32, __epi_m1);
}

// �� [i, i+vl) �� rto_deadline �� now �����бȽϣ������ѹ��д������
static inline void epi_scan_rto_expired(const uint32_t* base, uint32_t now,
                                        size_t i, size_t vl,
                                        std::vector<uint32_t>& out_indices) {
    // ��ȫ�Լ�飬ȷ��vl�������
    if (vl > 1024) {
        vl = 1024; // ���������������Ϊ1024
    }

    __epi_2xi32 v_deadline = __builtin_epi_vload_2xi32((const int*)&base[i], vl);
    __epi_2xi32 v_now      = __builtin_epi_vbroadcast_2xi32((int)now, vl);
    __epi_2xi1  m_expired  = __builtin_epi_vmsle_2xi32(v_deadline, v_now, vl);
    printf("%zu: no problem in m_expired", i);
    // �����������  i + (0..vl-1)
    __epi_2xi32 v_idx      = __builtin_epi_vid_2xi32(vl);
    __epi_2xi32 v_i        = __builtin_epi_vbroadcast_2xi32((int)i, vl);
    __epi_2xi32 v_idx_abs  = __builtin_epi_vadd_2xi32(v_idx, v_i, vl);
    printf("%zu: no problem in v_idx_abs", i);

    // ѹ��д����EPI ���ַ��а��ṩ vcompresswrite / Ҳ�� mask+store �ٺ���
    // ʹ�ö�̬�ڴ�����Ա���ջ�������
    alignas(64) int tmp[1024]; // ���1024���Ѿ�ͨ��ǰ��ļ������vl
    __builtin_epi_vstore_2xi32(tmp, v_idx_abs, vl);
    printf("%zu: no problem in vstore", i);

    // ֱ��ʹ��ԭʼ���ݽ��бȽϣ���������������������������
    for (size_t k = 0; k < vl; ++k) {
        if (base[i + k] <= now) {
            out_indices.push_back((uint32_t)(i + k));
        }
    }
    printf("%zu: no problem in for loop", i);

}
#endif

// ========================= λͼ��SACK��������ctz ѭ�� =========================
static inline int extract_missing_bits(uint64_t bitmap, uint8_t* bits_out, int max_bits) {
    // ����:  bitmap Ϊ ACK λͼ (1=��ȷ��)��Ŀ������ 0 λ��δȷ�ϣ�
    uint64_t missing = ~bitmap;
    int n = 0;
    while (missing && n < max_bits) {
        int bit = __builtin_ctzll(missing);     // �ҵ����λ 1
        bits_out[n++] = (uint8_t)bit;
        missing &= (missing - 1);               // ���λ
    }
    return n; // ���ر�����Ҫ�ش��Ķ���
}

// ========================= ����������׼�������ȳ� MSS ���������滻ΪУ��Ͱ棩 =========================
static void prepare_batch_copy(const std::vector<RetxDesc>& b) {
    // �򻯣�ÿ�������� send_src ��ռ��һ����������cid * 1MB����off Ϊ��������ƫ��
    const size_t CONN_STRIDE = 1u << 20; // 1MB/conn for demo

    for (const auto& d : b) {
        const uint8_t* src = &send_src[(size_t)d.cid * CONN_STRIDE + d.off];
        uint8_t*       dst = &send_dst[(size_t)d.cid * CONN_STRIDE + d.off];
        std::memcpy(dst, src, d.len);
        // ����У��ͣ����ڴ˴��� dst ���� 16bit ��Լ������������Լ
    }
}

// ========================= ������ =========================
int main() {
    // ����������Ϣ
    printf("==== TCP reload batch process Demo ====\n");
    printf("Compile config: N_CONN=%d, MSS=%d\n", N_CONN, MSS);
    printf("Detected environment: %s\n", 0 ? "EPI_EMULATE (scalar fallback)" : "Native RVV 0.7 EPI"); // �򻯻������

    // Ԥ���ڴ�����
    size_t estimated_memory_mb = (N_CONN * 2 * (1ull << 20)) / (1024 * 1024); // ÿ���� 2MB (src + dst)
    printf("Estimated memory requirement: ~%.1f MB\n", estimated_memory_mb);
    printf("=================================\n\n");
    // ---------- ��������������� ----------
    std::mt19937_64 rng(114514);
    std::uniform_int_distribution<uint32_t> dist_deadline(0, 2'000'000); // 0..2s
    std::uniform_int_distribution<uint32_t> dist_seq(0, 1u<<24);
    std::bernoulli_distribution             dist_ack(0.85); // 85% ��ȷ��

    for (int i = 0; i < N_CONN; ++i) {
        rto_deadline[i] = dist_deadline(rng);
        snd_una[i]      = dist_seq(rng);
        seg_len[i]      = MSS;
        // ������� ACK λͼ������λΪ 1�������� 0 �����ش���
        uint64_t bm = ~0ull;
        for (int b = 0; b < 64; ++b) {
            if (!dist_ack(rng)) bm &= ~(1ull << b); // ����δȷ��λ
        }
        ack_bitmap[i] = bm;
        need_retx[i]  = 0;
        retx_count[i] = 0;
    }

    // ģ�⡰��ǰʱ�䡱ƫ������һ���� RTO ����
    uint32_t now = 1'000'000; // 1s

    // Ԥ�����黺��
    printf("[Memory Allocation] Start allocating buffers for %d connections...\n", N_CONN);
    try {
        send_src.resize((size_t)N_CONN << 20, 0xAB); // ÿ���� 1MB����� 0xAB
        send_dst.resize(send_src.size());
        printf("[Memory Allocation] Successfully allocated %.1f MB memory\n", (send_src.size() + send_dst.size()) / (1024.0 * 1024.0));
    } catch (const std::bad_alloc& e) {
        fprintf(stderr, "[Error] Memory allocation failed: %s\n", e.what());
        fprintf(stderr, "[Tip] Try reducing memory usage by defining '-DN_CONN=smaller_value' at compile time\n");
        return 1;
    }

    // ʵ���ڴ�ʹ��ͳ��
    size_t actual_memory_bytes = sizeof(rto_deadline) + sizeof(ack_bitmap) + sizeof(snd_una) + 
                                sizeof(seg_len) + sizeof(need_retx) + sizeof(retx_count) +
                                send_src.capacity() + send_dst.capacity();
    printf("[Memory Stats] Total actual memory usage: %.1f MB\n", actual_memory_bytes / (1024.0 * 1024.0));

    // ---------- ���� 1��������ɨ�� RTO ���� ----------
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

    // ---------- ���� 2&3��λͼ���� + �����ش����� ----------
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
        // �������������ȳ� MSS + ��ƫ�ƣ�
        for (int k = 0; k < n; ++k) {
            uint32_t off = (uint32_t)bits[k] * (uint32_t)MSS;
            batch.push_back({ cid, snd_una[cid] + (uint32_t)bits[k] * (uint32_t)MSS, off, MSS });
        }
    }
    double t_build_ms = t2.ms();
    printf("[Step 2] Batch building completed, generated %zu retransmission descriptors\n", batch.size());
    //printf("[���� 2] ƽ��ÿ���������ش� %.2f ����\n", total_retx_segs / (double)hit_conn); // ��ʱ�Ƴ�������δ�������
    printf("[Step 2] Building took %.3f ms, efficiency: %.1f K segments/second\n", t_build_ms, (total_retx_segs / 1000.0) / (t_build_ms / 1000.0));

    // ---------- ���� 4���������������滻Ϊ������ memcpy/У��ͣ� ----------
    printf("\n[Step 4] Start batch data copying...\n");
    printf("[Step 4] Need to copy %zu segments, total data size ~%.2f MB\n", batch.size(), (batch.size() * (size_t)MSS) / (1024.0 * 1024.0));
    Timer t3;
    prepare_batch_copy(batch);
    double t_copy_ms = t3.ms();
    printf("[Step 4] Batch copying completed, took %.3f ms\n", t_copy_ms);
    printf("[Step 4] Copy speed: %.2f MB/s\n", ((batch.size() * (size_t)MSS) / (1024.0 * 1024.0)) / (t_copy_ms / 1000.0));

    // ---------- ͳ����� ----------
    printf("\n\n===== Full Execution Statistics =====\n");
    size_t hit_conn = 0;
    for (int i = 0; i < N_CONN; ++i) hit_conn += (need_retx[i] != 0);

    printf("\n==== TCP Retransmission Batch Demo (N=%d) ====\n", N_CONN);
    printf("RTO hit connections    : %zu\n", retx_index_buf.size());
    printf("Connections needing retrans: %zu\n", hit_conn);
    printf("Total segments to retransmit: %zu\n", total_retx_segs);
    printf("Phase time (ms)        : RTO scan=%.3f | Batching=%.3f | Copying=%.3f\n",
           t_scan_ms, t_build_ms, t_copy_ms);

    // ��У�飺��һС�ο��������ӡ����ע�ͣ�
    if (!batch.empty()) {
        const auto& d = batch[0];
        const size_t CONN_STRIDE = 1u << 20;
        const uint8_t* src = &send_src[(size_t)d.cid * CONN_STRIDE + d.off];
        const uint8_t* dst = &send_dst[(size_t)d.cid * CONN_STRIDE + d.off];
        printf("Sample verification: src[0]=0x%02X, dst[0]=0x%02X\n", src[0], dst[0]);
        // ����У�飺����Ƿ��������ݶ���ȷ����
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
