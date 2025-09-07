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
#include <utility>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace std;
constexpr int N = 1000;
constexpr int BATCH_SIZE = 4096;
using int32 = int;

enum TaskType { FIB, HAMMING_B, HAMMING_LOOP };
double fib_time = 0, hamming_b_time = 0, hamming_loop_time = 0;
double vec_fib_time = 0;    // 跨核执行斐波那契时间
mutex stats_mutex;
class Experiment;

namespace Utils {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<uint64_t> dis(0, UINT64_MAX);

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

    // 批量生成随机数
    vector<uint64_t> generate_batch(size_t n) {
        vector<uint64_t> batch(n);
        for(auto& v : batch) v = dis(gen);
        return batch;
    }

    // 批量循环计算（保持原始计算逻辑）
    uint64_t hamming_loop_batch(const vector<uint64_t>& data) {
        uint64_t total = 0;
        for(auto x : data) {
            while(x) {
                total += x & 1;
                x >>= 1;
            }
        }
        return total;
    }

    // 批量B扩展计算
    uint64_t hamming_b_batch(const vector<uint64_t>& data) {
        uint64_t total = 0;
        for(auto x : data) {
            int count;
            __asm__ volatile (
                "cpop %0, %1"
                : "=r" (count)
                : "r" (x)
            );
            total += count;
        }
        return total;
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

class BExtStealStrategy : public SchedulingStrategy {
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
    bool is_bext_pool;
    int core_base;
    Experiment* experiment;

    void setAffinity(int base) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for(int i=0; i<4; ++i) CPU_SET(base+i, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    }

public:
    ThreadPool(int cores, int base, bool is_bext, Experiment* exp);
    
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
    bool trySteal(pair<TaskType, function<void()>>& task_pair);
};

ThreadPool::ThreadPool(int cores, int base, bool is_bext, Experiment* exp) 
    : core_base(base), is_bext_pool(is_bext), experiment(exp) {
        for(int i=0; i<cores; ++i) {
            workers.emplace_back([this] {
                setAffinity(core_base);
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
    unique_ptr<ThreadPool> bext_pool;
    atomic<int> completed{0};
    int total_tasks;
    float ratio;
    long long fib_n;

    virtual void submitTask(TaskType type) {
        auto wrapper = [=] {
            auto start = chrono::high_resolution_clock::now();
            executeTask(type);
            auto dur = chrono::duration<double, milli>(
                chrono::high_resolution_clock::now() - start);
            updateStats(type, dur.count());
            completed++;
        };

        if(type == FIB) scalar_pool->submit(FIB, wrapper);
        else bext_pool->submit(HAMMING_B, wrapper);
    }

    virtual void executeTask(TaskType type) {
        auto batch = Utils::generate_batch(BATCH_SIZE);
        uint32_t test_value = Utils::dis(Utils::gen);
        switch(type) {
            case FIB: Utils::fib(fib_n); break;
            case HAMMING_B: Utils::hamming_b_batch(batch); break;
            case HAMMING_LOOP: Utils::hamming_loop_batch(batch); break;
        }
    }

    void updateStats(TaskType type, double time) {
        lock_guard<mutex> lock(stats_mutex);
        switch(type) {
            case FIB: fib_time += time; break;
            case HAMMING_B: hamming_b_time += time; break;
            case HAMMING_LOOP: hamming_loop_time += time; break;
        }
    }

public:
    Experiment(int t, float r, long long f_n) : total_tasks(t), ratio(r), fib_n(f_n) {}
    virtual void run() = 0;
    void incrementCompleted() { completed++; }
    
    virtual ~Experiment() {
        scalar_pool->stopPool();
        bext_pool->stopPool();
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

    // 任务转换逻辑
    if (!is_bext_pool && stolen_type == HAMMING_B) {
        task_pair = { HAMMING_LOOP, [this] {
            auto start = chrono::high_resolution_clock::now();
            auto batch = Utils::generate_batch(BATCH_SIZE);
            uint32_t test_value = Utils::dis(Utils::gen);
            Utils::hamming_loop_batch(batch);
            auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
            {
                lock_guard<mutex> lock(stats_mutex);
                hamming_loop_time += dur.count();
            }
            experiment->incrementCompleted();
        }};
        return true;
    } else if (is_bext_pool && stolen_type == FIB) {
        task_pair = { FIB, [this, stolen_func] {
            auto start = chrono::high_resolution_clock::now();
            stolen_func();
            auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
            {
                lock_guard<mutex> lock(stats_mutex);
                vec_fib_time += dur.count();
            }
        }};
        return true;
    } else {
        task_pair = move(stolen_pair);
        return true;
    }
}

class Exp1 : public Experiment {
public:
    Exp1(int t, float r, long long f_n) : Experiment(t, r, f_n) {
        scalar_pool.reset(new ThreadPool(4, 0, false, this));
        bext_pool.reset(new ThreadPool(4, 4, true, this));
        scalar_pool->configure(unique_ptr<SchedulingStrategy>(new StrictStrategy()));
        bext_pool->configure(unique_ptr<SchedulingStrategy>(new StrictStrategy()));
    }

    void run() override {
        for(int i=0; i<total_tasks; ++i) {
            (rand()%100 < ratio*100) ? submitTask(HAMMING_B) : submitTask(FIB);
        }
        while(completed < total_tasks) this_thread::sleep_for(chrono::milliseconds(100));
    }
};

class Exp2 : public Experiment {
public:
    Exp2(int t, float r, long long f_n) : Experiment(t, r, f_n) {
        scalar_pool.reset(new ThreadPool(4, 0, false, this));
        bext_pool.reset(new ThreadPool(4, 4, true, this));
        bext_pool->configure(unique_ptr<SchedulingStrategy>(new BExtStealStrategy()), scalar_pool.get());
    }

    void run() override {
        for (int i=0; i<total_tasks; ++i) {
            bool is_bext = (rand()%100 < ratio*100);
            is_bext ? submitTask(HAMMING_B) : submitTask(FIB);
        }
        while (completed < total_tasks) this_thread::sleep_for(10ms);
    }
};

class Exp3 : public Experiment {
public:
    Exp3(int t, float r, long long f_n) : Experiment(t, r, f_n) {
        scalar_pool.reset(new ThreadPool(4, 0, false, this));
        bext_pool.reset(new ThreadPool(4, 4, true, this));
        scalar_pool->configure(make_unique<BidirectionalStealStrategy>(), bext_pool.get());
        bext_pool->configure(make_unique<BidirectionalStealStrategy>(), scalar_pool.get());
    }

    void run() override {
        for (int i=0; i<total_tasks; ++i) {
            bool is_bext = (rand()%100 < ratio*100);
            is_bext ? submitTask(HAMMING_B) : submitTask(FIB);
        }
        while (completed < total_tasks) this_thread::sleep_for(10ms);
    }
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " <total_tasks> <ratio> <fib_n>\n"
             << "Example: " << argv[0] << " 1000 0.8 40" << endl;
        return 1;
    }

    int total_tasks = atoi(argv[1]);
    float ratio = atof(argv[2]);
    long long fib_n = atoll(argv[3]);

    // 实验1：严格隔离
    {
        auto start = chrono::high_resolution_clock::now();
        Exp1(total_tasks, ratio, fib_n).run();
        auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
        
        cout << "=== Exp1 Strict Isolation ===" << endl
             << "[Fibonacci]" << endl
             << "  Scalar native: " << fib_time << "ms" << endl
             << "[Hamming Weight]" << endl
             << "  B-ext native: " << hamming_b_time << "ms" << endl
             << "Total latency: " << dur.count() << "ms" << endl;
    }

    // 重置统计
    {
        lock_guard<mutex> lock(stats_mutex);
        fib_time = vec_fib_time = hamming_b_time = hamming_loop_time = 0;
    }

    // 实验2：B扩展核窃取
    {
        auto start = chrono::high_resolution_clock::now();
        Exp2(total_tasks, ratio, fib_n).run();
        auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
        
        cout << "\n=== Exp2 B-ext Stealing ===" << endl
             << "[Fibonacci]" << endl
             << "  Scalar native: " << fib_time << "ms" << endl
             << "  B-ext stolen: " << vec_fib_time << "ms" << endl
             << "  Total: " << (fib_time + vec_fib_time) << "ms" << endl
             << "[Hamming Weight]" << endl
             << "  B-ext native: " << hamming_b_time << "ms" << endl
             << "Total latency: " << dur.count() << "ms" << endl;
    }

    // 重置统计
    {
        lock_guard<mutex> lock(stats_mutex);
        fib_time = vec_fib_time = hamming_b_time = hamming_loop_time = 0;
    }

    // 实验3：双向窃取
    {
        auto start = chrono::high_resolution_clock::now();
        Exp3(total_tasks, ratio, fib_n).run();
        auto dur = chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start);
        
        cout << "\n=== Exp3 Bidirectional Stealing ===" << endl
             << "[Fibonacci]" << endl
             << "  Scalar native: " << fib_time << "ms" << endl
             << "  B-ext stolen: " << vec_fib_time << "ms" << endl
             << "  Total: " << (fib_time + vec_fib_time) << "ms" << endl
             << "[Hamming Weight]" << endl
             << "  B-ext native: " << hamming_b_time << "ms" << endl
             << "  Scalar stolen: " << hamming_loop_time << "ms" << endl
             << "  Total: " << (hamming_b_time + hamming_loop_time) << "ms" << endl
             << "Total latency: " << dur.count() << "ms" << endl;
    }

    return 0;
}