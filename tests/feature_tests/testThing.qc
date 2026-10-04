int main() {
    int *x = nullptr;
    if (x && *x == 42) {
        // unreachable
    }
    return 0;
}
