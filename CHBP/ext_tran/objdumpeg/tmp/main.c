#include <stdio.h>

// // 1. 循环+分支，ext在if里
// int main(){
//     int a, even = 0, odd = 0;
//     for(a = 64; a >= 0; a--){
//         if(a % 2){
//             even++;
//             __asm__("fcvt.s.l ft0,a0");
//         }
//         else{
//             odd++;
//             //__asm__("fcvt.d.s ft0,ft0");
//         }
//     }
//     printf("even=%d odd=%d\n", even, odd);
//     return 0;
// }

// // 2. 循环+分支，ext在else里
// int main(){
//     int a, even = 0, odd = 0;
//     for(a = 64; a >= 0; a--){
//         if(a % 2){
//             even++;
//             //__asm__("fcvt.s.l ft0,a0");
//         }
//         else{
//             odd++;
//             __asm__("fcvt.d.s ft0,ft0");
//         }
//     }
//     printf("even=%d odd=%d\n", even, odd);
//     return 0;
// }

// // 3. 循环+分支，ext在else if里
// int main(){
//     int a, even = 0, odd = 0;
//     for(a = 64; a >= 0; a--){
//         if(a % 2){
//             even++;
//             //__asm__("fcvt.s.l ft0,a0");
//         }
//         else if(a % 3){
//             odd++;
//             __asm__("fcvt.d.s ft0,ft0");
//         }
//         else{
//             even--;
//         }
//     }
//     printf("even=%d odd=%d\n", even, odd);
//     return 0;
// }

// // 4. 循环+分支，ext在if前
// int main(){
//     int a, even = 0, odd = 0;
//     for(a = 64; a >= 0; a--){
//         __asm__("fcvt.d.s ft0,ft0");
//         if(a % 2){
//             even++;
//             //__asm__("fcvt.s.l ft0,a0");
//         }
//         else{
//             odd++;
//         }
//     }
//     printf("even=%d odd=%d\n", even, odd);
//     return 0;
// }

// // 5. 循环+分支，ext在else后
// int main(){
//     int a, even = 0, odd = 0;
//     for(a = 64; a >= 0; a--){
//         if(a % 2){
//             even++;
//             //__asm__("fcvt.s.l ft0,a0");
//         }
//         else{
//             odd++;
//         }
//         __asm__("fcvt.d.s ft0,ft0");
//     }
//     printf("even=%d odd=%d\n", even, odd);
//     return 0;
// }

// 6. 双重循环，ext在外循环
int main(){
    int a, even = 0, odd = 0;
    for(a = 64; a >= 0; a--){
        __asm__("fcvt.d.s ft0,ft0");
        for(int b = a; b < 64; b++){
            odd++;
        }
    }
    printf("even=%d odd=%d\n", even, odd);
    return 0;
}