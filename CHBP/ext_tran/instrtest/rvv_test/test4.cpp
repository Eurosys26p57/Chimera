// // add total time and enable xiugai matrix size
// half core execute matrix multiplication using RVV
// matrix args can be modified by command line args
// fibonacci_n can be modified by command line args
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <random>
#include <sched.h>
#include <pthread.h>

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

// 矩阵乘法（使用RVV向量扩展）
void matrix_mul_vector(int32** A, int32** B, int32** C, int N) {
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
    if (argc < 6) {
        std::cout << "Usage: " << argv[0] << " <experiment> <NUM_TASKS> <matrix_ratio (for experiment1)> <N (matrix size)> <fib_n>" << std::endl;
        return 1;
    }
    int experiment = std::atoi(argv[1]);
    int NUM_TASKS = std::atoi(argv[2]);
    double matrix_ratio = std::atof(argv[3]);
    int N = std::atoi(argv[4]);
    int fib_n = std::atoi(argv[5]);

    double total_fib_time = 0.0;
    double total_matrix_time = 0.0;

    auto overall_start = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    unsigned int num_cores = std::thread::hardware_concurrency();
    unsigned int half_cores = num_cores / 2;

    std::uniform_real_distribution<> dis_ratio(0.0, 1.0);

    for (int i = 0; i < NUM_TASKS; ++i) {
        int core_id = i % num_cores;
        threads.emplace_back([i, core_id, &total_fib_time, &total_matrix_time, half_cores, experiment, matrix_ratio, &dis_ratio, N, fib_n]() {
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(core_id, &cpuset);
            pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

            auto start = std::chrono::steady_clock::now();
            if (experiment == 1) {
                double r = dis_ratio(gen);
                if (r < matrix_ratio) {
                    int32** A = new int32*[N];
                    int32** B = new int32*[N];
                    int32** C = new int32*[N];
                    for (int x = 0; x < N; ++x) {
                        A[x] = new int32[N];
                        B[x] = new int32[N];
                        C[x] = new int32[N]();
                        for (int y = 0; y < N; ++y) {
                            A[x][y] = dis_int(gen);
                            B[x][y] = dis_int(gen);
                        }
                    }
                    matrix_mul_vector(A, B, C, N);
                    for (int x = 0; x < N; ++x) {
                        delete[] A[x]; delete[] B[x]; delete[] C[x];
                    }
                    delete[] A; delete[] B; delete[] C;
                } else {
                    fibonacci_iterative(fib_n);
                }
            } else if (experiment == 2) {
                if (core_id < static_cast<int>(half_cores)) {
                    int32** A = new int32*[N];
                    int32** B = new int32*[N];
                    int32** C = new int32*[N];
                    for (int x = 0; x < N; ++x) {
                        A[x] = new int32[N];
                        B[x] = new int32[N];
                        C[x] = new int32[N]();
                        for (int y = 0; y < N; ++y) {
                            A[x][y] = dis_int(gen);
                            B[x][y] = dis_int(gen);
                        }
                    }
                    matrix_mul_vector(A, B, C, N);
                    for (int x = 0; x < N; ++x) {
                        delete[] A[x]; delete[] B[x]; delete[] C[x];
                    }
                    delete[] A; delete[] B; delete[] C;
                } else {
                    fibonacci_iterative(fib_n);
                }
            }
            auto end = std::chrono::steady_clock::now();
            std::chrono::duration<double, std::milli> duration = end - start;
            if (experiment == 1 || core_id < static_cast<int>(half_cores)) {
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
