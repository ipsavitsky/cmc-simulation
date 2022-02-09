#include <stdio.h>
#define SIZE 10000
double a[SIZE][SIZE];
int i;
double b[SIZE][SIZE];
double c[SIZE][SIZE];

#define rdtsc(var)                                      \
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi)); \
    var = ((long)hi << 32) | lo

void main(int argc, char **argv) {
    unsigned long tick, tick2;
    unsigned int lo, hi;
    rdtsc(tick);
    int j, k;
    for (i = 0; i < SIZE; i++) {
        for (j = 0; j < SIZE; j++) {
            b[i][j] = 20;
            c[i][j] = 19;
        }
    }
    for (j = 0; j < SIZE; j++) {
        for (i = 0; i < SIZE; i++) {
            a[i][j] = b[i][j] + c[i][j];
        }
    }
    rdtsc(tick2);
    printf("time = %ld\n", tick2 - tick);
}
