#include <iostream>
#include <thread>

// 线程函数
void print_message(const std::string& message) {
    std::cout << message << std::endl;
}

int main() {
    // 创建线程
    std::thread thread1(print_message, "Thread 1");
    std::thread thread2(print_message, "Thread 2");

    // 等待线程结束
    thread1.join();
    thread2.join();

    std::cout << "Main thread exiting." << std::endl;
    return 0;
}