#include <iostream>
#include <vector>
#include <chrono>
#include <random>

// 定义矩阵大小
const int N = 42; // 可以根据需要调整矩阵大小
const int N_SCL = 32;
// 全局变量用于存储总时间
double fib_total_time = 0.0;
double matrix_total_time = 0.0;
double rvv_matrix_total_time = 0.0;

// 随机数生成器
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> dis(0, 100);

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

//矩阵乘法（RVV向量扩展版）
void matrix_mul_vector(int32_t A[N][N], int32_t B[N][N], int32_t C[N][N]) {
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

//void matrix_mul_vector(int32_t A[N][N], int32_t B[N][N], int32_t C[N][N]) {
//    size_t vl = __builtin_epi_vsetvl(N, __epi_e32, __epi_m1);
//    for (int i = 0; i < N; i++) {
//        for (int j = 0; j < N; j++) {
//            __epi_i32 vec_c = __builtin_epi_vbroadcast_i32(0, vl);
//            for (int k = 0; k < N; k++) {
//                __epi_i32 vec_a = __builtin_epi_vload_i32(&A[i][k], vl);
//                __epi_i32 vec_b = __builtin_epi_vload_i32(&B[k][j], vl);
//                vec_c = __builtin_epi_vadd_i32(vec_c, __builtin_epi_vmul_i32(vec_a, vec_b, vl), vl);
//            }
//            __builtin_epi_vstore_i32(&C[i][j], vec_c, vl);
//        }
//    }
//}

// 生成随机矩阵（普通版）
void fill_matrix(std::vector<std::vector<int>>& matrix) {
    for (auto& row : matrix) {
        for (auto& elem : row) {
            elem = dis(gen);
        }
    }
}

// 生成随机矩阵（RVV版）
void fill_array(int32_t matrix[N][N]) {
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
    std::vector<std::vector<int>> A(N_SCL, std::vector<int>(N_SCL));
    std::vector<std::vector<int>> B(N_SCL, std::vector<int>(N_SCL));
    std::vector<std::vector<int>> C(N_SCL, std::vector<int>(N_SCL));
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
    int32_t A[N][N];
    int32_t B[N][N];
    int32_t C[N][N];
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

int main() {
    const int num_tasks = 1000; // 执行任务的次数

    // 执行普通矩阵乘法任务
    for (int i = 0; i < num_tasks; ++i) {
        run_matrix_multiply_task(i);
    }

    // 执行RVV矩阵乘法任务
    for (int i = 0; i < num_tasks; ++i) {
        run_matrix_mul_vector_task(i);
    }

    // 计算平均时间
    double avg_matrix_time = matrix_total_time / num_tasks;
    double avg_rvv_matrix_time = rvv_matrix_total_time / num_tasks;

    // 输出结果
    std::cout << "Average time for normal matrix multiplication: " << avg_matrix_time << " ms" << std::endl;
    std::cout << "Average time for RVV matrix multiplication: " << avg_rvv_matrix_time << " ms" << std::endl;

    // 计算性能提升百分比
    double speedup = (avg_matrix_time - avg_rvv_matrix_time) / avg_matrix_time * 100;
    std::cout << "Speedup: " << speedup << "%" << std::endl;

    return 0;
}
