struct None {
}
concept Empty {
}
None proves Empty;
int main() {
    comptime if (!Empty proved_by None) {
        `compile_error("Empty must be proved by None");
        return 1;
    } else {
        return 0;
    }
}

