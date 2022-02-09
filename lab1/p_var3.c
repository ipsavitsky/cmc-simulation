#include <stdio.h>
#define SIZE 10000
double a[SIZE][SIZE];
int i;
double b[SIZE][SIZE];
double c[SIZE][SIZE];
void main(int argc, char **argv) {
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
}
