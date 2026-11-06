type Error = double | int;
struct Err {
    int ok;
    string msg;
}
type Error2 = double | int | Err;
Err custom_error() {
    return Err{1, "hello"};
}
Error error_union() {
    return 123;
}
Error2 returns_errors(bool test) {
    if (test) try custom_error();
    try error_union();
    return 0.0;
}
int main() {
    Error2 e = returns_errors(false);
    `qout("Error 1: %i\n", e as int);
    e = returns_errors(true);
    `qout("Error 2: %s\n", (e as Err).msg);
    double result = returns_errors(false) catch (e) {
        double res = 0.0;
        match (e) {
            int(i) => res = `to_double(i);
            default => return -1;
        }
        res;
    };
    `qout("Collected result: %d\n", result);
    return 0;
}
