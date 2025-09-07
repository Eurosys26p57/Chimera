//- 矩阵乘分成两个线程池。
//  - 第一个版本：（标量和向量，分别在各自的核心执行）
//  - 2：向量核心空闲时可取标量任务，反之不行
//  - 3：向量核心空闲时可取标量任务，标量核心空闲时可取向量任务的标量版本
//  - 整体时间控制在最慢端到端10s
//- 斐波那契和矩阵乘的时间控制的差不多。
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <random>
#include <sched.h>
#include <pthread.h>
#include <atomic>

#define N 64  // 可修改的矩阵大小
typedef int int32;

// 全局变量用于统计总时间
std::atomic<double> fib_total_time(0.0);
std::atomic<double> matrix_total_time(0.0);
std::atomic<double> rvv_matrix_total_time(0.0);

// 随机数生成器
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> dis(1, 100);

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

// 矩阵乘法（普通版）
void matrix_multiply(const std::vector<std::vector<int>>& A,
                     const std::vector<std::vector<int>>& B,
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

// 矩阵乘法（RVV向量扩展版）
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

// 生成随机矩阵（普通版）
void fill_matrix(std::vector<std::vector<int>>& matrix) {
    for (auto& row : matrix) {
        for (auto& elem : row) {
            elem = dis(gen);
        }
    }
}

// 生成随机矩阵（RVV版）
void fill_array(int32 matrix[N][N]) {
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            matrix[i][j] = dis(gen);
        }
    }
}

// 任务包装函数：斐波那契
void run_fibonacci_task(int n, int task_id) {
    auto start = std::chrono::steady_clock::now();
    long long result = fibonacci_iterative(n);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    fib_total_time = fib_total_time + duration.count();
//    std::cout << "Task " << task_id << " (Fibonacci): result = "
//              << result << ", time = " << duration.count() << " ms" << std::endl;
}

// 任务包装函数：普通矩阵乘法
void run_matrix_multiply_task(int task_id) {
    std::vector<std::vector<int>> A(N, std::vector<int>(N));
    std::vector<std::vector<int>> B(N, std::vector<int>(N));
    std::vector<std::vector<int>> C(N, std::vector<int>(N));
    fill_matrix(A);
    fill_matrix(B);

    auto start = std::chrono::steady_clock::now();
    matrix_multiply(A, B, C);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    matrix_total_time = matrix_total_time + duration.count();
    // std::cout << "Task " << task_id << " (Matrix Multiply): time = "
    //          << duration.count() << " ms" << std::endl;
}

// 任务包装函数：RVV矩阵乘法
void run_matrix_mul_vector_task(int task_id) {
    int32 A[N][N];
    int32 B[N][N];
    int32 C[N][N];
    fill_array(A);
    fill_array(B);

    auto start = std::chrono::steady_clock::now();
    matrix_mul_vector(A, B, C);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    rvv_matrix_total_time = rvv_matrix_total_time + duration.count();
    std::cout << "Task " << task_id << " (RVV Matrix): time = "
              << duration.count() << " ms" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <experiment> <NUM_TASKS> [matrix_ratio]" << std::endl;
        return 1;
    }

    int experiment = std::atoi(argv[1]);
    int NUM_TASKS = std::atoi(argv[2]);
    double matrix_ratio = 0.0;
    if (experiment == 1 && argc < 4) {
        std::cout << "For experiment1, please provide matrix_ratio" << std::endl;
        return 1;
    }
    if (experiment == 1) matrix_ratio = std::atof(argv[3]);

    std::vector<std::thread> threads;
    auto overall_start = std::chrono::steady_clock::now();

    if (experiment == 1) {
        std::uniform_real_distribution<> ratio_dis(0.0, 1.0);
        for (int i = 0; i < NUM_TASKS; ++i) {
            if (ratio_dis(gen) < matrix_ratio) {
                threads.emplace_back([i]() { run_matrix_multiply_task(i); });
            } else {
                threads.emplace_back([i]() { run_fibonacci_task(40, i); });
            }
        }
    } else if (experiment == 2) {
        unsigned num_cores = std::thread::hardware_concurrency();
        unsigned half_cores = num_cores / 2;
        for (int i = 0; i < NUM_TASKS; ++i) {
            if (i % num_cores < half_cores) {
                threads.emplace_back([i]() { run_matrix_mul_vector_task(i); });
            } else {
                threads.emplace_back([i]() { run_fibonacci_task(40, i); });
            }
        }
    }

    for (auto& t : threads) t.join();
    auto overall_end = std::chrono::steady_clock::now();

    double overall_time = std::chrono::duration<double, std::milli>(overall_end - overall_start).count();


    // 输出统计结果
    std::cout << "\n============= Statistics =============" << std::endl;
    std::cout << "Total Fibonacci time: " << fib_total_time << " ms" << std::endl;
    if (experiment == 1) {
        std::cout << "Total Matrix Multiply time: " << matrix_total_time << " ms" << std::endl;
    } else {
        std::cout << "Total RVV Matrix time: " << rvv_matrix_total_time << " ms" << std::endl;
    }
    std::cout << "Overall Experiment Time (End-to-End Delay): " << overall_time << " ms" << std::endl;
    std::cout << "=======================================" << std::endl;

    return 0;
}