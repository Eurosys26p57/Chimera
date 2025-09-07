#include <stdio.h>
#include <stdint.h>
#include <time.h>

// 传统方法：循环统计1的位数
int hamming_weight_loop(uint32_t x) {
    int count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

// B扩展方法：使用cpop指令
int hamming_weight_b(uint32_t x) {
    uint64_t x_zext = (uint64_t)x; // 零扩展至64位
    int count;
    __asm__ volatile (
        "cpop %0, %1"
        : "=r" (count)
        : "r" (x_zext)
    );
    return count;
}

// 测试函数
void test_hamming(uint32_t x) {
    struct timespec start, end;
    int loop_result, b_result;
    double time_loop, time_b;

    // 测试传统方法
    clock_gettime(CLOCK_MONOTONIC, &start);
    loop_result = hamming_weight_loop(x);
    clock_gettime(CLOCK_MONOTONIC, &end);
    time_loop = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);

    // 测试B扩展方法
    clock_gettime(CLOCK_MONOTONIC, &start);
    b_result = hamming_weight_b(x);
    clock_gettime(CLOCK_MONOTONIC, &end);
    time_b = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);

    // 输出结果
    printf("Value: 0x%08x\n", x);
    printf("Loop result: %d (%.2f ns)\n", loop_result, time_loop);
    printf("B-ext result: %d (%.2f ns)\n", b_result, time_b);
    printf("Speedup: %.2fx\n\n", time_loop / time_b);
}

int main() {
    // 测试不同数据模式
    test_hamming(0x00000000);      // 全0
    test_hamming(0xFFFFFFFF);      // 全1
    test_hamming(0x12345678);      // 随机模式
    test_hamming(0xAAAAAAAA);      // 交替模式
    return 0;
}