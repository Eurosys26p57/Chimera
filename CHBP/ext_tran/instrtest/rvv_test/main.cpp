#include <iostream>
#include <vector>

#define N 4

// 迭代计算斐波那契数（高效）
long long fibonacci_iterative(int n) {
    if (n <= 1) return n;
    long long a = 0, b = 1;
    for (int i = 2; i <= n; ++i) {
        long long temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}

// 矩阵乘法（普通版，不使用RISC-V向量扩展）
void matrix_multiply(const std::vector<std::vector<int>>& A,
                     const std::vector<std::vector<int>>& B,
                     std::vector<std::vector<int>>& C) {
    int n = A.size();
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            C[i][j] = 0;
            for (int k = 0; k < n; ++k) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

// 矩阵乘法（使用RISC-V向量扩展RVV）
void matrix_mul_vector_rvv(int A[N][N], int B[N][N], int C[N][N]) {
    size_t vl = __builtin_epi_vsetvl(N, __epi_e32, __epi_m1);

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            __epi_2xi32 vec_c = __builtin_epi_vbroadcast_2xi32(0, vl);
            for (int k = 0; k < N; k++) {
                __epi_2xi32 vec_a = __builtin_epi_vload_2xi32(&A[i][k], vl);
                __epi_2xi32 vec_b = __builtin_epi_vload_2xi32(&B[k][j], vl);
                vec_c = __builtin_epi_vadd_2xi32(vec_c, __builtin_epi_vmul_2xi32(vec_a, vec_b, vl), vl);
            }
            __builtin_epi_vstore_2xi32(&C[i][j], vec_c, vl);
        }
    }
}

int main() {
    // 测试斐波那契函数
    int fib_n = 10;
    std::cout << "Fibonacci(" << fib_n << ") = " << fibonacci_iterative(fib_n) << std::endl;

    // 测试普通矩阵乘法
    std::vector<std::vector<int>> A = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};
    std::vector<std::vector<int>> B = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    std::vector<std::vector<int>> C(4, std::vector<int>(4, 0));

    matrix_multiply(A, B, C);
    std::cout << "Matrix multiplication result (normal):\n";
    for (const auto& row : C) {
        for (int val : row) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }

    // 测试RVV矩阵乘法
    int A_rvv[N][N] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};
    int B_rvv[N][N] = {{1, 1, 0, 0}, {0, 1, 0, 1}, {1, 0, 1, 0}, {1, 1, 0, 1}};
    int C_rvv[N][N] = {0};

    matrix_mul_vector_rvv(A_rvv, B_rvv, C_rvv);
    std::cout << "Matrix multiplication result (RVV):\n";
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            std::cout << C_rvv[i][j] << " ";
        }
        std::cout << std::endl;
    }

    return 0;
}

