#include<stdio.h>
#include<stdint.h>
#include<assert.h>

typedef struct {
    uint64_t b;
} test_case_t;

//define micro to execute asm
int main(){
    test_case_t test_cases[] = {
        {1}, 
        {0}, 
        {0x7FFFFFFF}, 
        {-1}, 
        {0xFFFFFFFE} 
    };

    int num_cases = sizeof(test_cases) / sizeof(test_case_t);
        
    for (int i = 0; i < num_cases; i++) {
        uint64_t b = test_cases[i].b;

        uint64_t a0;
        uint64_t b0 = b;
        uint64_t d0;
        __asm__ __volatile__("zext.w %2, %1\n\t"
                                "slli %0, %2, %3\n\t"
                                :"=r" (a0), "=r"(b0), "=r"(d0)
                                :"I"(5), "1" (b0)
                                );
        uint64_t a1;
        uint64_t b1 = b;
        __asm__ __volatile__("slli.uw %0, %1, %2\n\t"
                                :"=r" (a1)
                                :"r" (b1), "I" (5)
                                );
        printf("Test case %d: b = %llu, c = 5\n", i + 1, b);
        printf("Result of zext.w+slli: %llu\n", a0);
        printf("Result of slli.uw: %llu\n", a1);
        assert(a0 == a1);
        printf("Check passed for test case %d.\n\n", i + 1);
    }
    return 0;
}
