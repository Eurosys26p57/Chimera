#include <stdio.h>

#define N 4


void matrix_mul_vector(long A[N][N], long B[N][N], long C[N][N]) {
    size_t vl = __builtin_epi_vsetvl(N, __epi_e64, __epi_m1);

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            __epi_1xi64 vec_c = __builtin_epi_vbroadcast_1xi64(0, vl);
            for (int k = 0; k < N; k++) {
                __epi_1xi64 vec_a = __builtin_epi_vload_1xi64(&A[i][k], vl);
                __epi_1xi64 vec_b = __builtin_epi_vload_1xi64(&B[k][j], vl);
                vec_c = __builtin_epi_vadd_1xi64(vec_c, __builtin_epi_vmul_1xi64(vec_a, vec_b, vl), vl);
            }
            __builtin_epi_vstore_1xi64(&C[i][j], vec_c, vl);
        }
    }
}

int main() {
    long A[N][N] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};
    long B[N][N] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    long C[N][N];

    matrix_mul_vector(A, B, C);


    printf("Matrix multiplication (vector) result:\n");
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            printf("%ld ", C[i][j]);
        }
        printf("\n");
    }

    return 0;
}