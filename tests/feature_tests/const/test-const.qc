int main() {
    const int x = 0;
    x = 123;
    int y = 0;
    const int *p = &x;
    *p = 23; 
    p = &y;
    int *const constp = &y;
    *constp = 123; 
    constp = &x;
    const int **const nonsense = &p;
    *nonsense = constp;
    nonsense = &constp; 
    **nonsense = 2;
}
/// & const int = const int*
/// & int = int*
/// & const int* = const int**
/// & int *const = int *const *
