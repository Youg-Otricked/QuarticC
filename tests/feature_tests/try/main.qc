namespace Exported {
    #include <IO, std>
}
type Error = double | int;
struct Err {
    int ok;
    string msg;
}
type Error2 = float | int | Err;
Err custom_error() {
    return Err{1, "hello"};
}
Error error_union() {
    return 123;
}
Error2 returns_errors(bool test) {
    if (test) try custom_error();
    try error_union();
    return 0.0f;
}
int main() {
    Error2 e = returns_errors(false);
    IO::qout("Error 1: %i\n", e as int);
    e = returns_errors(true);
    IO::qout("Error 2: %s\n", (e as Err).msg);
    float result = returns_errors(false) catch (e) {
        float res = 0.0f;
        match (e) {
            int(i) => res = `to_float(i);
            default => return -1;
        }
        res;
    };
    IO::qout("Collected result: %f\n", result);
    return 0;
}
