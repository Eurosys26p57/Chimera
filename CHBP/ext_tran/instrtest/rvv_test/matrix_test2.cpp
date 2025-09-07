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
#include <functional>
#include <utility> // for std::pair
#include <iomanip>



using namespace std;
constexpr int N = 42;
constexpr int N_SCL = 32;


using int32 = int;

enum TaskType { FIB, VEC_MATRIX, SCL_MATRIX };
double fib_time = 0, vec_mat_time = 0, scl_mat_time = 0;
double vec_fib_time = 0;    // 向量线程池执行斐波那契的时间
double vec_pool_native_scl = 0;    // 向量池原生标量矩阵乘
double scl_pool_stolen_scl = 0;    // 标量池窃取的标量矩阵乘
double matrix_count = 0;
double sclmat_count = 0;
mutex stats_mutex;
class Experiment;


namespace Utils {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(1, 100);

    long long fib(long long n) {
        if (n <= 1) return n;
        long long a = 0, b = 1, c;
        for (long long i = 2; i <= n; ++i) {
            c = a + b;
            a = b;
            b = c;
        }
        return b;
    }

    void scalar_mult(const vector<vector<int>>& A,
                     const vector<vector<int>>& B,
                     vector<vector<int>>& C) {
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

class SchedulingStrategy {
public:
    virtual bool canSteal(atomic_int& self, atomic_int& other) = 0;
    virtual ~SchedulingStrategy() = default;
};

class StrictStrategy : public SchedulingStrategy {
public:
    bool canSteal(atomic_int&, atomic_int&) override { return false; }
};

class VectorStealStrategy : public SchedulingStrategy {
public:
    bool canSteal(atomic_int& self, atomic_int& other) override {
        return (self.load() == 0) && (other.load() > 0);
    }
};

class BidirectionalStealStrategy : public SchedulingStrategy {
public:
    bool canSteal(atomic_int& self, atomic_int& other) override {
        return (self.load() == 0) && (other.load() > 0);
    }
};

class ThreadPool {
protected:
    vector<thread> workers;
    queue<pair<TaskType, function<void()>>> tasks;
    mutex q_mutex;
    condition_variable cv;
    atomic_bool stop{false};
    atomic_int active{0};
    unique_ptr<SchedulingStrategy> strategy;
    ThreadPool* paired_pool = nullptr;
    bool is_vector_pool;
    int core_base;
    Experiment* experiment; // 添加Experiment指针

    void setAffinity(int core_id) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        //int core_id = base + (worker_index % 8); // 假设8线程池
        CPU_SET(core_id, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    }

public:
    ThreadPool(int cores, int base, bool is_vector, Experiment* exp);
    
    ~ThreadPool() { 
        stopPool(); 
    }

    void configure(unique_ptr<SchedulingStrategy> s, ThreadPool* p = nullptr) {
        strategy = move(s);
        paired_pool = p;
    }

    void submit(TaskType type, function<void()> task) {
        {
            lock_guard<mutex> lock(q_mutex);
            tasks.emplace(type, move(task));
        }
        cv.notify_one();
    }

    void stopPool() {
        {
            lock_guard<mutex> lock(q_mutex);
            stop = true;
        }
        cv.notify_all();
        for (auto& t : workers) {
            if (t.joinable()) t.join();
        }
    }

private:
    bool getTask(pair<TaskType, function<void()>>& task_pair) {
        lock_guard<mutex> lock(q_mutex);
        if(tasks.empty()) return false;
        task_pair = move(tasks.front());
        tasks.pop();
        return true;
    }
    bool trySteal(std::pair<TaskType, std::function<void()>>& task_pair);

};

ThreadPool::ThreadPool(int cores, int base, bool is_vector, Experiment* exp) 
    : core_base(base), is_vector_pool(is_vector), experiment(exp) {
        for(int i=0; i<cores; ++i) {
            workers.emplace_back([this, i] {
                setAffinity(core_base+i);
                while(!stop) {
                    pair<TaskType, function<void()>> task_pair;
                    if(getTask(task_pair) || this->trySteal(task_pair)) {
                        active++;
                        task_pair.second();
                        active--;
                    } else {
                        unique_lock<mutex> lock(q_mutex);
                        cv.wait_for(lock, chrono::milliseconds(100), [this]{ return !tasks.empty() || stop; });
                    }
                }
            });
        }
    }


class Experiment {
protected:
    unique_ptr<ThreadPool> scalar_pool;
    unique_ptr<ThreadPool> vector_pool;
    atomic<int> completed{0};
    int total_tasks;
    float ratio;
    long long fib_n;

    virtual void submitTask(TaskType type) {
        auto wrapper = [=] {
            switch(type) {
                case FIB: {
                    auto start = chrono::high_resolution_clock::now();
                    Utils::fib(fib_n); 
                    auto dur = chrono::duration<double, milli>(
                chrono::high_resolution_clock::now() - start);
                    updateStats(type, dur.count());
                    break;
                }
                case VEC_MATRIX: {
                    int32 A[N][N], B[N][N], C[N][N];
                    generateMatrix(A); generateMatrix(B);
                    auto start = chrono::high_resolution_clock::now();
                    Utils::vector_mult(A, B, C);
                    auto dur = chrono::duration<double, milli>(
                chrono::high_resolution_clock::now() - start);
                    updateStats(type, dur.count());
                    break;
                }
                case SCL_MATRIX: {
                    vector<vector<int>> A(N_SCL, vector<int>(N_SCL));
                    vector<vector<int>> B(N_SCL, vector<int>(N_SCL));
                    vector<vector<int>> C(N_SCL, vector<int>(N_SCL));
                    for (int i = 0; i < N_SCL; ++i) {
                        for (int j = 0; j < N_SCL; ++j) {
                            A[i][j] = Utils::dis(Utils::gen);
                            B[i][j] = Utils::dis(Utils::gen);
                        }
                    }
                    auto start = chrono::high_resolution_clock::now();
                    Utils::scalar_mult(A, B, C);
                    auto dur = chrono::duration<double, milli>(
                chrono::high_resolution_clock::now() - start);
                    updateStats(type, dur.count());
                    break;
                }
            }
            
            completed++;
        };

        if(type == FIB) scalar_pool->submit(FIB, wrapper);
        else vector_pool->submit(VEC_MATRIX, wrapper);
    }

    virtual void executeTask(TaskType type) {
        switch(type) {
            case FIB: Utils::fib(fib_n); break;
            case VEC_MATRIX: {
                int32 A[N][N], B[N][N], C[N][N];
                generateMatrix(A); generateMatrix(B);
                Utils::vector_mult(A, B, C);
                break;
            }
            case SCL_MATRIX: {
                vector<vector<int>> A(N_SCL, vector<int>(N_SCL));
                vector<vector<int>> B(N_SCL, vector<int>(N_SCL));
                vector<vector<int>> C(N_SCL, vector<int>(N_SCL));
                for (int i = 0; i < N_SCL; ++i) {
                for (int j = 0; j < N_SCL; ++j) {
                    A[i][j] = Utils::dis(Utils::gen);
                    B[i][j] = Utils::dis(Utils::gen);
                }
            }
                Utils::scalar_mult(A, B, C);
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
        lock_guard<mutex> lock(stats_mutex);
        switch(type) {
            case FIB: fib_time += time; break;
            case VEC_MATRIX: vec_mat_time += time; break;
            case SCL_MATRIX: scl_mat_time += time; break;
        }
    }

public:
    Experiment(int t, float r, long long f_n) : total_tasks(t), ratio(r), fib_n(f_n) {}
    virtual void run() = 0;
    void incrementCompleted() { completed++; }
    
    virtual ~Experiment() {
        scalar_pool->stopPool();
        vector_pool->stopPool();
    }
};


bool ThreadPool::trySteal(pair<TaskType, function<void()>>& task_pair) {
    if (!paired_pool || !strategy) return false;
    if (!strategy->canSteal(active, paired_pool->active)) return false;

    lock_guard<mutex> lock(paired_pool->q_mutex);
    if (paired_pool->tasks.empty()) return false;

    auto stolen_pair = move(paired_pool->tasks.front());
    TaskType stolen_type = stolen_pair.first;
    function<void()> stolen_func = move(stolen_pair.second);
    paired_pool->tasks.pop();

    // 处理任务偷取逻辑，修正使用正确的闭包
    if (!is_vector_pool && stolen_type == VEC_MATRIX) {
        task_pair = { SCL_MATRIX, [this] {
            vector<vector<int>> A(N_SCL, vector<int>(N_SCL)), B(N_SCL, vector<int>(N_SCL)), C(N_SCL, vector<int>(N_SCL));
            for (int i = 0; i < N_SCL; ++i) {
                for (int j = 0; j < N_SCL; ++j) {
                    A[i][j] = Utils::dis(Utils::gen);
                    B[i][j] = Utils::dis(Utils::gen);
                }
            }
            auto start = chrono::high_resolution_clock::now();
            Utils::scalar_mult(A, B, C);
            auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
            {
                lock_guard<mutex> lock(stats_mutex);
                scl_mat_time += dur.count();  // 统计标量矩阵乘法时间
            }
            sclmat_count += 1;
            experiment->incrementCompleted(); // 递增Experiment的completed
        }};
        return true;
    } else if (is_vector_pool && stolen_type == FIB) {
        task_pair = { FIB, [this, stolen_func] {
            auto start = chrono::high_resolution_clock::now();
            stolen_func(); // 直接执行原闭包，使用正确的fib_n
            auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
            {
                lock_guard<mutex> lock(stats_mutex);
                vec_fib_time += dur.count();  // 统计向量池执行斐波那契的时间
            }
        }};
        return true;
    } else if (!is_vector_pool && stolen_type == SCL_MATRIX) {
        task_pair = { SCL_MATRIX, [this] {
            vector<vector<int>> A(N_SCL, vector<int>(N_SCL)), B(N_SCL, vector<int>(N_SCL)), C(N_SCL, vector<int>(N_SCL));
            for (int i = 0; i < N_SCL; ++i) {
                for (int j = 0; j < N_SCL; ++j) {
                    A[i][j] = Utils::dis(Utils::gen);
                    B[i][j] = Utils::dis(Utils::gen);
                }
            }
            auto start = chrono::high_resolution_clock::now();
            Utils::scalar_mult(A, B, C);
            auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
            {
                lock_guard<mutex> lock(stats_mutex);
                scl_pool_stolen_scl += dur.count();
            }
            experiment->incrementCompleted();
        }};
        return true;
    
    }else {
        task_pair = move(stolen_pair);
        return true;
    }
}


class Exp1 : public Experiment {
public:
    Exp1(int t, float r, long long f_n) : Experiment(t, r, f_n) {
        scalar_pool.reset(new ThreadPool(1, 0, false, this));
        vector_pool.reset(new ThreadPool(1, 1, true, this));
        scalar_pool->configure(unique_ptr<SchedulingStrategy>(new StrictStrategy()));
        vector_pool->configure(unique_ptr<SchedulingStrategy>(new StrictStrategy()));
    }

    void run() override {
        for(int i=0; i<total_tasks; ++i) {
            (rand()%100 < ratio*100) ? submitTask(VEC_MATRIX) : submitTask(FIB);
        }
        while(completed < total_tasks) this_thread::sleep_for(10ms);
    }
};

class Exp2 : public Experiment {
public:
    Exp2(int t, float r, long long f_n) : Experiment(t, r, f_n) {
        scalar_pool.reset(new ThreadPool(1, 0, false, this));
        vector_pool.reset(new ThreadPool(1, 1, true, this));
        vector_pool->configure(unique_ptr<SchedulingStrategy>(new VectorStealStrategy()), scalar_pool.get());
    }

    void run() override {
        for (int i=0; i<total_tasks; ++i) {
            bool is_vec = (rand()%100 < ratio*100);
            is_vec ? submitTask(VEC_MATRIX) : submitTask(FIB);
        }
        while (completed < total_tasks) this_thread::sleep_for(10ms);
    }
};

class Exp3 : public Experiment {
public:
    Exp3(int t, float r, long long f_n) : Experiment(t, r, f_n) {
        scalar_pool.reset(new ThreadPool(1, 0, false, this));
        vector_pool.reset(new ThreadPool(1, 1, true, this));
        scalar_pool->configure(make_unique<BidirectionalStealStrategy>(), vector_pool.get());
        vector_pool->configure(make_unique<BidirectionalStealStrategy>(), scalar_pool.get());
    }

    void run() override {
        for (int i=0; i<total_tasks; ++i) {
            bool is_vec = (rand()%100 < ratio*100);
            if(is_vec) matrix_count += 1;
            is_vec ? submitTask(VEC_MATRIX) : submitTask(FIB);
        }
        while (completed < total_tasks) this_thread::sleep_for(10ms);
    }
};

class Exp4 : public Experiment {
public:
    Exp4(int t, float r, long long f_n) : Experiment(t, r, f_n) {
        scalar_pool.reset(new ThreadPool(1, 0, false, this));
        vector_pool.reset(new ThreadPool(1, 1, true, this));
        scalar_pool->configure(make_unique<BidirectionalStealStrategy>(), vector_pool.get());
        vector_pool->configure(make_unique<BidirectionalStealStrategy>(), scalar_pool.get());
    }

    void run() override {
        for (int i=0; i<total_tasks; ++i) {
            bool is_vec = (rand()%100 < ratio*100);
            is_vec ? submitTask(SCL_MATRIX) : submitTask(FIB);
        }
        while (completed < total_tasks) this_thread::sleep_for(10ms);
    }
    
private:
    void submitTask(TaskType type) override {
        if (type == FIB) {
            // 提交斐波那契任务到标量池，统计到 fib_time
            scalar_pool->submit(FIB, [=] {
                auto start = chrono::high_resolution_clock::now();
                Utils::fib(fib_n);
                auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
                {
                    lock_guard<mutex> lock(stats_mutex);
                    fib_time += dur.count();
                }
                completed++;
            });
        } else if (type == SCL_MATRIX) {
            // 提交标量矩阵乘法到向量池，统计到 vec_pool_native_scl
            vector_pool->submit(SCL_MATRIX, [=] {
                vector<vector<int>> A(N_SCL, vector<int>(N_SCL)), B(N_SCL, vector<int>(N_SCL)), C(N_SCL, vector<int>(N_SCL));
                for (int i = 0; i < N_SCL; ++i) {
                    for (int j = 0; j < N_SCL; ++j) {
                        A[i][j] = Utils::dis(Utils::gen);
                        B[i][j] = Utils::dis(Utils::gen);
                    }
                }
                auto start = chrono::high_resolution_clock::now();
                Utils::scalar_mult(A, B, C);
                auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
                {
                    lock_guard<mutex> lock(stats_mutex);
                    vec_pool_native_scl += dur.count();
                }
                completed++;
            });
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " <total_tasks> <ratio> <fib_n>\n"
             << "Example: " << argv[0] << " 1000 0.8 40960" << endl;
        return 1;
    }

    char* end;
    long total_tasks = strtol(argv[1], &end, 10);
    if (*end != '\0' || total_tasks <= 0) {
        cerr << "Error: total_tasks must be a positive integer" << endl;
        return 1;
    }

    double ratio = strtod(argv[2], &end);
    if (*end != '\0' || ratio < 0 || ratio > 1) {
        cerr << "Error: ratio must be between 0.0 and 1.0" << endl;
        return 1;
    }

    long long fib_n = strtoll(argv[3], &end, 10);
    if (*end != '\0' || fib_n <= 0) {
        cerr << "Error: fib_n must be a positive integer" << endl;
        return 1;
    }

    cout << fixed << setprecision(2);
    // 实验1：严格隔离
    {
        auto start = chrono::high_resolution_clock::now();
        Exp1(total_tasks, ratio, fib_n).run();
        auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
        
        cout << "=== Exp1 Strict Isolation ===" << endl
             << "[Fibonacci]" << endl
             << "  Scalar native: " << fib_time << "ms" << endl
             << "[Matrix]" << endl
             << "  Vector native: " << vec_mat_time << "ms" << endl
             << "Total latency: " << dur.count() << "ms" << endl;
    }

    // 重置统计信息
    {
        lock_guard<mutex> lock(stats_mutex);
        fib_time = vec_fib_time = vec_mat_time = scl_mat_time = 0;
    }

    // 实验2：向量池偷取标量任务
    {
        auto start = chrono::high_resolution_clock::now();
        Exp2(total_tasks, ratio, fib_n).run();
        auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
        
        cout << "\n=== Exp2 Vector Stealing ===" << endl
             << "[Fibonacci]" << endl
             << "  Scalar native: " << fib_time << "ms" << endl
             << "  Vector stolen: " << vec_fib_time << "ms" << endl
             << "  Total: " << (fib_time + vec_fib_time) << "ms" << endl
             << "[Matrix]" << endl
             << "  Vector native: " << vec_mat_time << "ms" << endl
             << "Total latency: " << dur.count() << "ms" << endl;
    }

    // 重置统计信息
    {
        lock_guard<mutex> lock(stats_mutex);
        fib_time = vec_fib_time = vec_mat_time = scl_mat_time = 0;
    }

    // 实验3：双向偷取
    {
        auto start = chrono::high_resolution_clock::now();
        Exp3(total_tasks, ratio, fib_n).run();
        auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
        
        cout << "\n=== Exp3 Bidirectional Stealing ===" << endl
             << "[Fibonacci]" << endl
             << "  Scalar native: " << fib_time << "ms" << endl
             << "  Vector stolen: " << vec_fib_time << "ms" << endl
             << "  Total: " << (fib_time + vec_fib_time) << "ms" << endl
             << "[Matrix]" << endl
             << "  Vector native: " << vec_mat_time << "ms" << endl
             << "  Scalar stolen: " << scl_mat_time << "ms" << endl
             << "  Total: " << (vec_mat_time + scl_mat_time) << "ms" << endl
             << "Total latency: " << dur.count() << "ms" << endl
             << "scl mat ratio:"<< sclmat_count / matrix_count << endl
             << "vec mat ratio:"<< 1- sclmat_count / matrix_count << endl;
    }
    
    {
        lock_guard<mutex> lock(stats_mutex);
        fib_time = vec_pool_native_scl = scl_pool_stolen_scl = 0;
    }

    {
        auto start = chrono::high_resolution_clock::now();
        Exp4(total_tasks, ratio, fib_n).run();
        auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
    
        cout << "\n=== Exp4 Bidirectional Scalar Stealing ===" << endl
             << "[Scalar Matrix Multiply]" << endl
             << "  Vector Pool Native: " << vec_pool_native_scl << "ms" << endl
             << "  Scalar Pool Stolen: " << scl_pool_stolen_scl << "ms" << endl
             << "[Fibonacci]" << endl
             << "  Scalar Native: " << fib_time << "ms" << endl
             << "  Vector Stolen: " << vec_fib_time << "ms" << endl
             << "Total latency: " << dur.count() << "ms" << endl;
    }
    return 0;
}

