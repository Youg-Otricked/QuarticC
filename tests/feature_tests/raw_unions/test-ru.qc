union RawUnion {
    int i;
    float f;
}
union Other<T, T2> {
    T t1;
    T2 t2;
}
int main() {
    RawUnion un;
    un.i = 123;
    `qout("%i\n", un.i);
    un.f = 23.023f;
    `qout("%f\n", un.f);
    Other<int, string> s;
    s.t2 = "hello";
    s.t1 = 123;
    return 0;
}
