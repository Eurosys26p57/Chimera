#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

__attribute__((optimize("align-functions=65536")))
unsigned long long f0(unsigned long long n){
	return ~n;
}

__attribute__((optimize("align-functions=65536")))
long long f1(unsigned long long n){
	return n == 0 || n == 1 ? n : (f1(n-1) + f1(n-2));
}

__attribute__((optimize("align-functions=65536")))
unsigned long long f2(unsigned long long n){
	unsigned long long f = 0, s = 1;
	if (n <= 1)
		return n;
	for (int c = 1; c < n; c++){
		unsigned long long t = f + s;
		f = s;
		s = t;
	}
	return s;
}

__attribute__((optimize("align-functions=65536")))
unsigned long long f3(unsigned long long n){
	return n;
}

__attribute__((optimize("align-functions=65536")))
int main(int argc, char * argv[]) {
	for (int i = 1 ; i < argc ; i++){
		unsigned long long x = strtoull(argv[i], NULL, 0);
		if (x < ULLONG_MAX)
			printf("%llu\t%p\t%p\n", f2(x), f2, &x);
	}
	scanf("\n");
	return 0;
}
