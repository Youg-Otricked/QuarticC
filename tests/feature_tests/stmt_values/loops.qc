T unreachableValue<T>() {unreachable;}
int main() {
    int x = 0;
    x = for (int i = 0; i < 5; i++) returning int {
        break 123;
    }(unreachableValue<int>());
    x = for (;false;) returning int {
        break;
    }(0);
    x = while (true) returning int {
        break 123;
    }(unreachableValue<int>());
    x = while (true) returning int { break; }(12);
    int[5] y = [1, 2, 3, 4, 5];
    x = foreach (int i in y) returning int {
        break i;
    }(0);
    return x - 1;
}

