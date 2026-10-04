struct MyStruct {
    int x;
    bool y;
}
int main() {
    int[123] a = [int, 123];
    char[] b = [char, 21];
    MyStruct c = MyStruct{};
    MyStruct d = MyStruct{123, true};
    MyStruct e = MyStruct{y: false, x: 123};
    return 0;
}
