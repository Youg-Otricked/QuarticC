class Counter {
    static int order_sum = 0;
    static int side_effect_count = 0;

    static void add_order(int val) {
        Counter::order_sum = Counter::order_sum * 10 + val;
    }
}
int test_lifo_order() {
    Counter::order_sum = 0;
    if (true) {
    defer Counter::add_order(3);
    defer Counter::add_order(2);
    defer Counter::add_order(1);
    }
    if (Counter::order_sum != 123) return 1;
    return 0;
}
int test_defer_on_return() {
    Counter::order_sum = 0;
    defer Counter::add_order(2);
    defer Counter::add_order(1);
    
    return Counter::order_sum;
}
int test_defer_in_loops() {
    Counter::order_sum = 0;
    
    int i = 0;
    while (i < 3) {
        i = i + 1;
        defer Counter::add_order(i);
        if (i == 2) {
            continue;
        }
    }
    if (Counter::order_sum != 123) return 3;
    Counter::order_sum = 0;
    int j = 0;
    while (j < 10) {
        j = j + 1;
        defer Counter::add_order(j);
        if (j == 2) {
            break;
        }
    }
    if (Counter::order_sum != 12) return 4;
    return 0;
}
int test_nested_scopes() {
    Counter::order_sum = 0;
    defer Counter::add_order(4);
    if (true) {
        defer Counter::add_order(2);
        defer Counter::add_order(1);
    }
    defer Counter::add_order(3);
    return 0;
}
class TestRunner {
    static int run_all_tests() {
        if (test_lifo_order() != 0) return 101;
        test_defer_on_return();
        if (Counter::order_sum != 12) return 102;
        if (test_defer_in_loops() != 0) return 103;
        if (test_nested_scopes() != 0) return 104;
        return 0;
    }
}
int main() {
    int res = TestRunner::run_all_tests();
    if (res == 0) {
        `qout("All defer tests passed");
    } else {
        `qout("%i", res);
    }
    return res;
}
