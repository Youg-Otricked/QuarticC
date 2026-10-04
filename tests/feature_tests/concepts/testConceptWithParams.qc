concept Pointer {
    all_of(Proving value) {
        *value;
    }
}
struct Empty {
}
Empty* proves Pointer;
int main() {}
