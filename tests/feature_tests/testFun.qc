class MyClass<T> {
    MyClass(T x) {}
    void operator( )(int x) {
        `qout("%i", x);
    }
    void operator( )<K>(K x) {
        return;
    }
    void operator++() {
    }
    void operator+=(MyClass<T> other) {
    }
}
int main() {
    MyClass<int> x = MyClass<int>(123);
    x(12);
    x<int>(21);
    MyClass<int> y;
    x++;
    x += y;
    return 0;
}
