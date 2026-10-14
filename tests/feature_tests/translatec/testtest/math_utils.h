#ifndef MATH_UTILS_H
#define MATH_UTILS_H
typedef struct {
    int x;
    int y;
} Point;
int add(int a, int b);
int factorial(int n);
void move_point(Point *p, int dx, int dy);
#endif
