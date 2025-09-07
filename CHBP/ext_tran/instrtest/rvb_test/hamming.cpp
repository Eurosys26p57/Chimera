#include <stdio.h>
#include <stdint.h>
#include <time.h>

// ��ͳ������ѭ��ͳ��1��λ��
int hamming_weight_loop(uint32_t x) {
    int count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

// B��չ������ʹ��cpopָ��
int hamming_weight_b(uint32_t x) {
    uint64_t x_zext = (uint64_t)x; // ����չ��64λ
    int count;
    __asm__ volatile (
        "cpop %0, %1"
        : "=r" (count)
        : "r" (x_zext)
    );
    return count;
}

// ���Ժ���
void test_hamming(uint32_t x) {
    struct timespec start, end;
    int loop_result, b_result;
    double time_loop, time_b;

    // ���Դ�ͳ����
    clock_gettime(CLOCK_MONOTONIC, &start);
    loop_result = hamming_weight_loop(x);
    clock_gettime(CLOCK_MONOTONIC, &end);
    time_loop = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);

    // ����B��չ����
    clock_gettime(CLOCK_MONOTONIC, &start);
    b_result = hamming_weight_b(x);
    clock_gettime(CLOCK_MONOTONIC, &end);
    time_b = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);

    // ������
    printf("Value: 0x%08x\n", x);
    printf("Loop result: %d (%.2f ns)\n", loop_result, time_loop);
    printf("B-ext result: %d (%.2f ns)\n", b_result, time_b);
    printf("Speedup: %.2fx\n\n", time_loop / time_b);
}

int main() {
    // ���Բ�ͬ����ģʽ
    test_hamming(0x00000000);      // ȫ0
    test_hamming(0xFFFFFFFF);      // ȫ1
    test_hamming(0x12345678);      // ���ģʽ
    test_hamming(0xAAAAAAAA);      // ����ģʽ
    return 0;
}