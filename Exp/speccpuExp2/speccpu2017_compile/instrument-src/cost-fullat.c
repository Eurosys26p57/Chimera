#include <stdio.h>
#include <stdint-gcc.h>

struct __attribute__((packed)) att_rec
{
    uint64_t old_;
    uint64_t new_;
    uint64_t encPtr_;
    char tramp_[16];
};

struct gtt_sbi
{
    const char *name;
    unsigned long l_addr;
    unsigned long start_addr;
    unsigned long end_addr;
    unsigned long att_start;
    unsigned long att_end;
    unsigned long hash_tbl;
    uint64_t hash_tbl_bit_sz;
    uint64_t hash_tbl_sz;
    uint64_t hash_key;
    struct gtt_sbi *next;
};

struct __attribute__((packed)) gtt_hash_rec
{
    uint64_t page;
    struct gtt_sbi *gtt;
};

struct gtt_sbi *gtt;
struct gtt_hash_rec *gtt_hash;
#define GTT_HASH_KEY 0x9e3779b97f4a7c55

inline unsigned long gtf(unsigned long ptr, unsigned long gtt_size, unsigned long gtt_hash_bit_sz);
inline static unsigned long atf(struct gtt_sbi *gtt, unsigned long ptr);
inline int32_t get_gtt_hash(unsigned long addr, unsigned long gtt_size, unsigned long gtt_hash_bit_sz);
inline uint32_t get_hash(unsigned long ptr, uint64_t entry_cnt, uint64_t hash_tbl_bit_sz, uint64_t key);

inline unsigned long
gtf(unsigned long ptr, unsigned long gtt_size, unsigned long gtt_hash_bit_sz)
{
    long enc_scheme;
    unsigned long ret = ptr;
    enc_scheme = (long)ptr;
    int gtt_ind = get_gtt_hash(ptr, gtt_size, gtt_hash_bit_sz);
    struct gtt_sbi *tmp = gtt_hash[gtt_ind].gtt;
    ret = atf(tmp, ptr);
    return ret;
}

inline static unsigned long
atf(struct gtt_sbi *gtt, unsigned long ptr)
{
    unsigned long hash_tbl = gtt->hash_tbl;
    uint32_t ind = get_hash(ptr - gtt->l_addr, gtt->hash_tbl_sz, gtt->hash_tbl_bit_sz, gtt->hash_key);
    unsigned long entry = hash_tbl + ind * sizeof(void *);
    unsigned long val = *((unsigned long *)(entry));

    struct att_rec *tbl = (struct att_rec *)(gtt->att_start);
    unsigned long translated_ptr = tbl[val].new_;
    return translated_ptr;
}

inline int32_t get_gtt_hash(unsigned long addr, unsigned long gtt_size, unsigned long gtt_hash_bit_sz)
{
    unsigned long page = (addr & 0xfffffffffffff000) >> 12;
    uint32_t ind = get_hash(page, gtt_size, gtt_hash_bit_sz, GTT_HASH_KEY);
    return ind;
}
inline uint32_t
get_hash(unsigned long ptr, uint64_t entry_cnt, uint64_t hash_tbl_bit_sz, uint64_t key)
{
    uint64_t ptr32 = ptr * /*0x9e3779b97f4a7c55 ;*/ key;
    ptr32 = ptr32 >> (64 - hash_tbl_bit_sz);
    ptr32 = ptr32 & (entry_cnt - 1); // ptr32 >> (64 - node->hash_tbl_bit_sz) & 0xffff;
    return ptr32;
}

int _start(unsigned long ptr, unsigned long gtt_size, unsigned long gtt_hash_bit_sz){
	return gtf(ptr, 32, 5);
}
