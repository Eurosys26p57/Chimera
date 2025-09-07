#include <stdio.h>

// 计算斐波那契数列的第 n 项
int fibonacci(int n) {
    if (n <= 1) return n;
    int a = 0, b = 1, c;
    for (int i = 2; i <= n; i++) {
        c = a + b;
        a = b;
        b = c;
    }
    return b;
}

int main() {
    int n = 10; // 计算第 10 项
    printf("Fibonacci(%d) = %d\n", n, fibonacci(n));
    return 0;
}