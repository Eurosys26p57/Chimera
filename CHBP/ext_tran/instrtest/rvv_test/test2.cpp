// add total time and enable xiugai matrix size
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <random>
#include <sched.h>
#include <pthread.h>

#define N 128 // 修改为动态调整的矩阵大小
typedef int int32;

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> dis_int(1, 10);

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

// 矩阵乘法（普通版，不使用RVV）
void matrix_multiply(std::vector<std::vector<int>>& A,
                     std::vector<std::vector<int>>& B,
                     std::vector<std::vector<int>>& C) {
    int size = A.size();
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            C[i][j] = 0;
            for (int k = 0; k < size; ++k) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

// 矩阵乘法（使用RVV向量扩展）
void matrix_mul_vector(int32 A[N][N], int32 B[N][N], int32 C[N][N]) {
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

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <experiment> <NUM_TASKS> [matrix_ratio]" << std::endl;
        return 1;
    }
    int experiment = std::atoi(argv[1]);
    int NUM_TASKS = std::atoi(argv[2]);
    double matrix_ratio = (argc >= 4) ? std::atof(argv[3]) : 0.5;

    double total_fib_time = 0.0;
    double total_matrix_time = 0.0;

    auto overall_start = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_TASKS; ++i) {
        double r = std::uniform_real_distribution<>(0.0, 1.0)(gen);
        threads.emplace_back([i, &total_fib_time, &total_matrix_time, r, matrix_ratio]() {
            auto start = std::chrono::steady_clock::now();
            if (r < matrix_ratio) {
                int32 A[N][N], B[N][N], C[N][N] = {0};
                for (int x = 0; x < N; ++x) {
                    for (int y = 0; y < N; ++y) {
                        A[x][y] = dis_int(gen);
                        B[x][y] = dis_int(gen);
                    }
                }
                matrix_mul_vector(A, B, C);
            } else {
                fibonacci_iterative(40);
            }
            auto end = std::chrono::steady_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;
            if (r < matrix_ratio) {
                total_matrix_time += duration.count();
            } else {
                total_fib_time += duration.count();
            }
        });
    }
    for (auto &t : threads) t.join();
    auto overall_end = std::chrono::steady_clock::now();

    double overall_time = std::chrono::duration<double, std::milli>(overall_end - overall_start).count();
    std::cout << "Total Fibonacci Time: " << total_fib_time << " ms" << std::endl;
    std::cout << "Total Matrix Multiplication Time: " << total_matrix_time << " ms" << std::endl;
    std::cout << "Overall Experiment Time (End-to-End Delay): " << overall_time << " ms" << std::endl;
    return 0;
}
