#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <random>
#include <sched.h>
#include <pthread.h>
#include <memory>
#include <functional> // 添加头文件

using namespace std;
constexpr int N = 64;
using int32 = int;

// ==================== 全局统计 ====================
enum TaskType { FIB, VEC_MATRIX, SCL_MATRIX };
double fib_time = 0, vec_mat_time = 0, scl_mat_time = 0; // 改为普通变量
mutex stats_mutex; // 保护统计变量

namespace Utils {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(1, 100);

    long long fib(int n) {
        if (n <= 1) return n;
        long long a = 0, b = 1;
        for (int i = 2; i <= n; ++i) {
            long long temp = a + b;
            a = b;
            b = temp;
        }
        return b;
    }

    void scalar_mult(const std::vector<std::vector<int>>& A,
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

    // 矩阵乘法（RVV扩展示例）
    void vector_mult(int32 A[N][N], int32 B[N][N], int32 C[N][N]) {
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
}

// ==================== 调度策略 ====================
class SchedulingStrategy {
public:
    virtual bool canSteal(atomic_int& self, atomic_int& other) = 0; // 修改参数类型
    virtual ~SchedulingStrategy() = default;
};

class StrictStrategy : public SchedulingStrategy {
public:
    bool canSteal(atomic_int&, atomic_int&) override { return false; }
};

class VectorStealStrategy : public SchedulingStrategy {
public:
    bool canSteal(atomic_int& self, atomic_int& other) override {
        return (self.load() == 0) && (other.load() > 0); // 判断条件调整
    }
};

class BidirectionalStealStrategy : public SchedulingStrategy {
public:
    bool canSteal(atomic_int& self, atomic_int& other) override {
        // 允许窃取条件：自身空闲（self=0）且对方有任务（other>0）
        return (self.load() == 0) && (other.load() > 0);
    }
};

// ==================== 线程池实现 ====================
class ThreadPool {
protected:
    vector<thread> workers;
    queue<function<void()>> tasks;
    mutex q_mutex;
    condition_variable cv;
    atomic_bool stop{false};
    atomic_int active{0}; // 保持atomic_int类型
    unique_ptr<SchedulingStrategy> strategy;
    ThreadPool* paired_pool = nullptr;
    bool is_vector_pool;  // 标记是否为向量核心池
    int core_base;

    void setAffinity(int base) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for(int i=0; i<2; ++i) CPU_SET(base+i, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    }

public:
    ThreadPool(int cores, int base, bool is_vector) 
        : core_base(base), is_vector_pool(is_vector) {
        for(int i=0; i<cores; ++i) {
            workers.emplace_back([this] {
                setAffinity(core_base);
                while(!stop) {
                    function<void()> task;
                    if(getTask(task) || trySteal(task)) {
                        active++;
                        task();
                        active--;
                    } else {
                        unique_lock<mutex> lock(q_mutex);
                        cv.wait_for(lock, std::chrono::milliseconds(100), [this]{ return !tasks.empty() || stop; }); // 修改时间
                    }
                }
            });
        }
    }
    
    //
    ~ThreadPool() { 
        cout << "Destroying ThreadPool..." << endl;
        stopPool(); 
    }
    //

    void configure(unique_ptr<SchedulingStrategy> s, ThreadPool* p = nullptr) {
        strategy = move(s);
        paired_pool = p;
    }

    template<class F>
    void submit(F&& task) {
        {
            lock_guard<mutex> lock(q_mutex);
            tasks.emplace(forward<F>(task));
        }
        cv.notify_one();
    }

    void stopPool() {
        {
            lock_guard<mutex> lock(q_mutex);
            stop = true;
        }
        cv.notify_all(); // 强制唤醒所有线程
        for (auto& t : workers) {
            if (t.joinable()) t.join();
        }
    }

private:
    bool getTask(function<void()>& task) {
        lock_guard<mutex> lock(q_mutex);
        if(tasks.empty()) return false;
        task = move(tasks.front());
        tasks.pop();
        return true;
    }

    bool trySteal(function<void()>& task) {
        if (!paired_pool || !strategy) return false;
        if (!strategy->canSteal(active, paired_pool->active)) return false;

        // 窃取任务并转换类型
        lock_guard<mutex> lock(paired_pool->q_mutex);
        if (paired_pool->tasks.empty()) return false;

        auto stolen_task = move(paired_pool->tasks.front());
        paired_pool->tasks.pop();

        // 标量池窃取向量任务：转换为标量矩阵乘法
        if (!is_vector_pool && stolen_task.type == VEC_MATRIX) {
            task = [this]() {
                auto start = chrono::high_resolution_clock::now();
                vector<vector<int>> A(N, vector<int>(N)), B(N, vector<int>(N)), C(N, vector<int>(N));
                // 生成随机矩阵
                for (int i=0; i<N; ++i) {
                    for (int j=0; j<N; ++j) {
                        A[i][j] = Utils::dis(Utils::gen);
                        B[i][j] = Utils::dis(Utils::gen);
                    }
                }
                Utils::scalar_mult(A, B, C);
                auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
                {
                    lock_guard<mutex> lock(stats_mutex);
                    scl_mat_time += dur.count();
                }
                completed++;
            };
        } 
        // 向量池窃取标量任务：直接执行斐波那契
        else if (is_vector_pool && stolen_task.type == FIB) {
            task = [this]() {
                auto start = chrono::high_resolution_clock::now();
                Utils::fib(40);
                auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
                {
                    lock_guard<mutex> lock(stats_mutex);
                    fib_time += dur.count();
                }
                completed++;
            };
        } else {
            task = move(stolen_task);
        }

        return true;
    }
};

// ==================== 实验模块 ====================
class Experiment {
protected:
    unique_ptr<ThreadPool> scalar_pool;
    unique_ptr<ThreadPool> vector_pool;
    atomic<int> completed{0};
    int total_tasks;
    float ratio;

    void submitTask(TaskType type) {
        auto wrapper = [=] {
            auto start = chrono::high_resolution_clock::now();
            executeTask(type);
            auto dur = chrono::duration<double, milli>(
                chrono::high_resolution_clock::now() - start);
            updateStats(type, dur.count());
            completed++;
        };

        if(type == FIB) scalar_pool->submit(wrapper);
        else vector_pool->submit(wrapper);
    }

    virtual void executeTask(TaskType type) {
        switch(type) {
            case FIB: Utils::fib(40); break;
            case VEC_MATRIX: {
                int32 A[N][N], B[N][N], C[N][N];
                generateMatrix(A); generateMatrix(B);
                Utils::vector_mult(A, B, C); // 修正参数顺序
                break;
            }
            case SCL_MATRIX: {
                vector<vector<int>> A(N, vector<int>(N));
                vector<vector<int>> B(N, vector<int>(N));
                vector<vector<int>> C(N, vector<int>(N));
                Utils::scalar_mult(A, B, C); // 修正参数顺序
                break;
            }
        }
    }

    void generateMatrix(int32 m[N][N]) {
        for(int i=0; i<N; ++i)
            for(int j=0; j<N; ++j)
                m[i][j] = Utils::dis(Utils::gen);
    }

    void updateStats(TaskType type, double time) {
        lock_guard<mutex> lock(stats_mutex); // 加锁保护
        switch(type) {
            case FIB: fib_time += time; break;
            case VEC_MATRIX: vec_mat_time += time; break;
            case SCL_MATRIX: scl_mat_time += time; break;
        }
    }

public:
    Experiment(int t, float r) : total_tasks(t), ratio(r) {}
    virtual void run() = 0;
    
    ~Experiment() {
        cout << "Stopping scalar pool..." << endl;
        scalar_pool->stopPool();
        cout << "Stopping vector pool..." << endl;
        vector_pool->stopPool();
    }
    
    
};

// 实验1实现
class Exp1 : public Experiment {
public:
    Exp1(int t, float r) : Experiment(t, r) {
        scalar_pool.reset(new ThreadPool(2, 0)); // 替换make_unique
        vector_pool.reset(new ThreadPool(2, 2));
        scalar_pool->configure(unique_ptr<SchedulingStrategy>(new StrictStrategy()));
        vector_pool->configure(unique_ptr<SchedulingStrategy>(new StrictStrategy()));
    }

    void run() override {
        for(int i=0; i<total_tasks; ++i) {
            (rand()%100 < ratio*100) ? submitTask(VEC_MATRIX) : submitTask(FIB);
        }
        while(completed < total_tasks) this_thread::sleep_for(std::chrono::milliseconds(100)); // 修改时间
        
    }
};

// 实验2实现
class Exp2 : public Experiment {
public:
    Exp2(int t, float r) : Experiment(t, r) {
        scalar_pool.reset(new ThreadPool(2, 0));
        vector_pool.reset(new ThreadPool(2, 2));
        vector_pool->configure(unique_ptr<SchedulingStrategy>(new VectorStealStrategy()), scalar_pool.get());
    }

    void run() override {
        for (int i=0; i<total_tasks; ++i) {
            bool is_vec = (rand()%100 < ratio*100);
            if (is_vec) submitTask(VEC_MATRIX); // 调用基类方法
            else submitTask(FIB); // 调用基类方法
        }
        while (completed < total_tasks) 
            this_thread::sleep_for(10ms);
    }
};

// 实验3实现
class Exp3 : public Experiment {
public:
    Exp3(int t, float r) : Experiment(t, r) {
        // 标量池（CPU 0-1），向量池（CPU 2-3）
        scalar_pool.reset(new ThreadPool(2, 0, false));
        vector_pool.reset(new ThreadPool(2, 2, true));
        
        // 配置双向窃取策略
        scalar_pool->configure(make_unique<BidirectionalStealStrategy>(), vector_pool.get());
        vector_pool->configure(make_unique<BidirectionalStealStrategy>(), scalar_pool.get());
    }

    void run() override {
        for (int i=0; i<total_tasks; ++i) {
            bool is_vec = (rand() % 100 < ratio * 100);
            if (is_vec) {
                submitTask(VEC_MATRIX); // 提交到向量池
            } else {
                submitTask(FIB);        // 提交到标量池
            }
        }
        while (completed < total_tasks) {
            this_thread::sleep_for(10ms);
        }
    }

private:
    void submitTask(TaskType type) override {
        auto task_wrapper = [this, type]() {
            auto start = chrono::high_resolution_clock::now();
            if (type == FIB) {
                Utils::fib(40);
            } else if (type == VEC_MATRIX) {
                int32 A[N][N], B[N][N], C[N][N];
                generateMatrix(A);
                generateMatrix(B);
                Utils::vector_mult(A, B, C);
            }
            auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
            {
                lock_guard<mutex> lock(stats_mutex);
                switch (type) {
                    case FIB: fib_time += dur.count(); break;
                    case VEC_MATRIX: vec_mat_time += dur.count(); break;
                    default: break;
                }
            }
            completed++;
        };

        if (type == FIB) {
            scalar_pool->submit(task_wrapper);
        } else {
            vector_pool->submit(task_wrapper);
        }
    }

    void generateMatrix(int32 m[N][N]) {
        for (int i=0; i<N; ++i) {
            for (int j=0; j<N; ++j) {
                m[i][j] = Utils::dis(Utils::gen);
            }
        }
    }
};

// ==================== 主函数 ====================
int main() {
    // 实验1运行
    {
        auto start = chrono::high_resolution_clock::now();
        Exp1(1000, 0.6).run();
        auto dur = chrono::duration<double, milli>(
            chrono::high_resolution_clock::now() - start);
        
        cout << "=== Exp 1 result ===" << endl
             << "fib time: " << fib_time << "ms" << endl
             << "rvv matrix_mul time: " << vec_mat_time << "ms" << endl
             << "total: " << dur.count() << "ms" << endl << endl;
    }

    // 重置统计
    {
        lock_guard<mutex> lock(stats_mutex);
        fib_time = vec_mat_time = scl_mat_time = 0;
    }

    // 实验2运行
    {
        auto start = chrono::high_resolution_clock::now();
        Exp2(1000, 0.6).run();
        auto dur = chrono::duration<double, milli>(
            chrono::high_resolution_clock::now() - start);
        
        cout << "=== Exp 2 result ===" << endl
             << "fib time:  " << fib_time << "ms" << endl
             << "rvv matrix_mul time: " << vec_mat_time << "ms" << endl
             << "total: " << dur.count() << "ms" << endl;
    }
    
    // 实验3运行
    {
        auto start = chrono::high_resolution_clock::now();
        Exp3 exp3(1000, 0.6);
        exp3.run();
        auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
        
        cout << "=== Exp 3 result ===" << endl
             << "fib time:  " << fib_time << "ms" << endl
             << "rvv matrix_mul time: " << vec_mat_time << "ms" << endl
             << "scalar matrix_mul time: " << scl_mat_time << "ms" << endl
             << "total: " << dur.count() << "ms" << endl;
    }

    return 0;
}