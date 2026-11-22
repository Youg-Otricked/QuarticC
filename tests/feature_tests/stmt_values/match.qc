T unreachableValue<T>() {unreachable;}
int main() {
    int x = match (26) returning int {
        default => 1;
    };
    return x - 1;
}


