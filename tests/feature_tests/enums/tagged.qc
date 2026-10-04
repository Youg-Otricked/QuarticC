enum Option<T> {
    Some(T);
    None;
}
enum Result<T, E> {
    Ok(T);
    Err(E);
}
int main() {
    Option<int> a = Option<int>.Some(42);
    Option<int> b = Option<int>.None;
    Result<int, int> success = Result<int, int>.Ok(123);
    Result<int, int> failure = Result<int, int>.Err(-1);
    `qout("Enums as ints: do they work? %i, %i, %i, %i, %i", a, b, success, failure, a + b + success + failure);
    match (a) {
        Option<int>.Some(value) => `qout("Extracted value: %i\n", value);
        Option<int>.None => `qout("Empty\n");
    }
    match (success) {
        default => `qout("Got a tag\n");
    }
    return 0;
}
