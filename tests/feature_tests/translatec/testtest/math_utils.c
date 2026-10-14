#include "math_utils.h"
int add(int a, int b) {
    return a + b;
}
int factorial(int n) {
    if (n < 0) {
        return -1;
    }
    int result = 1;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    return result;
}
void move_point(Point *p, int dx, int dy) {
    if (p == 0) {
        return;
    }
    p->x += dx;
    p->y += dy;
}
