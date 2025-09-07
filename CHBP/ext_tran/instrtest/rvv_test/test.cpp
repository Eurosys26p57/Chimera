#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <random>
#include <sched.h>
#include <pthread.h>

#define N 4
typedef int int32;

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

// 矩阵乘法（使用RVV向量扩展，RVV 0.7版本，数据类型为int）
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

// 任务包装函数：斐波那契任务
void run_fibonacci_task(int n, int task_id, std::vector<double>& task_times) {
    auto start = std::chrono::steady_clock::now();
    long long result = fibonacci_iterative(n);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    task_times[task_id] = duration.count();
    std::cout << "Task " << task_id << " (Fibonacci): result = "
              << result << ", time = " << duration.count() << " ms" << std::endl;
}

// 任务包装函数：普通矩阵乘法任务（使用std::vector）
void run_matrix_multiply_task(const std::vector<std::vector<int>>& A,
                              const std::vector<std::vector<int>>& B,
                              std::vector<std::vector<int>>& C,
                              int task_id,
                              std::vector<double>& task_times) {
    auto start = std::chrono::steady_clock::now();
    matrix_multiply(A, B, C);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    task_times[task_id] = duration.count();
    std::cout << "Task " << task_id << " (Matrix Multiply): time = "
              << duration.count() << " ms" << std::endl;
}

// 任务包装函数：RVV矩阵乘法任务
void run_matrix_mul_vector_task(int32 A[N][N], int32 B[N][N], int32 C[N][N],
                                int task_id,
                                std::vector<double>& task_times) {
    auto start = std::chrono::steady_clock::now();
    matrix_mul_vector(A, B, C);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    task_times[task_id] = duration.count();
    std::cout << "Task " << task_id << " (Matrix Multiply RVV): time = "
              << duration.count() << " ms" << std::endl;
}

int main(int argc, char* argv[]) {
    // 参数解析：experiment类型, NUM_TASKS, [matrix_ratio for experiment1]
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <experiment> <NUM_TASKS> [matrix_ratio (for experiment1)]" << std::endl;
        return 1;
    }
    int experiment = std::atoi(argv[1]);   // 1 或 2
    int NUM_TASKS = std::atoi(argv[2]);
    double matrix_ratio = 0.0;
    if (experiment == 1) {
        if (argc < 4) {
            std::cout << "For experiment1, please provide matrix_ratio (0.0 - 1.0)" << std::endl;
            return 1;
        }
        matrix_ratio = std::atof(argv[3]);
    }

    auto overall_start = std::chrono::steady_clock::now();

    std::vector<double> task_times(NUM_TASKS, 0.0);
    std::vector<std::thread> threads;

    if (experiment == 1) {
        // 实验1：所有任务均在所有核心上运行，根据 matrix_ratio 随机选择任务类型
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, 1.0);
        for (int i = 0; i < NUM_TASKS; ++i) {
            double r = dis(gen);
            if (r < matrix_ratio) {
                // 普通矩阵乘法任务（不使用RVV）
                threads.emplace_back([i, &task_times]() {
                    std::vector<std::vector<int>> A = {
                        {1, 2, 3, 4},
                        {5, 6, 7, 8},
                        {9, 10, 11, 12},
                        {13, 14, 15, 16}
                    };
                    std::vector<std::vector<int>> B = {
                        {1, 0, 0, 0},
                        {0, 1, 0, 0},
                        {0, 0, 1, 0},
                        {0, 0, 0, 1}
                    };
                    std::vector<std::vector<int>> C(4, std::vector<int>(4, 0));
                    run_matrix_multiply_task(A, B, C, i, task_times);
                });
            } else {
                // 斐波那契任务
                threads.emplace_back([i, &task_times]() {
                    int n = 40;  // 可根据需求调整 n 的值
                    run_fibonacci_task(n, i, task_times);
                });
            }
        }
    } else if (experiment == 2) {
        // 实验2：一半核心执行 RVV 矩阵乘法任务，另一半核心执行斐波那契任务
        unsigned int num_cores = std::thread::hardware_concurrency();
        unsigned int half_cores = num_cores / 2;
        for (int i = 0; i < NUM_TASKS; ++i) {
            int core_id = i % num_cores;
            if (core_id < static_cast<int>(half_cores)) {
                // RVV矩阵乘法任务，设置线程亲和到 core_id
                threads.emplace_back([i, core_id, &task_times]() {
                    cpu_set_t cpuset;
                    CPU_ZERO(&cpuset);
                    CPU_SET(core_id, &cpuset);
                    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

                    int32 A_rvv[N][N] = {
                        {1, 2, 3, 4},
                        {5, 6, 7, 8},
                        {9, 10, 11, 12},
                        {13, 14, 15, 16}
                    };
                    int32 B_rvv[N][N] = {
                        {1, 0, 0, 0},
                        {0, 1, 0, 0},
                        {0, 0, 1, 0},
                        {0, 0, 0, 1}
                    };
                    int32 C_rvv[N][N] = {0};
                    run_matrix_mul_vector_task(A_rvv, B_rvv, C_rvv, i, task_times);
                });
            } else {
                // 斐波那契任务，设置线程亲和到 core_id
                threads.emplace_back([i, core_id, &task_times]() {
                    cpu_set_t cpuset;
                    CPU_ZERO(&cpuset);
                    CPU_SET(core_id, &cpuset);
                    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

                    int n = 40;
                    run_fibonacci_task(n, i, task_times);
                });
            }
        }
    } else {
        std::cerr << "Invalid experiment number." << std::endl;
        return 1;
    }

    // 等待所有任务完成
    for (auto &t : threads) {
        t.join();
    }

    auto overall_end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> overall_duration = overall_end - overall_start;
    std::cout << "Overall experiment time: " << overall_duration.count() << " ms" << std::endl;

    return 0;
}
