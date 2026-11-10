namespace Exported {
    #include <IO, std>
}
type Union1 = int | string;
struct Big {
    Union1 un;
    int idx;
    string data;
}
type Union2 = Big | Union1;
type GU<T> = T | int;
int main() {
    Union1 un1 = 123;
    Big b = Big{un1, 1223, "hello"};
    Union2 u = b;
    match (un1) {
        int(i) => IO::qout("It's an int. %i\n", i);
        default => IO::qout("No\n");
    }
    IO::qout("%i %i", un1 as int, (u as Big).idx);
    GU<double> g = 123;
    g = 0.023;
    panic "crash";
}
