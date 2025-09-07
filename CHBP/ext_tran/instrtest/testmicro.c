#include<stdio.h>
#include<stdint.h>
#include<assert.h>

//define micro to execute asm
int main(){
        uint64_t a = 1;
        uint64_t b = 2;

        uint64_t a0 = a;
        uint64_t b0 = b;
        uint64_t c0;
         __asm__ __volatile__("zext.w %2, %1\n\t"
                                "add %0, %0, %2\n\t"
                                :"=r" (a0), "=r"(b0), "=r" (c0)
                                :"1" (b0), "0" (a0)
                                );
        uint64_t a1 = a;
        uint64_t b1 = b;
         __asm__ __volatile__("add.uw %0, %0, %1\n\t"
                                :"=r" (a1)
                                :"r" (b1), "0" (a1)
                                );
        printf("%lld,%lld\n",a0,a1);
        assert(a0==a1);
        printf("Check passed.\n");
        return 0;

}
