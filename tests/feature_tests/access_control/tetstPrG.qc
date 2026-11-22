class MyClass<T> {
    private int field;
    public int get() {
        return this->field;
    }
    public int access(MyClass<T> other) {
        return other.field;
    }
    MyClass(int i) { this->field = i; }
}
int main() {
    MyClass<int> thing = MyClass<int>(123);
    MyClass<int> other = MyClass<int>(0);
    int x = thing.get();
    int ty = thing.access(other);
    return ty;
}
