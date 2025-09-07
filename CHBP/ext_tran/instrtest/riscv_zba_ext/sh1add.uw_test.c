#include<stdio.h>
#include<stdint.h>
#include<assert.h>

typedef struct {
    uint64_t b;
    uint64_t c;
} test_case_t;

//define micro to execute asm
int main(){
    test_case_t test_cases[] = {
        {1, 2}, 
        {0, 0}, 
        {0x7FFFFFFF, 1}, 
        {-1, 1}, 
        {0xFFFFFFFE, 0} 
    };

    int num_cases = sizeof(test_cases) / sizeof(test_case_t);
        
    for (int i = 0; i < num_cases; i++) {
        uint64_t b = test_cases[i].b;
        uint64_t c = test_cases[i].c;

        uint64_t a0;
        uint64_t b0 = b;
        uint64_t c0 = c;
        uint64_t d0;
        __asm__ __volatile__("zext.w %3, %1\n\t"
                                "slli %3, %3, 1\n\t"
                                "add %0, %3, %2\n\t"
                                :"=r" (a0), "=r"(b0), "=r"(c0), "=r"(d0)
                                :"1" (b0), "2" (c0)
                                );
        uint64_t a1;
        uint64_t b1 = b;
        uint64_t c1 = c;
        __asm__ __volatile__("sh1add.uw %0, %1, %2\n\t"
                                :"=r" (a1)
                                :"r" (b1), "r" (c1)
                                );
        printf("Test case %d: b = %llu, c = %llu\n", i + 1, b, c);
        printf("Result of zext.w + slli + add: %llu\n", a0);
        printf("Result of sh1add.uw: %llu\n", a1);
        assert(a0 == a1);
        printf("Check passed for test case %d.\n\n", i + 1);
    }
    return 0;
}
