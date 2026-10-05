int add(int a, int b) {
    return a + b;
}
int map(int a, int b, fn(int a, int b) -> int f) {
    return f(a, b);
}
int main() {
    return map(123, 1, &add);
}
